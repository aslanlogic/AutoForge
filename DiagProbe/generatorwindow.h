#ifndef GENERATORWINDOW_H
#define GENERATORWINDOW_H

#include "ffcore/database.h"

#include <QHash>
#include <QVector>
#include <QWidget>

class QComboBox;
class QSettings;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTimer;
class Tester;

// The Interactive generator: sends bus messages of the .ffxml, like CANoe's IG.
//  - Transmit list: the messages to send, each once or cyclically. It starts
//    empty; add messages with Add message.
//  - Signal editor: the signals of the selected message, in physical values or
//    by value table name; the payload follows at once.
//  - Raw frame: any ID and bytes.
// What it sends shows as Tx in the Trace window.
class GeneratorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GeneratorWindow(Tester *tester, QWidget *parent = nullptr);

    // Empties the transmit list and offers the database's messages to add.
    void setDatabase(const ff::Database *database);
    // Adds a message to the transmit list; false if unknown.
    bool addMessage(int messageId);
    int entryCount() const { return m_entries.size(); }
    // Sends entry `index` once.
    void sendEntry(int index);
    // Sets a signal's physical value in entry `index`.
    bool setSignalValue(int index, int signalId, double physical);
    QByteArray payloadOf(int index) const;
    // Starts or stops sending entry `index` every cycle time.
    void setCyclic(int index, bool running);

    // The transmit list, for a DiagProbe configuration: each message by name with
    // its mode, cycle time and signal values. Load after setDatabase().
    void saveSettings(QSettings &settings) const;
    void loadSettings(QSettings &settings);

private:
    struct Entry
    {
        int messageId = 0;
        QHash<int, double> values; // ff Signal::id -> physical value
        QByteArray payload;
        QTimer *timer = nullptr;
        bool cyclic = false; // the mode: sent every cycle time (when started) instead of once
    };

    void clearEntries();
    void rebuildList();
    void updateRow(int index);
    void showSignals();
    void encode(Entry &entry);
    void sendRawFrame();

    Tester *m_tester;
    const ff::Database *m_database = nullptr;
    QVector<Entry> m_entries;

    QComboBox *m_addCombo;
    QTableWidget *m_list;
    QLabel *m_signalsLabel;
    QTableWidget *m_signals;
    QLineEdit *m_rawId;
    QLineEdit *m_rawData;
    QLabel *m_rawStatus;
};

#endif // GENERATORWINDOW_H
