#ifndef DATAIDENTIFIERDIALOG_H
#define DATAIDENTIFIERDIALOG_H

#include "usccore/dataidentifier.h"
#include "usccore/securitylevel.h"
#include "usccore/session.h"

#include <QDialog>
#include <QHash>
#include <QStringList>
#include <QVector>

class DataIdentifierLayoutWidget;
class QLineEdit;
class QTableWidget;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;

using namespace usc;

// Asks for all properties of a DID: its number, name and length, who may read
// it, and (on the other tabs) the data types placed in its data record.
class DataIdentifierDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other DIDs; OK is refused for these.
    // usedDids: DID numbers of the other DIDs; OK is refused for these.
    // allDataTypes: the data types that can be placed in the DID.
    // sessions, securityLevels: all of them, offered for read access.
    DataIdentifierDialog(const DataIdentifier &dataIdentifier, const QStringList &existingNames,
                         const QVector<quint16> &usedDids, const QVector<DataType> &allDataTypes,
                         const QVector<Session> &sessions, const QVector<SecurityLevel> &securityLevels,
                         QWidget *parent = nullptr);

    DataIdentifier dataIdentifier() const;

public slots:
    void accept() override;

private slots:
    void addDataType();
    void renameDataType();
    void removeDataType();
    void moveDataType(int mappingIndex, int newStartBit);

private:
    QWidget *createDefinitionTab();
    QWidget *createDataTypesTab();
    QWidget *createLayoutTab();
    QWidget *createReadAccessTab(const QVector<Session> &sessions, const QVector<SecurityLevel> &securityLevels);
    QWidget *createCommentTab();
    void refreshDataTypes(); // redraws the Data Types and Layout tabs from m_mappings
    int findFreeStartBit(const DataType &dataType) const; // -1 if it does not fit anywhere

    DataIdentifier m_dataIdentifier; // the DID as it was when the dialog opened
    QStringList m_existingNames;
    QVector<quint16> m_usedDids;
    QHash<int, DataType> m_dataTypesById;
    QVector<DataTypeMapping> m_mappings; // edited here; copied into the DID on OK
    QTabWidget *m_tabs = nullptr;

    // Definition tab
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_didEdit = nullptr;
    QSpinBox *m_lengthSpin = nullptr;

    // Data Types tab
    QWidget *m_dataTypesTab = nullptr;
    QTableWidget *m_dataTypesTable = nullptr;

    // Layout tab
    DataIdentifierLayoutWidget *m_layoutWidget = nullptr;

    // Read Access tab
    QWidget *m_readAccessTab = nullptr;
    QListWidget *m_sessionList = nullptr;       // one checkable row per session; data = Session::id
    QListWidget *m_securityLevelList = nullptr; // one checkable row per level; data = SecurityLevel::id

    // Comment tab
    QPlainTextEdit *m_commentEdit = nullptr;
};

#endif // DATAIDENTIFIERDIALOG_H
