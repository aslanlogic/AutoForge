#include "consistencychecker.h"
#include "database.h"

#include <QHash>
#include <QLocale>

#include <algorithm>
#include <cmath>

namespace ConsistencyChecker {

namespace {

using Severity = Issue::Severity;

QString number(double value)
{
    return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
}

// The raw values an integer signal can carry. Returns false for float signals,
// which have no such simple range.
bool rawRange(const Signal &signal, double *minimum, double *maximum)
{
    switch (signal.valueType) {
    case Signal::ValueType::Unsigned:
        *minimum = 0;
        *maximum = std::ldexp(1.0, signal.length) - 1;
        return true;
    case Signal::ValueType::Signed:
        *minimum = -std::ldexp(1.0, signal.length - 1);
        *maximum = std::ldexp(1.0, signal.length - 1) - 1;
        return true;
    case Signal::ValueType::IeeeFloat:
    case Signal::ValueType::IeeeDouble:
        return false;
    }
    return false;
}

void checkMessages(const Database &database, QVector<Issue> &issues)
{
    const Network &network = database.network();
    QHash<int, Signal> signalsById;
    for (const Signal &signal : database.allSignals())
        signalsById.insert(signal.id, signal);

    // Two messages with the same ID would be the same frame on the bus.
    // On CAN, a standard and an extended ID with the same number are different frames.
    QHash<QString, QString> firstWithId; // ID (with format on CAN) -> message name
    for (const Message &message : database.allMessages()) {
        auto add = [&](Severity severity, const QString &problem) {
            issues.append({severity, "Message", message.name, problem});
        };

        QString networkProblem = checkMessageAgainstNetwork(message, network);
        if (!networkProblem.isEmpty()) {
            // checkMessageAgainstNetwork() starts with "Message '<name>': "; the name has its own column.
            const QString prefix = QString("Message '%1': ").arg(message.name);
            if (networkProblem.startsWith(prefix)) {
                networkProblem.remove(0, prefix.size());
                networkProblem[0] = networkProblem.at(0).toUpper();
            }
            add(Severity::Error, networkProblem);
        }

        const QString idKey = frameIdToText(message.frameId)
            + (isCan(network.protocol) && message.idFormat == Message::IdFormat::Extended ? " ext" : "");
        if (firstWithId.contains(idKey))
            add(Severity::Error, QString("Has the same ID %1 as message '%2'.")
                                     .arg(frameIdToText(message.frameId), firstWithId.value(idKey)));
        else
            firstWithId.insert(idKey, message.name);

        const QString layoutProblem = checkSignalLayout(message.length, message.signalMappings, signalsById);
        if (!layoutProblem.isEmpty())
            add(Severity::Error, layoutProblem);

        if (message.transmitterId == 0)
            add(Severity::Warning, "Has no transmitter.");
        if (message.receiverIds.isEmpty())
            add(Severity::Warning, "Has no receivers.");
        if (message.signalMappings.isEmpty())
            add(Severity::Warning, "Carries no signals.");
    }
}

void checkSignals(const Database &database, QVector<Issue> &issues)
{
    for (const Signal &signal : database.allSignals()) {
        auto add = [&](Severity severity, const QString &problem) {
            issues.append({severity, "Signal", signal.name, problem});
        };

        if (signal.valueType == Signal::ValueType::IeeeFloat && signal.length != 32)
            add(Severity::Error, QString("Is an IEEE Float but %1 bits long (must be 32).").arg(signal.length));
        if (signal.valueType == Signal::ValueType::IeeeDouble && signal.length != 64)
            add(Severity::Error, QString("Is an IEEE Double but %1 bits long (must be 64).").arg(signal.length));
        if (signal.factor == 0)
            add(Severity::Error, "Has a factor of 0, so every raw value means the same.");
        if (signal.minimum > signal.maximum)
            add(Severity::Error, QString("Minimum %1 is larger than maximum %2.")
                                     .arg(number(signal.minimum), number(signal.maximum)));
        else if (signal.initialValue < signal.minimum || signal.initialValue > signal.maximum)
            add(Severity::Warning, QString("Init. value %1 is outside minimum %2 ... maximum %3.")
                                       .arg(number(signal.initialValue), number(signal.minimum),
                                            number(signal.maximum)));

        bool placed = false;
        for (const Message &message : database.allMessages()) {
            for (const SignalMapping &mapping : message.signalMappings)
                placed = placed || mapping.signalId == signal.id;
        }
        if (!placed)
            add(Severity::Warning, "Is not placed in any message.");

        // Every value of the value table must be a raw value the signal can carry.
        const ValueTable *valueTable = database.findValueTable(signal.valueTableId);
        double rawMinimum = 0, rawMaximum = 0;
        if (valueTable && rawRange(signal, &rawMinimum, &rawMaximum)) {
            for (const ValueTable::Entry &entry : valueTable->entries) {
                if (entry.value < rawMinimum || entry.value > rawMaximum) {
                    add(Severity::Warning, QString("Value table '%1' has the value %2, which the %3-bit %4 "
                                                   "signal cannot carry (%5 ... %6).")
                                               .arg(valueTable->name).arg(entry.value).arg(signal.length)
                                               .arg(toString(signal.valueType).toLower(),
                                                    number(rawMinimum), number(rawMaximum)));
                }
            }
        }
    }
}

void checkNodes(const Database &database, QVector<Issue> &issues)
{
    for (const Node &node : database.allNodes()) {
        if (database.messagesTransmittedBy(node.id).isEmpty() && database.messagesReceivedBy(node.id).isEmpty())
            issues.append({Severity::Warning, "Network Node", node.name, "Neither transmits nor receives any message."});
    }
}

void checkValueTables(const Database &database, QVector<Issue> &issues)
{
    for (const ValueTable &valueTable : database.allValueTables()) {
        if (valueTable.entries.isEmpty())
            issues.append({Severity::Warning, "Value Table", valueTable.name, "Has no values."});
        if (database.signalsUsingValueTable(valueTable.id).isEmpty())
            issues.append({Severity::Warning, "Value Table", valueTable.name, "Is not used by any signal."});
    }
}

} // namespace

QVector<Issue> check(const Database &database)
{
    QVector<Issue> issues;
    checkMessages(database, issues);
    checkSignals(database, issues);
    checkNodes(database, issues);
    checkValueTables(database, issues);

    // Errors first; stable_sort keeps the order within each group.
    std::stable_sort(issues.begin(), issues.end(), [](const Issue &a, const Issue &b) {
        return a.severity == Severity::Error && b.severity != Severity::Error;
    });
    return issues;
}

} // namespace ConsistencyChecker
