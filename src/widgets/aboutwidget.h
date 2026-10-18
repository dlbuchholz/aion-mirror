#ifndef ABOUTWIDGET_H
#define ABOUTWIDGET_H

#include <QWidget>

class QLabel;
class QVBoxLayout;

// Settings → About: version, build and environment details, copy-to-clipboard report
class AboutWidget : public QWidget {
    Q_OBJECT

public:
    explicit AboutWidget(QWidget* parent = nullptr);

    void refresh();                                  // Re-read the information

protected:
    void showEvent(QShowEvent* event) override;

private:
    void copyReport();

    QVBoxLayout* infoLayout = nullptr;
    QLabel* copiedLabel = nullptr;
};

#endif // ABOUTWIDGET_H
