#include "aboutwidget.h"
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
    while (QLayoutItem* item = infoLayout->takeAt(0)) {
        if (item->layout()) {
            while (QLayoutItem* child = item->layout()->takeAt(0)) {
                delete child->widget();
                delete child;
            }
        }
        delete item->widget();
        delete item;
    }

    const Theme::Colors colors = Theme::colors();
    QString currentSection;
    QFormLayout* form = nullptr;
    for (const SystemInfo::Entry& entry : SystemInfo::entries()) {
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
