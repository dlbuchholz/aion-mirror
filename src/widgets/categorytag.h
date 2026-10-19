#ifndef CATEGORYTAG_H
#define CATEGORYTAG_H

#include <QWidget>

// A category's rounded tag in its colour, as the transaction list shows it
class CategoryTag : public QWidget {
public:
    explicit CategoryTag(QWidget* parent = nullptr);

    void setCategory(const QString& name, const QString& color);
    QString text() const { return name; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QFont tagFont() const;

    QString name;
    QString color;
};

#endif // CATEGORYTAG_H
