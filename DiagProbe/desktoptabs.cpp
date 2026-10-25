#include "desktoptabs.h"

#include <QInputDialog>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QTabBar>
#include <QToolButton>

#include <algorithm>

DesktopTabs::DesktopTabs(QWidget *parent)
    : QTabWidget(parent)
{
    setDocumentMode(true);
    setMovable(true);

    auto *addButton = new QToolButton;
    addButton->setText("+");
    addButton->setAutoRaise(true);
    addButton->setToolTip("New desktop");
    connect(addButton, &QToolButton::clicked, this, [this] { setCurrentWidget(addDesktop(newDesktopName())); });
    setCornerWidget(addButton, Qt::TopRightCorner);

    connect(tabBar(), &QTabBar::tabBarDoubleClicked, this, [this](int index) {
        if (index < 0)
            setCurrentWidget(addDesktop(newDesktopName())); // the empty part of the tab bar
        else
            renameDesktop(index);
    });
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar(), &QTabBar::customContextMenuRequested, this, &DesktopTabs::showTabMenu);
}

QMdiArea *DesktopTabs::addDesktop(const QString &name)
{
    auto *area = new QMdiArea;
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    area->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(area, &QMdiArea::customContextMenuRequested, this,
            [this, area](const QPoint &position) { showAreaMenu(area, position); });
    addTab(area, name);
    return area;
}

QMdiArea *DesktopTabs::area(int index) const
{
    return qobject_cast<QMdiArea *>(widget(index));
}

QMdiArea *DesktopTabs::currentArea() const
{
    return qobject_cast<QMdiArea *>(currentWidget());
}

QStringList DesktopTabs::desktopNames() const
{
    QStringList names;
    for (int i = 0; i < count(); ++i)
        names << tabText(i);
    return names;
}

int DesktopTabs::desktopOf(const QMdiSubWindow *window) const
{
    return window ? indexOf(window->mdiArea()) : -1;
}

void DesktopTabs::moveWindow(QMdiSubWindow *window, int desktopIndex)
{
    QMdiArea *target = area(desktopIndex);
    QMdiArea *source = window->mdiArea();
    if (!target || target == source)
        return;
    const QRect geometry = window->geometry();
    const bool visible = !window->isHidden();
    if (source)
        source->removeSubWindow(window);
    target->addSubWindow(window);
    window->setGeometry(geometry);
    window->setVisible(visible);
}

void DesktopTabs::showWindow(QMdiSubWindow *window)
{
    setCurrentWidget(window->mdiArea());
    // Closing a window only hides it, and the widget inside it too; show both again.
    window->widget()->show();
    window->showNormal();
    window->mdiArea()->setActiveSubWindow(window);
}

void DesktopTabs::fillMoveMenu(QMenu *menu, QMdiSubWindow *window)
{
    menu->clear();
    if (!window) {
        menu->addAction("No active window")->setEnabled(false);
        return;
    }
    const int current = desktopOf(window);
    for (int i = 0; i < count(); ++i) {
        QAction *action = menu->addAction(tabText(i));
        action->setCheckable(true);
        action->setChecked(i == current);
        action->setEnabled(i != current);
        connect(action, &QAction::triggered, this, [this, window, i] {
            moveWindow(window, i);
            showWindow(window); // follow it to its new desktop
        });
    }
}

void DesktopTabs::fillAddMenu(QMenu *menu, int desktopIndex)
{
    menu->clear();
    QList<QMdiSubWindow *> windows = allWindows();
    std::sort(windows.begin(), windows.end(), [](const QMdiSubWindow *a, const QMdiSubWindow *b) {
        return a->windowTitle() < b->windowTitle();
    });
    for (QMdiSubWindow *window : windows) {
        const int current = desktopOf(window);
        const bool here = current == desktopIndex;
        // Where it is now, so it is clear that adding it here takes it away from there.
        QString where;
        if (window->isHidden())
            where = here ? " (closed)" : QString(" (closed, on %1)").arg(tabText(current));
        else if (!here)
            where = QString(" (on %1)").arg(tabText(current));
        QAction *action = menu->addAction(window->windowTitle() + where);
        action->setEnabled(!here || window->isHidden()); // already open here: nothing to do
        connect(action, &QAction::triggered, this, [this, window, desktopIndex] {
            moveWindow(window, desktopIndex);
            showWindow(window);
        });
    }
}

void DesktopTabs::setDesktops(QStringList names)
{
    if (names.isEmpty())
        names << "Desktop 1";
    while (count() > names.size()) {
        // Keep the windows: move them to the first desktop, then drop the last one.
        QMdiArea *last = area(count() - 1);
        for (QMdiSubWindow *window : last->subWindowList())
            moveWindow(window, 0);
        removeTab(count() - 1);
        delete last;
    }
    while (count() < names.size())
        addDesktop(QString());
    for (int i = 0; i < names.size(); ++i)
        setTabText(i, names[i]);
}

void DesktopTabs::renameDesktop(int index)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Rename Desktop", "Name:", QLineEdit::Normal,
                                               tabText(index), &ok).trimmed();
    if (ok && !name.isEmpty())
        setTabText(index, name);
}

void DesktopTabs::closeDesktop(int index)
{
    if (count() <= 1 || index < 0 || index >= count())
        return;
    QMdiArea *closing = area(index);
    const int neighbour = index == 0 ? 1 : index - 1;
    for (QMdiSubWindow *window : closing->subWindowList())
        moveWindow(window, neighbour);
    removeTab(index);
    delete closing;
}

QString DesktopTabs::newDesktopName() const
{
    const QStringList names = desktopNames();
    for (int number = count() + 1;; ++number) {
        const QString name = QString("Desktop %1").arg(number);
        if (!names.contains(name))
            return name;
    }
}

void DesktopTabs::showTabMenu(const QPoint &position)
{
    const int index = tabBar()->tabAt(position);
    QMenu menu;
    if (index >= 0) {
        menu.addAction("Rename...", this, [this, index] { renameDesktop(index); });
        QAction *close = menu.addAction("Close", this, [this, index] { closeDesktop(index); });
        close->setEnabled(count() > 1);
        if (count() <= 1)
            close->setToolTip("The last desktop cannot be closed");
        menu.addSeparator();
    }
    if (index >= 0) {
        QMenu *add = menu.addMenu("Add Window");
        fillAddMenu(add, index);
        menu.addSeparator();
    }
    menu.addAction("New Desktop", this, [this] { setCurrentWidget(addDesktop(newDesktopName())); });
    menu.exec(tabBar()->mapToGlobal(position));
}

void DesktopTabs::showAreaMenu(QMdiArea *area, const QPoint &position)
{
    // Only on the empty area, not on a window (whose own right-clicks end up here too).
    for (QMdiSubWindow *window : area->subWindowList()) {
        if (window->isVisible() && window->geometry().contains(position))
            return;
    }
    QMenu menu;
    QMenu *add = menu.addMenu("Add Window");
    fillAddMenu(add, indexOf(area));
    menu.addSeparator();
    menu.addAction("Tile", area, &QMdiArea::tileSubWindows);
    menu.addAction("Cascade", area, &QMdiArea::cascadeSubWindows);
    menu.exec(area->viewport()->mapToGlobal(position)); // positions are relative to the viewport
}

QList<QMdiSubWindow *> DesktopTabs::allWindows() const
{
    QList<QMdiSubWindow *> windows;
    for (int i = 0; i < count(); ++i)
        windows += area(i)->subWindowList();
    return windows;
}
