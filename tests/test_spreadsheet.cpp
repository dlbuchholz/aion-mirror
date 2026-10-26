#include <QtTest>

#include "spreadsheet.h"
#include "tests/test_spreadsheet_files.h"

using Spreadsheet::Format;
using Rows = QList<QStringList>;

namespace {

QByteArray zip(const QList<QPair<QString, QByteArray>>& files) {
    return zipFiles(files);
}

const QByteArray PackageRels =
    R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
    R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>)"
    R"(</Relationships>)";

QByteArray workbook(const QByteArray& sheets, const QByteArray& properties = "<workbookPr/>") {
    return R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
           R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
           + properties + "<sheets>" + sheets + "</sheets></workbook>";
}

QByteArray worksheet(const QByteArray& rows) {
    return R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>)" + rows
           + "</sheetData></worksheet>";
}

QByteArray ods(const QByteArray& tables, const QByteArray& manifest = QByteArray()) {
    const QByteArray content =
        R"(<office:document-content xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
        R"(xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" )"
        R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0">)"
        "<office:body><office:spreadsheet>" + tables + "</office:spreadsheet></office:body></office:document-content>";
    QList<QPair<QString, QByteArray>> files = {{"mimetype", "application/vnd.oasis.opendocument.spreadsheet"},
                                               {"content.xml", content}};
    if (!manifest.isEmpty()) files.append({"META-INF/manifest.xml", manifest});
    return zip(files);
}

} // namespace

class TestSpreadsheet : public QObject {
    Q_OBJECT

private slots:
    void knowsSpreadsheetsByTheirName() {
        QCOMPARE(Spreadsheet::formatOf("C:/Downloads/Umsaetze.XLSX"), Format::Xlsx);
        QCOMPARE(Spreadsheet::formatOf("konto.ods"), Format::Ods);
        QCOMPARE(Spreadsheet::formatOf("konto.xls"), Format::None);
        QCOMPARE(Spreadsheet::formatOf("konto.csv"), Format::None);
    }

    void readsExcelSheetsWithTheirValues() {
        const QByteArray styles =
            "<styleSheet><numFmts>"
            R"(<numFmt numFmtId="164" formatCode="dd/mm/yyyy;@"/>)"
            R"(<numFmt numFmtId="165" formatCode="&quot;Day &quot;0"/>)"            // d in quotes: not a date
            R"(<numFmt numFmtId="166" formatCode="#,##0.00\ [$€-407]"/>)"           // Currency: not a date
            "</numFmts>"
            R"(<cellStyleXfs count="1"><xf numFmtId="14"/></cellStyleXfs>)"        // Not cell styles
            R"(<cellXfs count="6"><xf numFmtId="0"/><xf numFmtId="14"/><xf numFmtId="164"/>)"
            R"(<xf numFmtId="20"/><xf numFmtId="165"/><xf numFmtId="166"/></cellXfs></styleSheet>)";
        const QByteArray strings =
            "<sst><si><t>Buchungstag</t></si><si><t>Empfänger</t></si><si><t>Betrag</t></si>"
            "<si><r><t xml:space=\"preserve\">REWE </t></r><r><rPr><b/></rPr><t>Markt</t></r></si>"
            "<si><t>Lohn</t><rPh><t>ignored</t></rPh></si></sst>";
        const QByteArray januar = worksheet(
            R"(<row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1" t="s"><v>1</v></c><c r="C1" t="s"><v>2</v></c></row>)"
            R"(<row r="2"><c r="A2" s="1"><v>46296</v></c><c r="B2" t="s"><v>3</v></c>)"
            R"(<c r="C2"><v>-32.399999999999999</v></c><c r="E2" t="inlineStr"><is><t>inline</t></is></c></row>)"
            R"(<row r="3"><c r="A3" s="0"/></row>)"
            R"(<row r="4"><c r="A4" s="2"><v>46297.75</v></c><c r="B4" t="str"><f>A1</f><v>Lohn</v></c>)"
            R"(<c r="C4"><v>2450</v></c><c r="D4" t="b"><v>1</v></c><c r="E4" s="3"><v>0.5</v></c>)"
            R"(<c r="F4" s="4"><v>7</v></c><c r="G4" s="5"><v>1234.5</v></c><c r="H4" t="e"><v>#DIV/0!</v></c>)"
            R"(<c r="I4" s="0"/></row>)");
        const QByteArray februar = worksheet(
            R"(<row r="1"><c r="A1" t="s"><v>0</v></c></row>)"
            R"(<row r="2"><c r="A2" s="1"><v>46327</v></c><c r="B2" t="s"><v>4</v></c></row>)");
        const QByteArray workbookRels =
            R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
            R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
            R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="/xl/worksheets/sheet2.xml"/>)"
            R"(<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/>)"
            R"(<Relationship Id="rId4" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>)"
            R"(</Relationships>)";
        const QByteArray file = zip({
            {"_rels/.rels", PackageRels},
            {"xl/workbook.xml", workbook(R"(<sheet name="Januar" sheetId="1" r:id="rId1"/><sheet name="Februar" sheetId="2" r:id="rId2"/>)")},
            {"xl/_rels/workbook.xml.rels", workbookRels},
            {"xl/sharedStrings.xml", strings},
            {"xl/styles.xml", styles},
            {"xl/worksheets/sheet1.xml", januar},
            {"xl/worksheets/sheet2.xml", februar},
        });

        QString error;
        const QList<Spreadsheet::Sheet> sheets = Spreadsheet::read(file, Format::Xlsx, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(sheets.size(), 2);
        QCOMPARE(sheets[0].name, QString("Januar"));
        QCOMPARE(sheets[0].rows, (Rows{
            {"Buchungstag", "Empfänger", "Betrag"},
            {"2026-10-01", "REWE Markt", "-32.4", "", "inline"},                 // Gaps stay; the empty row 3 is left out
            {"2026-10-02", "Lohn", "2450", "TRUE", "0.5", "7", "1234.5", "#DIV/0!"},   // Times alone aren't dates
        }));
        QCOMPARE(sheets[0].rowNumbers, (QList<int>{1, 2, 4}));                      // As in Excel
        QCOMPARE(sheets[1].name, QString("Februar"));
        QCOMPARE(sheets[1].rows, (Rows{{"Buchungstag"}, {"2026-11-01", "Lohn"}}));
        QCOMPARE(sheets[1].rowNumbers, (QList<int>{1, 2}));
    }

    void excelDatesCanCountFrom1904() {
        const QByteArray workbookRels =
            R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
            R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
            R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>)"
            R"(</Relationships>)";
        const QByteArray file = zip({
            {"_rels/.rels", PackageRels},
            {"xl/workbook.xml", workbook(R"(<sheet name="Mac" sheetId="1" r:id="rId1"/>)", R"(<workbookPr date1904="1"/>)")},
            {"xl/_rels/workbook.xml.rels", workbookRels},
            {"xl/styles.xml", R"(<styleSheet><cellXfs><xf numFmtId="0"/><xf numFmtId="14"/></cellXfs></styleSheet>)"},
            {"xl/worksheets/sheet1.xml", worksheet(R"(<row r="1"><c r="A1" s="1"><v>1</v></c></row>)")},
        });
        QString error;
        const QList<Spreadsheet::Sheet> sheets = Spreadsheet::read(file, Format::Xlsx, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(sheets.size(), 1);
        QCOMPARE(sheets.first().rows, (Rows{{"1904-01-02"}}));
    }

    void readsLibreOfficeSheets() {
        const QByteArray file = ods(
            R"(<table:table table:name="Konto"><table:table-column/>)"
            R"(<table:table-row><table:table-cell office:value-type="string"><text:p>Datum</text:p></table:table-cell>)"
            R"(<table:table-cell office:value-type="string"><text:p>Empfänger</text:p></table:table-cell>)"
            R"(<table:table-cell office:value-type="string"><text:p>Betrag</text:p></table:table-cell>)"
            R"(<table:table-cell table:number-columns-repeated="1021"/></table:table-row>)"
            R"(<table:table-row><table:table-cell office:value-type="date" office:date-value="2026-10-01"><text:p>01.10.26</text:p></table:table-cell>)"
            R"(<table:table-cell office:value-type="string"><text:p>REWE<text:s text:c="2"/>Markt</text:p>)"
            R"(<office:annotation><text:p>A note</text:p></office:annotation></table:table-cell>)"
            R"(<table:table-cell office:value-type="currency" office:currency="EUR" office:value="-32.4"><text:p>-32,40 €</text:p></table:table-cell></table:table-row>)"
            R"(<table:table-row table:number-rows-repeated="2"><table:table-cell office:value-type="date" office:date-value="2026-10-02T10:30:00"/>)"
            R"(<table:table-cell table:number-columns-repeated="2"/><table:table-cell office:value-type="float" office:value="5"/></table:table-row>)"
            R"(<table:table-row table:number-rows-repeated="1048570"><table:table-cell table:number-columns-repeated="1024"/></table:table-row>)"
            R"(</table:table>)"
            R"(<table:table table:name="Leer"><table:table-row><table:table-cell/></table:table-row></table:table>)"
            R"(<table:table table:name="Zwei"><table:table-row><table:table-cell office:value-type="boolean" office:boolean-value="true"/>)"
            R"(<table:covered-table-cell/><table:table-cell office:value-type="string"><text:p>a</text:p><text:p>b</text:p></table:table-cell>)"
            R"(</table:table-row></table:table>)");

        QString error;
        const QList<Spreadsheet::Sheet> sheets = Spreadsheet::read(file, Format::Ods, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(sheets.size(), 3);
        QCOMPARE(sheets[0].name, QString("Konto"));
        QCOMPARE(sheets[0].rows, (Rows{
            {"Datum", "Empfänger", "Betrag"},                                       // The 1021 empty columns don't count
            {"2026-10-01", "REWE  Markt", "-32.4"},                                 // Values, not what the cells show
            {"2026-10-02", "", "", "5"},
            {"2026-10-02", "", "", "5"},                                            // Repeated; the empty filler rows aren't
        }));
        QCOMPARE(sheets[0].rowNumbers, (QList<int>{1, 2, 3, 4}));
        QVERIFY(sheets[1].rows.isEmpty());
        QCOMPARE(sheets[2].rows, (Rows{{"TRUE", "", "a\nb"}}));
        QCOMPARE(sheets[2].rowNumbers, (QList<int>{1}));
    }

    void explainsWhatCantBeRead() {
        QString error;
        QVERIFY(Spreadsheet::read("Datum;Betrag\n", Format::Xlsx, &error).isEmpty());
        QCOMPARE(error, QString("This file isn't a valid Excel file, or it's damaged."));

        QVERIFY(Spreadsheet::read(QByteArray("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1 rest of an old file"), Format::Xlsx, &error).isEmpty());
        QVERIFY(error.startsWith("This is an old Excel file (.xls) or protected by a password."));

        QVERIFY(Spreadsheet::read(zip({{"something.txt", "hello"}}), Format::Xlsx, &error).isEmpty());
        QCOMPARE(error, QString("This file isn't an Excel workbook."));

        const QByteArray encrypted = ods("<table:table/>", "<manifest:manifest><manifest:encryption-data/></manifest:manifest>");
        QVERIFY(Spreadsheet::read(encrypted, Format::Ods, &error).isEmpty());
        QVERIFY(error.contains("password"));

        QVERIFY(Spreadsheet::read(ods("<table:table table:name=\"Broken\"><table:table-row>"), Format::Ods, &error).isEmpty());
        QVERIFY(error.startsWith("The file is damaged"));
    }
};

QTEST_MAIN(TestSpreadsheet)
#include "test_spreadsheet.moc"
