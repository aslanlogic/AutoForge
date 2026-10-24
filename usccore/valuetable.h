#ifndef USCCORE_VALUETABLE_H
#define USCCORE_VALUETABLE_H

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace usc {

// Gives names to raw values, e.g. 0 = "Off", 1 = "On", 2 = "Error".
// A value table is shared: any number of data types can use the same one.
struct ValueTable
{
    struct Entry
    {
        qint64 value = 0; // raw value, as sent in the data record
        QString description;
    };

    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    QVector<Entry> entries; // sorted by value, no value twice
    QString comment;
};

// Equal if every field is equal. Used to tell whether an edit changed anything.
inline bool operator==(const ValueTable::Entry &a, const ValueTable::Entry &b)
{
    return a.value == b.value && a.description == b.description;
}

inline bool operator==(const ValueTable &a, const ValueTable &b)
{
    return a.id == b.id && a.name == b.name && a.entries == b.entries && a.comment == b.comment;
}

} // namespace usc

#endif // USCCORE_VALUETABLE_H
