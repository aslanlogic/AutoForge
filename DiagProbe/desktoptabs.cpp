#include "desktoptabs.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

// The window of a separate desktop. Closing it puts the desktop back as a tab.
class DesktopWindow : public QWidget
{
public:
    DesktopWindow(QWidget *parent, std::function<void()> backToTabs)
        : QWidget(parent, Qt::Window), m_backToTabs(std::move(backToTabs)) {}

protected:
    void closeEvent(QCloseEvent *event) override
    {
        event->ignore(); // the window goes away when the desktop is back as a tab
        QMetaObject::invokeMethod(this, [this] { m_backToTabs(); }, Qt::QueuedConnection);
    }

private:
    std::function<void()> m_backToTabs;
};

} // namespace

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

QMdiArea *DesktopTabs::currentArea() const
{
    return qobject_cast<QMdiArea *>(currentWidget());
}

int DesktopTabs::desktopCount() const
{
    return count() + m_detached.size();
}

QMdiArea *DesktopTabs::desktopArea(int desktop) const
{
    if (desktop >= 0 && desktop < count())
        return qobject_cast<QMdiArea *>(widget(desktop));
    desktop -= count();
    return desktop >= 0 && desktop < m_detached.size() ? m_detached[desktop].area : nullptr;
}

QString DesktopTabs::desktopName(int desktop) const
{
    if (desktop >= 0 && desktop < count())
        return tabText(desktop);
    desktop -= count();
    return desktop >= 0 && desktop < m_detached.size() ? m_detached[desktop].name : QString();
}

QStringList DesktopTabs::desktopNames() const
{
    QStringList names;
    for (int i = 0; i < desktopCount(); ++i)
        names << desktopName(i);
    return names;
}

bool DesktopTabs::isDetached(int desktop) const
{
    return desktop >= count() && desktop < desktopCount();
}

int DesktopTabs::desktopOf(const QMdiSubWindow *window) const
{
    return window ? desktopOfArea(window->mdiArea()) : -1;
}

int DesktopTabs::desktopOfArea(const QMdiArea *area) const
{
    for (int i = 0; area && i < desktopCount(); ++i) {
        if (desktopArea(i) == area)
            return i;
    }
    return -1;
}

void DesktopTabs::moveWindow(QMdiSubWindow *window, int desktop)
{
    QMdiArea *target = desktopArea(desktop);
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
    QMdiArea *area = window->mdiArea();
    if (indexOf(area) >= 0) {
        setCurrentWidget(area);
    } else {
        // On a separate desktop: bring that window to the front.
        area->window()->showNormal();
        area->window()->raise();
        area->window()->activateWindow();
    }
    // Closing a window only hides it, and the widget inside it too; show both again.
    window->widget()->show();
    window->showNormal();
    area->setActiveSubWindow(window);
}

void DesktopTabs::fillMoveMenu(QMenu *menu, QMdiSubWindow *window)
{
    menu->clear();
    if (!window) {
        menu->addAction("No active window")->setEnabled(false);
        return;
    }
    const int current = desktopOf(window);
    for (int i = 0; i < desktopCount(); ++i) {
        QAction *action = menu->addAction(desktopName(i) + (isDetached(i) ? " (separate window)" : ""));
        action->setCheckable(true);
        action->setChecked(i == current);
        action->setEnabled(i != current);
        connect(action, &QAction::triggered, this, [this, window, i] {
            moveWindow(window, i);
            showWindow(window); // follow it to its new desktop
        });
    }
}

void DesktopTabs::fillAddMenu(QMenu *menu, int desktop)
{
    menu->clear();
    QList<QMdiSubWindow *> windows = allWindows();
    std::sort(windows.begin(), windows.end(), [](const QMdiSubWindow *a, const QMdiSubWindow *b) {
        return a->windowTitle() < b->windowTitle();
    });
    for (QMdiSubWindow *window : windows) {
        const int current = desktopOf(window);
        const bool here = current == desktop;
        // Where it is now, so it is clear that adding it here takes it away from there.
        QString where;
        if (window->isHidden())
            where = here ? " (closed)" : QString(" (closed, on %1)").arg(desktopName(current));
        else if (!here)
            where = QString(" (on %1)").arg(desktopName(current));
        QAction *action = menu->addAction(window->windowTitle() + where);
        action->setEnabled(!here || window->isHidden()); // already open here: nothing to do
        // By area, not number: the numbers change when a desktop is detached.
        QMdiArea *area = desktopArea(desktop);
        connect(action, &QAction::triggered, this, [this, window, area] {
            moveWindow(window, desktopOfArea(area));
            showWindow(window);
        });
    }
}

void DesktopTabs::setDesktops(QStringList names)
{
    while (!m_detached.isEmpty())
        attachDesktop(m_detached.first().area);
    if (names.isEmpty())
        names << "Desktop 1";
    while (count() > names.size()) {
        // Keep the windows: move them to the first desktop, then drop the last one.
        QMdiArea *last = desktopArea(count() - 1);
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

void DesktopTabs::saveDetached(QSettings &settings) const
{
    QStringList desktops; // plain numbers, so the file stays readable
    for (int i = count(); i < desktopCount(); ++i)
        desktops << QString::number(i);
    settings.setValue("Desktops/detached", desktops);
    for (int i = 0; i < m_detached.size(); ++i)
        settings.setValue(QString("Desktops/detachedGeometry%1").arg(count() + i), m_detached[i].window->geometry());
}

void DesktopTabs::restoreDetached(QSettings &settings)
{
    // By area: detaching renumbers the tabs.
    QList<QPair<QMdiArea *, QRect>> toDetach;
    for (const QString &value : settings.value("Desktops/detached").toStringList()) {
        const int desktop = value.toInt();
        if (QMdiArea *area = desktopArea(desktop))
            toDetach << qMakePair(area, settings.value(QString("Desktops/detachedGeometry%1").arg(desktop)).toRect());
    }
    for (const auto &item : toDetach) {
        if (QWidget *window = detachDesktop(indexOf(item.first))) {
            if (item.second.isValid())
                window->setGeometry(item.second);
        }
    }
}

void DesktopTabs::renameDesktop(int tabIndex)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Rename Desktop", "Name:", QLineEdit::Normal,
                                               tabText(tabIndex), &ok).trimmed();
    if (ok && !name.isEmpty())
        setTabText(tabIndex, name);
}

void DesktopTabs::closeDesktop(int tabIndex)
{
    if (count() <= 1 || tabIndex < 0 || tabIndex >= count())
        return;
    QMdiArea *closing = desktopArea(tabIndex);
    const int neighbour = tabIndex == 0 ? 1 : tabIndex - 1;
    for (QMdiSubWindow *window : closing->subWindowList())
        moveWindow(window, neighbour);
    removeTab(tabIndex);
    delete closing;
}

QWidget *DesktopTabs::detachDesktop(int tabIndex)
{
    if (count() <= 1 || tabIndex < 0 || tabIndex >= count())
        return nullptr; // one desktop always stays in the main window
    auto *area = qobject_cast<QMdiArea *>(widget(tabIndex));
    const QString name = tabText(tabIndex);
    const QSize size = currentWidget()->size(); // as big as the desktops in the main window
    removeTab(tabIndex);

    auto *window = new DesktopWindow(this->window(), [this, area] { attachDesktop(area); });
    window->setWindowTitle(name + " - DiagProbe");
    window->addActions(m_sharedActions); // their shortcuts work here too
    auto *backButton = new QPushButton("Back to Tabs");
    backButton->setToolTip("Put this desktop back into the main window");
    connect(backButton, &QPushButton::clicked, this, [this, area] { attachDesktop(area); });
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(4, 4, 4, 0);
    bar->addStretch();
    bar->addWidget(backButton);
    auto *layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(bar);
    layout->addWidget(area);
    area->show();
    window->resize(size.width() > 300 && size.height() > 200 ? size : QSize(1000, 700));
    window->show();
    m_detached.append(Detached{area, window, name});
    return window;
}

void DesktopTabs::attachDesktop(QMdiArea *area)
{
    for (int i = 0; i < m_detached.size(); ++i) {
        if (m_detached[i].area != area)
            continue;
        const Detached detached = m_detached.takeAt(i);
        addTab(area, detached.name);
        area->show();
        setCurrentWidget(area);
        detached.window->deleteLater();
        return;
    }
}

QString DesktopTabs::newDesktopName() const
{
    const QStringList names = desktopNames();
    for (int number = desktopCount() + 1;; ++number) {
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
        QAction *detach = menu.addAction("Move to Separate Window", this, [this, index] { detachDesktop(index); });
        detach->setEnabled(count() > 1);
        QAction *close = menu.addAction("Close", this, [this, index] { closeDesktop(index); });
        close->setEnabled(count() > 1);
        menu.addSeparator();
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
    fillAddMenu(add, desktopOfArea(area));
    menu.addSeparator();
    menu.addAction("Tile", area, &QMdiArea::tileSubWindows);
    menu.addAction("Cascade", area, &QMdiArea::cascadeSubWindows);
    menu.exec(area->viewport()->mapToGlobal(position)); // positions are relative to the viewport
}

QList<QMdiSubWindow *> DesktopTabs::allWindows() const
{
    QList<QMdiSubWindow *> windows;
    for (int i = 0; i < desktopCount(); ++i)
        windows += desktopArea(i)->subWindowList();
    return windows;
}
