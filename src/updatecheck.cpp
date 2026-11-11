#include "updatecheck.h"
#include "version.h"

#include <QCoreApplication>
#include <QMessageBox>

namespace UpdateCheck {

void checkInteractively(QWidget* parent) {
    // TODO: query the release feed and compare against AION_VERSION_MAJOR/MINOR/PATCH
    QMessageBox::information(parent, QCoreApplication::translate("UpdateCheck", "Check for updates"),
                             QCoreApplication::translate("UpdateCheck", "You're using Aion %1.\n\n"
                                                                        "Checking for updates isn't available yet.").arg(AION_VERSION));
}

} // namespace UpdateCheck
