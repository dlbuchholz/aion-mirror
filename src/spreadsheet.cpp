#include "spreadsheet.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <cmath>
#include <optional>

#include <miniz.h>

namespace Spreadsheet {

namespace {

constexpr mz_uint64 MaxFileSize = 256 * 1024 * 1024;   // Unpacked, per file in the archive (zip bombs)
constexpr int MaxRepeats = 1000;                        // Rows or cells repeated more often are filler

const QString OfficeNs = QStringLiteral("urn:oasis:names:tc:opendocument:xmlns:office:1.0");
const QString TableNs = QStringLiteral("urn:oasis:names:tc:opendocument:xmlns:table:1.0");
const QString TextNs = QStringLiteral("urn:oasis:names:tc:opendocument:xmlns:text:1.0");

// The files of a zip archive in memory
class Zip {
public:
    explicit Zip(const QByteArray& bytes) : data(bytes) {
        open = mz_zip_reader_init_mem(&zip, data.constData(), size_t(data.size()), 0);
    }
    ~Zip() {
        if (open) mz_zip_reader_end(&zip);
    }
    Zip(const Zip&) = delete;
    Zip& operator=(const Zip&) = delete;

    bool isOpen() const { return open; }

    // Empty if the file isn't there or is too large
    std::optional<QByteArray> file(const QString& name) {
        const int index = mz_zip_reader_locate_file(&zip, name.toUtf8().constData(), nullptr, 0);
        mz_zip_archive_file_stat stat;
        if (index < 0 || !mz_zip_reader_file_stat(&zip, mz_uint(index), &stat) || stat.m_uncomp_size > MaxFileSize) {
            return std::nullopt;
        }
        size_t size = 0;
        void* bytes = mz_zip_reader_extract_to_heap(&zip, mz_uint(index), &size, 0);
        if (!bytes) {
            return stat.m_uncomp_size == 0 ? std::optional<QByteArray>(QByteArray()) : std::nullopt;
        }
        QByteArray contents(static_cast<const char*>(bytes), qsizetype(size));
        mz_free(bytes);
        return contents;
    }

private:
    QByteArray data;                                 // miniz reads it in place
    mz_zip_archive zip{};
    bool open = false;
};

QString numberText(double value) {
    // Up to 10 decimals, without the float noise beyond them ("32.4", not "32.399999999999999")
    QString text = QString::number(value, 'f', 10);
    if (text.contains('.')) {
        while (text.endsWith('0')) text.chop(1);
        if (text.endsWith('.')) text.chop(1);
    }
    return text == "-0" ? QString("0") : text;
}

QString dateText(const QDate& date) {
    return date.toString("yyyy-MM-dd");
}

// Rows end at their last value; rows without any are left out. number is the row's in the sheet.
void addRow(Sheet& sheet, QStringList row, int number, int times = 1) {
    while (!row.isEmpty() && row.last().trimmed().isEmpty()) row.removeLast();
    if (row.isEmpty()) return;
    for (int i = 0; i < qMin(times, MaxRepeats); ++i) {
        sheet.rows.append(row);
        sheet.rowNumbers.append(number + i);
    }
}

// --- Excel (.xlsx) ---

// A relationships file: id -> target, and type -> target, targets resolved against baseDir
struct Relationships {
    QHash<QString, QString> byId;
    QHash<QString, QString> byType;                 // The last part of the type URL ("styles", ...)
};

Relationships readRelationships(const QByteArray& xmlData, const QString& baseDir) {
    Relationships result;
    QXmlStreamReader xml(xmlData);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != u"Relationship") continue;
        const QString target = xml.attributes().value("Target").toString();
        const QString path = target.startsWith('/') ? target.mid(1)
                                                    : QDir::cleanPath(baseDir.isEmpty() ? target : baseDir + "/" + target);
        result.byId.insert(xml.attributes().value("Id").toString(), path);
        result.byType.insert(xml.attributes().value("Type").toString().section('/', -1), path);
    }
    return result;
}

QStringList readSharedStrings(const QByteArray& xmlData) {
    QStringList strings;
    QXmlStreamReader xml(xmlData);
    QString current;
    int phonetic = 0;                                // <rPh> holds reading aids, not the text
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == u"si") current.clear();
            else if (xml.name() == u"rPh") ++phonetic;
            else if (xml.name() == u"t" && phonetic == 0) current += xml.readElementText();
        } else if (xml.isEndElement()) {
            if (xml.name() == u"si") strings.append(current);
            else if (xml.name() == u"rPh") --phonetic;
        }
    }
    return strings;
}

bool isDateFormat(int id, QString code) {
    // Excel's built-in date formats (times alone, 18-21 and 45-47, aren't dates)
    if ((id >= 14 && id <= 17) || id == 22 || (id >= 27 && id <= 36) || (id >= 50 && id <= 58)) {
        return true;
    }
    if (code.isEmpty()) return false;
    static const QRegularExpression quoted(R"("[^"]*")");
    static const QRegularExpression escaped(R"(\\.)");
    static const QRegularExpression bracketed(R"(\[[^\]]*\])");   // Colours, conditions, locales
    code.remove(quoted).remove(escaped).remove(bracketed);
    code = code.toLower();
    // m alone could be minutes ("h:mm")
    return code.contains('d') || code.contains('y') || (code.contains('m') && !code.contains('h') && !code.contains('s'));
}

// Per cell style (the s attribute of a cell): whether its numbers are dates
QList<bool> readDateStyles(const QByteArray& xmlData) {
    QHash<int, QString> customFormats;
    QList<bool> dateStyles;
    QXmlStreamReader xml(xmlData);
    bool inCellXfs = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == u"numFmt") {
                customFormats.insert(xml.attributes().value("numFmtId").toInt(), xml.attributes().value("formatCode").toString());
            } else if (xml.name() == u"cellXfs") {
                inCellXfs = true;
            } else if (xml.name() == u"xf" && inCellXfs) {
                const int id = xml.attributes().value("numFmtId").toInt();
                dateStyles.append(isDateFormat(id, customFormats.value(id)));
            }
        } else if (xml.isEndElement() && xml.name() == u"cellXfs") {
            inCellXfs = false;
        }
    }
    return dateStyles;
}

int columnIndex(QStringView reference) {
    // "BC12" -> 54
    int column = 0;
    for (QChar c : reference) {
        if (!c.isLetter()) break;
        column = column * 26 + (c.toUpper().unicode() - 'A' + 1);
    }
    return column - 1;
}

void readXlsxSheet(Sheet& sheet, const QByteArray& xmlData, const QStringList& strings, const QList<bool>& dateStyles,
                   const QDate& epoch, QString* error) {
    QStringList row;
    int rowNumber = 0;
    QXmlStreamReader xml(xmlData);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == u"row") {
            row.clear();
            const int number = xml.attributes().value("r").toInt();
            rowNumber = number > 0 ? number : rowNumber + 1;
        } else if (xml.isEndElement() && xml.name() == u"row") {
            addRow(sheet, row, rowNumber);
        } else if (xml.isStartElement() && xml.name() == u"c") {
            const QXmlStreamAttributes attributes = xml.attributes();
            const QString reference = attributes.value("r").toString();
            const int column = reference.isEmpty() ? int(row.size()) : columnIndex(reference);
            const QString type = attributes.value("t").toString();
            const int style = attributes.value("s").toInt();
            QString value;
            QString inlineText;
            while (!xml.atEnd() && !(xml.isEndElement() && xml.name() == u"c")) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == u"v") value = xml.readElementText();
                else if (xml.isStartElement() && xml.name() == u"t") inlineText += xml.readElementText();
            }

            QString text;
            if (type == "s") {
                text = strings.value(value.toInt());
            } else if (type == "inlineStr") {
                text = inlineText;
            } else if (type == "str" || type == "e") {
                text = value;                        // A formula's text, or an error like #DIV/0!
            } else if (type == "b") {
                text = value == "1" ? "TRUE" : "FALSE";
            } else if (type == "d") {
                text = value.left(10);               // ISO date
            } else if (!value.isEmpty()) {
                bool ok = false;
                const double number = value.toDouble(&ok);
                if (!ok) {
                    text = value;
                } else if (dateStyles.value(style)) {
                    text = dateText(epoch.addDays(qint64(std::floor(number))));
                } else {
                    text = numberText(number);
                }
            }
            if (column >= 0 && column < 16384) {     // Excel's last column is XFD
                while (row.size() <= column) row.append(QString());
                row[column] = text;
            }
        }
    }
    if (xml.hasError()) {
        *error = QString("The file is damaged: %1").arg(xml.errorString());
    }
}

QList<Sheet> readXlsx(Zip& zip, QString* error) {
    // The workbook: from the package's relationships, usually xl/workbook.xml
    QString workbookPath = "xl/workbook.xml";
    if (const auto packageRels = zip.file("_rels/.rels")) {
        workbookPath = readRelationships(*packageRels, "").byType.value("officeDocument", workbookPath);
    }
    const auto workbook = zip.file(workbookPath);
    if (!workbook) {
        *error = "This file isn't an Excel workbook.";
        return {};
    }
    const QString baseDir = QFileInfo(workbookPath).path();
    const Relationships rels = readRelationships(
        zip.file(baseDir + "/_rels/" + QFileInfo(workbookPath).fileName() + ".rels").value_or(QByteArray()), baseDir);
    const QStringList strings = readSharedStrings(zip.file(rels.byType.value("sharedStrings")).value_or(QByteArray()));
    const QList<bool> dateStyles = readDateStyles(zip.file(rels.byType.value("styles")).value_or(QByteArray()));

    // Sheet names and files, and the date system (1900, or 1904 from old Macs)
    QList<QPair<QString, QString>> sheetFiles;
    QDate epoch(1899, 12, 30);                       // Day 1 is 1 January 1900 (Excel counts a 29 February 1900)
    QXmlStreamReader xml(*workbook);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        if (xml.name() == u"workbookPr") {
            const auto date1904 = xml.attributes().value("date1904");
            if (date1904 == u"1" || date1904 == u"true") epoch = QDate(1904, 1, 1);
        } else if (xml.name() == u"sheet") {
            QString relationshipId;
            for (const QXmlStreamAttribute& attribute : xml.attributes()) {
                if (attribute.name() == u"id") relationshipId = attribute.value().toString();   // r:id
            }
            sheetFiles.append({xml.attributes().value("name").toString(), rels.byId.value(relationshipId)});
        }
    }

    QList<Sheet> sheets;
    for (const auto& [name, path] : std::as_const(sheetFiles)) {
        const auto sheetData = zip.file(path);
        if (!sheetData) continue;                    // Chart sheets and the like have no cells
        Sheet sheet{name, {}, {}};
        readXlsxSheet(sheet, *sheetData, strings, dateStyles, epoch, error);
        if (!error->isEmpty()) return {};
        sheets.append(sheet);
    }
    if (sheets.isEmpty()) {
        *error = "This workbook has no sheets with cells.";
    }
    return sheets;
}

// --- LibreOffice (.ods) ---

// The text of a cell: its paragraphs, with ODF's spaces, tabs and line breaks
QString odsCellText(QXmlStreamReader& xml) {
    QString text;
    int paragraphs = 0;
    int depth = 0;                                   // Inside a paragraph
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == u"annotation") {       // A comment on the cell
                xml.skipCurrentElement();
            } else if (xml.name() == u"p" && xml.namespaceUri() == TextNs) {
                if (paragraphs++ > 0) text += '\n';
                ++depth;
            } else if (xml.name() == u"s") {
                text += QString(qMax(1, xml.attributes().value(TextNs, "c").toInt()), ' ');
            } else if (xml.name() == u"tab") {
                text += '\t';
            } else if (xml.name() == u"line-break") {
                text += '\n';
            }
        } else if (xml.isEndElement()) {
            if (xml.name() == u"p" && xml.namespaceUri() == TextNs) --depth;
            else if (xml.name() == u"table-cell" || xml.name() == u"covered-table-cell") break;
        } else if (xml.isCharacters() && depth > 0) {
            text += xml.text();
        }
    }
    return text;
}

QList<Sheet> readOds(Zip& zip, QString* error) {
    const auto manifest = zip.file("META-INF/manifest.xml");
    if (manifest && manifest->contains("encryption-data")) {
        *error = "This spreadsheet is protected by a password. Save it without one and try again.";
        return {};
    }
    const auto content = zip.file("content.xml");
    if (!content) {
        *error = "This file isn't a LibreOffice spreadsheet.";
        return {};
    }

    QList<Sheet> sheets;
    QStringList row;
    int rowNumber = 0;                               // Of the row's first repeat
    int rowRepeats = 1;
    int pendingEmpty = 0;                            // Empty cells, added only if a value follows
    QXmlStreamReader xml(*content);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.namespaceUri() == TableNs) {
            const QXmlStreamAttributes attributes = xml.attributes();
            if (xml.name() == u"table") {
                sheets.append(Sheet{attributes.value(TableNs, "name").toString(), {}, {}});
                rowNumber = 0;
                rowRepeats = 1;
            } else if (xml.name() == u"table-row") {
                row.clear();
                pendingEmpty = 0;
                rowNumber += rowRepeats;                 // After the rows (and repeats) before
                rowRepeats = qMax(1, attributes.value(TableNs, "number-rows-repeated").toInt());
            } else if (xml.name() == u"table-cell" || xml.name() == u"covered-table-cell") {
                const int repeats = qMax(1, attributes.value(TableNs, "number-columns-repeated").toInt());
                const QString type = attributes.value(OfficeNs, "value-type").toString();
                const QString number = attributes.value(OfficeNs, "value").toString();
                const QString date = attributes.value(OfficeNs, "date-value").toString();
                const QString boolean = attributes.value(OfficeNs, "boolean-value").toString();
                QString text = odsCellText(xml);     // Reads to the cell's end
                if (type == "float" || type == "currency" || type == "percentage") {
                    text = numberText(number.toDouble());
                } else if (type == "date") {
                    text = date.left(10);
                } else if (type == "boolean") {
                    text = boolean == "true" ? "TRUE" : "FALSE";
                }
                if (text.isEmpty()) {
                    pendingEmpty += repeats;
                } else if (row.size() + pendingEmpty < 16384) {
                    for (; pendingEmpty > 0; --pendingEmpty) row.append(QString());
                    for (int i = 0; i < qMin(repeats, MaxRepeats); ++i) row.append(text);
                }
            }
        } else if (xml.isEndElement() && xml.name() == u"table-row" && xml.namespaceUri() == TableNs && !sheets.isEmpty()) {
            addRow(sheets.last(), row, rowNumber, rowRepeats);
        }
    }
    if (xml.hasError()) {
        *error = QString("The file is damaged: %1").arg(xml.errorString());
        return {};
    }
    if (sheets.isEmpty()) {
        *error = "This spreadsheet has no sheets.";
    }
    return sheets;
}

} // namespace

Format formatOf(const QString& path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "xlsx" || suffix == "xlsm") return Format::Xlsx;
    if (suffix == "ods") return Format::Ods;
    return Format::None;
}

QString formatName(Format format) {
    switch (format) {
    case Format::Xlsx: return "Excel";
    case Format::Ods: return "LibreOffice";
    case Format::None: break;
    }
    return QString();
}

QList<Sheet> read(const QByteArray& data, Format format, QString* error) {
    error->clear();
    if (data.startsWith("\xD0\xCF\x11\xE0")) {
        // The old binary format, which password-protected .xlsx files use too
        *error = "This is an old Excel file (.xls) or protected by a password. Save it as .xlsx without a "
                 "password and try again.";
        return {};
    }
    Zip zip(data);
    if (!zip.isOpen()) {
        *error = QString("This file isn't a valid %1 file, or it's damaged.").arg(formatName(format));
        return {};
    }
    switch (format) {
    case Format::Xlsx: return readXlsx(zip, error);
    case Format::Ods: return readOds(zip, error);
    case Format::None: break;
    }
    *error = "This file isn't a spreadsheet.";
    return {};
}

QList<Sheet> readFile(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QString("The file couldn't be opened: %1").arg(file.errorString());
        return {};
    }
    return read(file.readAll(), formatOf(path), error);
}

} // namespace Spreadsheet
