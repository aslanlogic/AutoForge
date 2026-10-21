#ifndef VALUETABLE_H
#define VALUETABLE_H

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace ff {

// Gives names to raw signal values, e.g. 0 = "Off", 1 = "On", 2 = "Error".
// A value table is shared: any number of signals can use the same one.
struct ValueTable
{
    struct Entry
    {
        qint64 value = 0; // raw value, as sent on the bus
        QString description;
    };

    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    QVector<Entry> entries; // sorted by value, no value twice
    QString comment;
};

} // namespace ff

#endif // VALUETABLE_H
