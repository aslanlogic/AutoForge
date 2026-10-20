#ifndef CONSISTENCYDIALOG_H
#define CONSISTENCYDIALOG_H

#include "consistencychecker.h"

#include <QDialog>

class QLabel;
class QTableWidget;

// Shows the result of a consistency check: a summary line and one row per
// error or warning. The rows can be sorted by clicking a column header.
class ConsistencyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConsistencyDialog(const QVector<ConsistencyChecker::Issue> &issues, QWidget *parent = nullptr);

private:
    QLabel *m_summaryLabel = nullptr;
    QTableWidget *m_issuesTable = nullptr;
};

#endif // CONSISTENCYDIALOG_H
