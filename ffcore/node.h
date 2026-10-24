#ifndef FFCORE_NODE_H
#define FFCORE_NODE_H

#include <QString>

namespace ff {

// A network node: a device (ECU, sensor, PC, ...) on the network that sends
// and receives messages.
struct Node
{
    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    QString comment;
};

// Equal if every field is equal. Used to tell whether an edit changed anything.
inline bool operator==(const Node &a, const Node &b)
{
    return a.id == b.id && a.name == b.name && a.comment == b.comment;
}

} // namespace ff

#endif // FFCORE_NODE_H
