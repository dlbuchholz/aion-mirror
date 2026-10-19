#include "aboutwidget.h"
#include "icons.h"
#include "systeminfo.h"
#include "theme.h"
#include "updatecheck.h"

#include <QClipboard>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

AboutWidget::AboutWidget(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* heading = new QLabel("About", this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(11);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    // Logo with name, version and license
    auto* identity = new QHBoxLayout();
    identity->setSpacing(16);
    auto* logo = new QLabel(this);
    logo->setObjectName("logoLabel");
    logo->setPixmap(Icons::logo(64, devicePixelRatioF()));
    logo->setAccessibleName("Aion logo");
    identity->addWidget(logo, 0, Qt::AlignTop);
    auto* identityText = new QVBoxLayout();
    identityText->setSpacing(2);
    auto* name = new QLabel("Aion", this);
    QFont nameFont = name->font();
    nameFont.setPointSize(14);
    name->setFont(nameFont);
    identityText->addWidget(name);
    auto* version = new QLabel(QString("Version %1").arg(SystemInfo::versionText()), this);
    version->setObjectName("aboutVersionLabel");
    identityText->addWidget(version);
    auto* license = new QLabel("Free software under the GNU GPL, version 3 or later. "
                               "The name and logo belong to the Aion contributors.", this);
    license->setObjectName("aboutLicenseLabel");
    license->setWordWrap(true);
    license->setForegroundRole(QPalette::PlaceholderText);
    identityText->addWidget(license);
    identity->addLayout(identityText, 1);
    layout->addLayout(identity);

    infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(14);
    layout->addLayout(infoLayout);

    auto* buttons = new QHBoxLayout();
    auto* copyButton = new QPushButton("Copy technical information", this);
    copyButton->setObjectName("copyInfoButton");
    copyButton->setToolTip("Copies the details above as text, for example for a bug report. "
                           "Your user folder is shown as ~.");
    connect(copyButton, &QPushButton::clicked, this, &AboutWidget::copyReport);
    buttons->addWidget(copyButton);
    auto* updateButton = new QPushButton("Check for updates", this);
    connect(updateButton, &QPushButton::clicked, this, [this]() { UpdateCheck::checkInteractively(this); });
    buttons->addWidget(updateButton);
    copiedLabel = new QLabel("Copied", this);
    copiedLabel->hide();
    buttons->addWidget(copiedLabel);
    buttons->addStretch();
    layout->addLayout(buttons);
    layout->addStretch();

    refresh();
}

void AboutWidget::refresh() {
    // The rows stay the same, only values like the database size change. Rebuilding all the
    // labels on every show made the page slow to open.
    const QList<SystemInfo::Entry> entries = SystemInfo::entries();
    if (valueLabels.size() == entries.size()) {
        for (qsizetype i = 0; i < entries.size(); ++i) {
            valueLabels[i]->setText(entries[i].value);
        }
        return;
    }

    const Theme::Colors colors = Theme::colors();
    QString currentSection;
    QFormLayout* form = nullptr;
    for (const SystemInfo::Entry& entry : entries) {
        if (entry.section != currentSection) {
            currentSection = entry.section;
            auto* sectionLabel = new QLabel(currentSection, this);
            QFont sectionFont = sectionLabel->font();
            sectionFont.setWeight(QFont::DemiBold);
            sectionLabel->setFont(sectionFont);
            QPalette sectionPalette = sectionLabel->palette();
            sectionPalette.setColor(QPalette::WindowText, colors.textSecondary);
            sectionLabel->setPalette(sectionPalette);
            infoLayout->addWidget(sectionLabel);

            form = new QFormLayout();
            form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
            form->setVerticalSpacing(6);
            form->setContentsMargins(0, 0, 0, 0);
            infoLayout->addLayout(form);
        }
        auto* value = new QLabel(entry.value, this);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(entry.label, value);
        valueLabels.append(value);
    }
}

void AboutWidget::copyReport() {
    QGuiApplication::clipboard()->setText(SystemInfo::report());
    copiedLabel->show();
    QTimer::singleShot(2000, copiedLabel, &QLabel::hide);
}

void AboutWidget::showEvent(QShowEvent* event) {
    refresh();
    QWidget::showEvent(event);
}
