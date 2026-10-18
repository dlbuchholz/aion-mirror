#include "updatecheck.h"
#include "version.h"

#include <QMessageBox>

namespace UpdateCheck {

void checkInteractively(QWidget* parent) {
    // TODO: query the release feed and compare against AION_VERSION_MAJOR/MINOR/PATCH
    QMessageBox::information(parent, "Check for updates",
                             QString("You're using Aion %1.\n\n"
                                     "Checking for updates isn't available yet.").arg(AION_VERSION));
}

} // namespace UpdateCheck
