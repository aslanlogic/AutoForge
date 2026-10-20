#ifndef NAVIGATIONFILTERMODEL_H
#define NAVIGATIONFILTERMODEL_H

#include <QSortFilterProxyModel>

// Sits between NavigationModel and the tree view and
//  - hides the items whose name does not match the search text,
//  - keeps the items under each heading in alphabetical order (A-Z, ignoring case).
// The category headings (Network, Messages, Signals, ...) are always shown,
// in their fixed order, so the tree keeps its shape. The search only looks at
// the items directly under a heading; what is inside a node comes along with it.
//
//   Database -> NavigationModel -> NavigationFilterModel -> QTreeView
//
// Note: indexes coming from the tree belong to this filter model; use
// mapToSource() before comparing them with NavigationModel indexes.
class NavigationFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit NavigationFilterModel(QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override;
};

#endif // NAVIGATIONFILTERMODEL_H
