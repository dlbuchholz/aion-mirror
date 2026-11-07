#ifndef EMPTYSTATE_H
#define EMPTYSTATE_H

#include <QLabel>
#include <QString>

// What a list or page says while it has nothing to show: a muted icon above muted, centred text,
// the same everywhere. It's a label, so its text is set and read like any other.
class EmptyState : public QLabel {
    Q_OBJECT

public:
    static constexpr int IconSize = 32;
    static constexpr int Gap = 10;                   // Between the icon and the text

    explicit EmptyState(const QString& icon, const QString& text = QString(), QWidget* parent = nullptr);

    QString iconName() const { return icon; }
    QRect iconRect() const;                          // Where the icon is drawn: centred above the text

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString icon;
};

#endif // EMPTYSTATE_H
