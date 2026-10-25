#ifndef DESKTOPTABS_H
#define DESKTOPTABS_H

#include <QStringList>
#include <QTabWidget>

class QMdiArea;
class QMdiSubWindow;
class QMenu;

// Desktops, like CANoe's: a tab bar where every tab is its own window area
// (QMdiArea) with its own windows and layout. Each window is on exactly one
// desktop and can be moved to another one.
//  - The + button (or a double-click on the empty tab bar) adds a desktop.
//  - Double-click a tab to rename it; right-click it to rename or close it. A closed
//    desktop's windows move to its neighbour, so no window is lost; the last
//    desktop cannot be closed.
//  - Tabs can be dragged into another order.
class DesktopTabs : public QTabWidget
{
    Q_OBJECT

public:
    explicit DesktopTabs(QWidget *parent = nullptr);

    // Adds a desktop at the end and returns its window area.
    QMdiArea *addDesktop(const QString &name);
    QMdiArea *area(int index) const;
    QMdiArea *currentArea() const;
    QStringList desktopNames() const;

    // The index of the desktop the window is on, or -1.
    int desktopOf(const QMdiSubWindow *window) const;
    // Moves the window to another desktop, keeping its size, position and visibility.
    void moveWindow(QMdiSubWindow *window, int desktopIndex);
    // Switches to the window's desktop and shows the window there (also if it was closed).
    void showWindow(QMdiSubWindow *window);
    // Fills a menu with one entry per desktop that moves the window there.
    void fillMoveMenu(QMenu *menu, QMdiSubWindow *window);

    // Makes these the desktops, in this order: windows on desktops that go away
    // move to the first one. For opening a configuration. Never fewer than one.
    void setDesktops(QStringList names);

    void renameDesktop(int index); // asks for the new name
    void closeDesktop(int index);

private:
    QString newDesktopName() const;
    void showTabMenu(const QPoint &position);
};

#endif // DESKTOPTABS_H
