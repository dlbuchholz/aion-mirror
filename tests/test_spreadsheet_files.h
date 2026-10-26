#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <miniz.h>

// Spreadsheet files for tests, made in memory

// A zip archive with these files, the way spreadsheets are packed
inline QByteArray zipFiles(const QList<QPair<QString, QByteArray>>& files) {
    mz_zip_archive archive{};
    if (!mz_zip_writer_init_heap(&archive, 0, 0)) return {};
    for (const auto& [name, contents] : files) {
        mz_zip_writer_add_mem(&archive, name.toUtf8().constData(), contents.constData(), size_t(contents.size()),
                              mz_uint(MZ_DEFAULT_LEVEL));
    }
    void* buffer = nullptr;
    size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&archive, &buffer, &size);
    QByteArray result(static_cast<const char*>(buffer), qsizetype(size));
    mz_zip_writer_end(&archive);                     // Frees the buffer too
    return result;
}

// A plain .xlsx workbook: numbers as numbers, everything else as text
inline QByteArray xlsxFile(const QList<QPair<QString, QList<QStringList>>>& sheets) {
    static const QRegularExpression number(R"(^-?\d+(\.\d+)?$)");
    QByteArray sheetList;
    QByteArray rels = R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)";
    QList<QPair<QString, QByteArray>> files;
    for (int i = 0; i < sheets.size(); ++i) {
        const QByteArray id = QByteArray::number(i + 1);
        sheetList += "<sheet name=\"" + sheets[i].first.toHtmlEscaped().toUtf8() + "\" sheetId=\"" + id + "\" r:id=\"rId" + id + "\"/>";
        rels += R"(<Relationship Id="rId)" + id
                + R"(" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet)"
                + id + R"(.xml"/>)";
        QByteArray rows;
        for (const QStringList& row : sheets[i].second) {
            rows += "<row>";
            for (const QString& value : row) {
                rows += number.match(value).hasMatch()
                    ? "<c><v>" + value.toUtf8() + "</v></c>"
                    : "<c t=\"inlineStr\"><is><t>" + value.toHtmlEscaped().toUtf8() + "</t></is></c>";
            }
            rows += "</row>";
        }
        files.append({"xl/worksheets/sheet" + QString::number(i + 1) + ".xml",
                      R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>)" + rows
                          + "</sheetData></worksheet>"});
    }
    rels += "</Relationships>";
    files.append({QString("_rels/.rels"),
                  QByteArray(R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
                             R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>)"
                             "</Relationships>")});
    files.append({QString("xl/workbook.xml"),
                  R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
                  R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>)"
                      + sheetList + "</sheets></workbook>"});
    files.append({QString("xl/_rels/workbook.xml.rels"), rels});
    return zipFiles(files);
}
