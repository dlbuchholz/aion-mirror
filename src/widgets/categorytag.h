#ifndef CATEGORYTAG_H
#define CATEGORYTAG_H

#include <QWidget>
#include <optional>

// A category's rounded tag in its colour, as the transaction list shows it
class CategoryTag : public QWidget {
public:
    explicit CategoryTag(QWidget* parent = nullptr);

    void setCategory(const QString& name, const QString& color);
    void setTheme(bool dark);                        // Always drawn for this theme, for previews
    QString text() const { return name; }
    QString colorName() const { return color; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QFont tagFont() const;

    QString name;
    QString color;
    std::optional<bool> dark;                        // Unset: the active theme
};

#endif // CATEGORYTAG_H
