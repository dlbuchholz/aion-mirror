#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include <QStringList>
#include <QTextStream>

// Command-line options: --version and --help
namespace CommandLine {

// Handles --version and --help by writing to out. Returns the exit code when Aion should exit
// right away, or -1 to start normally. Options Aion doesn't know (Qt's own, like -style) don't
// stop it from starting.
int handle(const QStringList& arguments, QTextStream& out);

// Aion is a window program, so on Windows its output goes nowhere when it's started from a
// command prompt. This connects it to that prompt, unless the output is already redirected.
void attachToParentConsole();

} // namespace CommandLine

#endif // COMMANDLINE_H
