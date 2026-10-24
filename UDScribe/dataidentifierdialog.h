#ifndef DATAIDENTIFIERDIALOG_H
#define DATAIDENTIFIERDIALOG_H

#include "usccore/dataidentifier.h"

#include <QDialog>
#include <QHash>
#include <QStringList>
#include <QVector>

class DataIdentifierLayoutWidget;
class QLineEdit;
class QTableWidget;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;

using namespace usc;

// Asks for the properties of a DID: its number, name and length, and the data types
// placed in its data record. Who may read it is set under 0x22 ReadDataByIdentifier
// (AccessDialog); this dialog keeps it as it is.
class DataIdentifierDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other DIDs; OK is refused for these.
    // usedDids: DID numbers of the other DIDs; OK is refused for these.
    // allDataTypes: the data types that can be placed in the DID.
    DataIdentifierDialog(const DataIdentifier &dataIdentifier, const QStringList &existingNames,
                         const QVector<quint16> &usedDids, const QVector<DataType> &allDataTypes,
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

    // Comment tab
    QPlainTextEdit *m_commentEdit = nullptr;
};

#endif // DATAIDENTIFIERDIALOG_H
