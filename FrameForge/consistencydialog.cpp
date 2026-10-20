#include "consistencydialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

using ConsistencyChecker::Issue;

ConsistencyDialog::ConsistencyDialog(const QVector<Issue> &issues, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Consistency Check");

    const int errors = int(std::count_if(issues.begin(), issues.end(), [](const Issue &issue) {
        return issue.severity == Issue::Severity::Error;
    }));
    const int warnings = issues.size() - errors;

    m_summaryLabel = new QLabel(issues.isEmpty()
                                    ? "No problems found."
                                    : QString("%1 error(s), %2 warning(s).").arg(errors).arg(warnings));

    m_issuesTable = new QTableWidget(issues.size(), 4);
    m_issuesTable->setHorizontalHeaderLabels({"Severity", "Type", "Name", "Problem"});
    m_issuesTable->horizontalHeader()->setStretchLastSection(true);
    m_issuesTable->verticalHeader()->setVisible(false);
    m_issuesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_issuesTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    const QIcon errorIcon = style()->standardIcon(QStyle::SP_MessageBoxCritical);
    const QIcon warningIcon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
    for (int row = 0; row < issues.size(); ++row) {
        const Issue &issue = issues.at(row);
        const bool error = (issue.severity == Issue::Severity::Error);
        m_issuesTable->setItem(row, 0, new QTableWidgetItem(error ? errorIcon : warningIcon,
                                                            error ? "Error" : "Warning"));
        m_issuesTable->setItem(row, 1, new QTableWidgetItem(issue.objectKind));
        m_issuesTable->setItem(row, 2, new QTableWidgetItem(issue.objectName));
        m_issuesTable->setItem(row, 3, new QTableWidgetItem(issue.problem));
    }
    m_issuesTable->resizeColumnsToContents();
    // No sort column yet, so the errors-first order stays until a header is clicked.
    // (Set before enabling sorting, which would otherwise sort by column 0 right away.)
    m_issuesTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    m_issuesTable->setSortingEnabled(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(m_issuesTable);
    layout->addWidget(buttons);

    resize(820, 420);
}
