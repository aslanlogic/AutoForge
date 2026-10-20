#include "navigationfiltermodel.h"

NavigationFilterModel::NavigationFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

bool NavigationFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // Top-level rows are the headings: always keep them.
    if (!sourceParent.isValid())
        return true;

    // Items: the default check, i.e. does the name contain the search text.
    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

bool NavigationFilterModel::lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const
{
    // Headings: keep the order NavigationModel gives them.
    if (!sourceLeft.parent().isValid())
        return sourceLeft.row() < sourceRight.row();

    // Items: by name, ignoring case. Names that differ only in case
    // (e.g. "abc" and "ABC") are then compared exactly, so their order is fixed too.
    const QString left = sourceLeft.data().toString();
    const QString right = sourceRight.data().toString();
    const int result = QString::compare(left, right, Qt::CaseInsensitive);
    if (result != 0)
        return result < 0;
    return left < right;
}
