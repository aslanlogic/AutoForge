#include "mainwindow.h"
#include "navigationmodel.h"
#include "securityleveldialog.h"
#include "usccore/database.h"

#include <QMenu>
#include <QMessageBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTreeView>

namespace {

// The requestSeed sub-functions of all security levels except the one with this id.
QVector<quint8> requestSeedsUsedByOthers(const Database &database, int id)
{
    QVector<quint8> requestSeeds;
    for (const SecurityLevel &securityLevel : database.allSecurityLevels()) {
        if (securityLevel.id != id)
            requestSeeds.append(securityLevel.requestSeed);
    }
    return requestSeeds;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1024, 768);
    setWindowTitle("UDScribe");

    m_database = new Database(this);
    m_navigationModel = new NavigationModel(m_database, this);

    setupLayout();
}

void MainWindow::setupLayout()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Left pane: the services tree. QTreeView scrolls on its own when content overflows.
    m_navigationTree = new QTreeView(m_splitter);
    m_navigationTree->setHeaderHidden(true);
    m_navigationTree->setModel(m_navigationModel);

    // Show newly added items by expanding their service.
    connect(m_navigationModel, &NavigationModel::rowsInserted,
            m_navigationTree, &QTreeView::expand);

    m_navigationTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_navigationTree, &QTreeView::customContextMenuRequested,
            this, &MainWindow::showNavigationContextMenu);
    connect(m_navigationTree, &QTreeView::doubleClicked,
            this, &MainWindow::onNavigationDoubleClicked);

    // Right pane: will show what is clicked in the tree.
    m_rightPane = new QStackedWidget(m_splitter);
    m_emptyPage = new QWidget;
    m_rightPane->addWidget(m_emptyPage);

    m_splitter->addWidget(m_navigationTree);
    m_splitter->addWidget(m_rightPane);
    m_splitter->setChildrenCollapsible(false);

    // Start with the left pane at 20% of the window width.
    const int leftWidth = width() * 20 / 100;
    m_splitter->setSizes({leftWidth, width() - leftWidth});

    setCentralWidget(m_splitter);
}

void MainWindow::showNavigationContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_navigationTree->indexAt(pos);
    if (!index.isValid())
        return;
    const int kind = index.data(NavigationModel::ItemKindRole).toInt();
    const int id = index.data(NavigationModel::ObjectIdRole).toInt();

    // Use the desktop theme's icons, falling back to Qt's built-in ones where no theme exists (e.g. Windows).
    const QIcon newIcon = QIcon::fromTheme("document-new", style()->standardIcon(QStyle::SP_FileIcon));
    const QIcon deleteIcon = QIcon::fromTheme("edit-delete", style()->standardIcon(QStyle::SP_TrashIcon));

    // Services offer "Add...", the items under them offer "Delete".
    QMenu menu(this);
    if (kind == NavigationModel::ServiceItem) {
        if (index.row() == NavigationModel::SecurityAccess)
            menu.addAction(newIcon, "Add...", this, &MainWindow::createNewSecurityLevel);
    } else if (kind == NavigationModel::SecurityLevelItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteSecurityLevel(id); });
    }

    if (!menu.isEmpty())
        menu.exec(m_navigationTree->viewport()->mapToGlobal(pos));
}

void MainWindow::onNavigationDoubleClicked(const QModelIndex &index)
{
    const int id = index.data(NavigationModel::ObjectIdRole).toInt();
    switch (index.data(NavigationModel::ItemKindRole).toInt()) {
    case NavigationModel::SecurityLevelItem:
        editSecurityLevel(id);
        break;
    }
}

void MainWindow::createNewSecurityLevel()
{
    const QStringList existingNames = m_database->securityLevelNames();
    const QVector<quint8> usedRequestSeeds = requestSeedsUsedByOthers(*m_database, 0);

    // Pick the first free default name: New_SecurityLevel_1, New_SecurityLevel_2, ...
    SecurityLevel newSecurityLevel;
    int number = 1;
    do {
        newSecurityLevel.name = QString("New_SecurityLevel_%1").arg(number++);
    } while (existingNames.contains(newSecurityLevel.name));

    // And the first free requestSeed sub-function. If none is free, the dialog says so.
    for (int value = 0; value <= 0xFF; ++value) {
        const quint8 requestSeed = quint8(value);
        if (isValidRequestSeed(requestSeed) && !usedRequestSeeds.contains(requestSeed)) {
            newSecurityLevel.requestSeed = requestSeed;
            break;
        }
    }

    SecurityLevelDialog dialog(newSecurityLevel, existingNames, usedRequestSeeds, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->addSecurityLevel(dialog.securityLevel());
}

void MainWindow::editSecurityLevel(int id)
{
    const SecurityLevel *stored = m_database->findSecurityLevel(id);
    if (!stored)
        return;
    const SecurityLevel securityLevel = *stored; // copy: the pointer is only valid until the next change

    QStringList otherNames = m_database->securityLevelNames();
    otherNames.removeOne(securityLevel.name);

    SecurityLevelDialog dialog(securityLevel, otherNames, requestSeedsUsedByOthers(*m_database, id), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->updateSecurityLevel(dialog.securityLevel());
}

void MainWindow::deleteSecurityLevel(int id)
{
    const SecurityLevel *securityLevel = m_database->findSecurityLevel(id);
    if (!securityLevel)
        return;

    const QString question = QString("Delete the security level '%1 %2'?")
                                 .arg(subFunctionToText(securityLevel->requestSeed), securityLevel->name);
    if (QMessageBox::question(this, "Delete Security Level", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeSecurityLevel(id);
}
