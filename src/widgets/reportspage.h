#ifndef REPORTSPAGE_H
#define REPORTSPAGE_H

#include <QWidget>
#include <functional>
#include "report.h"

class QButtonGroup;
class QComboBox;
class QLabel;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;
class CategoryBars;
class FlowChart;
class ReportCard;

// Reports: an overview with a card per report, and one report at a time in detail. The range
// (remembered) and the account are chosen at the top and apply to all of them.
class ReportsPage : public QWidget {
    Q_OBJECT

public:
    enum class Report {
        SpendingByCategory,
        IncomeVsSpending,
        Balance,
        CategoryTrend,
        TopPayees,
        BudgetVsActual
    };
    static constexpr int ALL_ACCOUNTS = -1;

    explicit ReportsPage(QWidget* parent = nullptr);

    // Asks for the days of a custom range, filled with the current ones (a dialog; tests replace it)
    std::function<bool(QDate& from, QDate& to)> askCustomRange;

    void setUserId(int userId);
    void reload();                                   // Accounts and transactions changed
    void applySettings();                            // Number formats changed

    void openReport(Report report);
    void showOverview();
    std::optional<Report> openedReport() const;      // None in the overview

    ReportRange range() const { return shownRange; }
    int accountId() const { return shownAccountId; }

    static QString title(Report report);             // "Spending by category"

protected:
    void changeEvent(QEvent* event) override;

private:
    QToolButton* addChip(const QString& text, ReportRange::Preset chipPreset, QLayout* layout);
    void onRangeClicked(int presetId);
    void setRange(ReportRange::Preset preset, const ReportRange& range);
    void refresh();                                  // The range or account changed
    void updateHeader();
    void updateIcons();
    void updateCards();
    void updateDetail();
    QWidget* spendingDetail(QWidget* parent) const;
    QWidget* flowDetail(QWidget* parent) const;

    int userId = -1;
    ReportRange::Preset preset = ReportRange::Preset::SixMonths;
    ReportRange shownRange;
    int shownAccountId = ALL_ACCOUNTS;
    std::optional<Report> opened;

    QToolButton* backLink = nullptr;
    QLabel* titleLabel = nullptr;
    QLabel* rangeLabel = nullptr;
    QButtonGroup* rangeGroup = nullptr;
    QComboBox* accountCombo = nullptr;
    QStackedWidget* stack = nullptr;
    QList<ReportCard*> cards;                        // In Report order
    CategoryBars* spendingPreview = nullptr;
    FlowChart* flowPreview = nullptr;
    QVBoxLayout* detailLayout = nullptr;
    QWidget* detailContent = nullptr;                // The opened report's, rebuilt when the numbers change

    SpendingReport spending;
    FlowReport flow;
};

#endif // REPORTSPAGE_H
