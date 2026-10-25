#ifndef DESKTOPTABS_H
#define DESKTOPTABS_H

#include <QList>
#include <QStringList>
#include <QTabWidget>

class QAction;
class QMdiArea;
class QMdiSubWindow;
class QMenu;
class QSettings;
class QWidget;

// Desktops, like CANoe's: a tab bar where every tab is its own window area
// (QMdiArea) with its own windows and layout. Each window is on exactly one
// desktop and can be moved to another one.
//  - The + button (or a double-click on the empty tab bar) adds a desktop.
//  - Double-click a tab to rename it; right-click it to rename or close it. A closed
//    desktop's windows move to its neighbour, so no window is lost; the last
//    desktop cannot be closed.
//  - Tabs can be dragged into another order.
//  - Right-click a desktop's empty area (or its tab) to add a window to it: the
//    window moves there from whatever desktop it is on, and opens if it was closed.
//  - Right-click a tab > Move to Separate Window puts the desktop in a window of its
//    own (e.g. for a second monitor); closing that window (or Back to Tabs) puts it
//    back. At least one desktop stays a tab.
//
// Desktops are numbered with the tabs first (as they are in the tab bar), then the
// separate ones (in the order they were detached).
class DesktopTabs : public QTabWidget
{
    Q_OBJECT

public:
    explicit DesktopTabs(QWidget *parent = nullptr);

    // Adds a desktop as the last tab and returns its window area.
    QMdiArea *addDesktop(const QString &name);
    QMdiArea *currentArea() const; // of the current tab

    int desktopCount() const;
    QMdiArea *desktopArea(int desktop) const;
    QString desktopName(int desktop) const;
    QStringList desktopNames() const;
    bool isDetached(int desktop) const;

    // The desktop the window is on, or -1.
    int desktopOf(const QMdiSubWindow *window) const;
    // The desktop of this window area, or -1.
    int desktopOfArea(const QMdiArea *area) const;
    // Moves the window to another desktop, keeping its size, position and visibility.
    void moveWindow(QMdiSubWindow *window, int desktop);
    // Brings the window's desktop to the front and shows the window there (also if it was closed).
    void showWindow(QMdiSubWindow *window);
    // Fills a menu with one entry per desktop that moves the window there.
    void fillMoveMenu(QMenu *menu, QMdiSubWindow *window);
    // Fills a menu with one entry per window that brings it to this desktop.
    void fillAddMenu(QMenu *menu, int desktop);

    // Actions whose shortcuts also work in the separate desktop windows (Start, Save, ...).
    void setSharedActions(const QList<QAction *> &actions) { m_sharedActions = actions; }

    // Makes these the desktops, all as tabs, in this order: windows on desktops that
    // go away move to the first one. For opening a configuration. Never fewer than one.
    void setDesktops(QStringList names);
    // Which desktops are separate windows and where, for a configuration.
    void saveDetached(QSettings &settings) const;
    // Detaches the desktops saveDetached() saved; call after setDesktops() and moving the windows.
    void restoreDetached(QSettings &settings);

    void renameDesktop(int tabIndex); // asks for the new name
    void closeDesktop(int tabIndex);
    // Moves the tab's desktop into a window of its own. Not the last tab.
    QWidget *detachDesktop(int tabIndex);
    // Puts a separate desktop back as the last tab.
    void attachDesktop(QMdiArea *area);

private:
    struct Detached
    {
        QMdiArea *area;
        QWidget *window;
        QString name;
    };

    QString newDesktopName() const;
    void showTabMenu(const QPoint &position);
    void showAreaMenu(QMdiArea *area, const QPoint &position);
    QList<QMdiSubWindow *> allWindows() const;

    QList<Detached> m_detached;
    QList<QAction *> m_sharedActions;
};

#endif // DESKTOPTABS_H
