#ifndef NODE_H
#define NODE_H

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

} // namespace ff

#endif // NODE_H
