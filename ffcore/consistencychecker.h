#ifndef CONSISTENCYCHECKER_H
#define CONSISTENCYCHECKER_H

#include <QString>
#include <QVector>

namespace ff {

class Database;

// Looks through a whole document for things that are wrong (errors) or that
// are allowed but probably a mistake (warnings). It only reads the Database;
// it changes nothing and knows nothing about widgets.
namespace ConsistencyChecker {

struct Issue
{
    enum class Severity { Error, Warning };

    Severity severity = Severity::Warning;
    QString objectKind; // "Message", "Signal", "Network Node" or "Value Table"
    QString objectName;
    QString problem;
};

// Errors first, then warnings; within each, in the order they were found.
QVector<Issue> check(const Database &database);

} // namespace ConsistencyChecker

} // namespace ff

#endif // CONSISTENCYCHECKER_H
