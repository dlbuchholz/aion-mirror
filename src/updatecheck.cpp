#include "updatecheck.h"
#include "appname.h"
#include "version.h"

#include <QCoreApplication>
#include <QMessageBox>

namespace UpdateCheck {

void checkInteractively(QWidget* parent) {
    // TODO: query the release feed and compare against APP_VERSION_MAJOR/MINOR/PATCH
    QMessageBox::information(parent, QCoreApplication::translate("UpdateCheck", "Check for updates"),
                             QCoreApplication::translate("UpdateCheck", "You're using %2 %1.\n\n"
                                                                        "Checking for updates isn't available yet.").arg(APP_VERSION, APP_NAME));
}

} // namespace UpdateCheck
