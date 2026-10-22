#include "commandline.h"
#include "systeminfo.h"

#include <QCommandLineParser>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace CommandLine {

int handle(const QStringList& arguments, QTextStream& out) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Aion, a personal finance app");
    const QCommandLineOption help = parser.addHelpOption();
    const QCommandLineOption version = parser.addVersionOption();

    // Print the options ourselves: QCommandLineParser would show a message box on Windows
    if (!parser.parse(arguments)) {
        return -1;                                   // Unknown options are Qt's or a typo; start anyway
    }
    if (parser.isSet(help) || parser.isSet("help-all")) {   // addHelpOption() also adds --help-all
        out << parser.helpText();
        out.flush();
        return 0;
    }
    if (parser.isSet(version)) {
        out << "Aion " << SystemInfo::versionText() << '\n';
        out.flush();
        return 0;
    }
    return -1;
}

void attachToParentConsole() {
#ifdef Q_OS_WIN
    if (GetStdHandle(STD_OUTPUT_HANDLE) != nullptr) {
        return;                                      // Redirected to a file or pipe already
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
    }
#endif
}

} // namespace CommandLine
