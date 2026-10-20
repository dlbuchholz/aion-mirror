#ifndef CATEGORIESSETTINGS_H
#define CATEGORIESSETTINGS_H

#include <QWidget>
#include <functional>
#include "category.h"

class QButtonGroup;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;

// Settings section listing the categories with their colour tags. A row opens an inline
// editor (name, type, colour, merge, delete). Changes are saved right away.
class CategoriesSettings : public QWidget {
    Q_OBJECT

public:
    explicit CategoriesSettings(QWidget* parent = nullptr);

    void setUserId(int userId);                      // Shows and edits that user's list
    void reload();

    // Asks before merging or deleting (a message box; tests replace it)
    std::function<bool(const QString& title, const QString& text)> confirm;

signals:
    void categoriesChanged();

protected:
    void changeEvent(QEvent* event) override;

private:
    void rebuildList();
    void openEditor(int categoryId);                 // 0 = new category
    void closeEditor();
    void selectColor(const QString& color);
    QString selectedColor() const;
    void save();
    void mergeInto(int intoId);
    void remove();
    void updateSwatches();
    const Category* category(int categoryId) const;

    QList<Category> categories;
    QHash<int, CategoryUsage> usage;
    int userId = -1;
    int editedId = -1;                               // -1 = editor closed, 0 = new category

    QLineEdit* searchEdit = nullptr;
    QPushButton* addButton = nullptr;
    QVBoxLayout* listLayout = nullptr;
    QLabel* emptyLabel = nullptr;

    // Inline editor, moved under the row it edits
    QFrame* editor = nullptr;
    QLineEdit* nameEdit = nullptr;
    QComboBox* typeCombo = nullptr;
    QButtonGroup* colorGroup = nullptr;
    QLabel* usageLabel = nullptr;
    QLabel* errorLabel = nullptr;
    QToolButton* mergeButton = nullptr;
    QPushButton* deleteButton = nullptr;
};

#endif // CATEGORIESSETTINGS_H
