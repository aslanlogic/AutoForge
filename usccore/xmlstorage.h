#ifndef USCCORE_XMLSTORAGE_H
#define USCCORE_XMLSTORAGE_H

#include <QString>

namespace usc {

class Database;

// Reads and writes a Database as a UDScribe file (extension .uscxml), which is XML:
//
//   <?xml version="1.0" encoding="UTF-8"?>
//   <UDScribe version="1">
//     <Network protocol="can" requestId="0x7E0" responseId="0x7E8">
//       <Can bitrate="500000" samplePoint="87.5" idFormat="standard"/>  (only the active protocol's settings)
//     </Network>
//     <ValueTables>
//       <ValueTable name="OnOff">
//         <Value value="0" description="Off"/>
//         <Value value="1" description="On"/>
//       </ValueTable>
//     </ValueTables>
//     <DataTypes>
//       <DataType name="EngineSpeed" length="16" byteOrder="intel" valueType="unsigned"
//                 unit="rpm" factor="0.25" offset="0" minimum="0" maximum="16383.75" valueTable="...">
//         <Comment>Crankshaft speed</Comment>       (valueType: signed, unsigned, float, double,
//       </DataType>                                  ascii or bytes; valueTable is optional)
//     </DataTypes>
//     <DataIdentifiers>
//       <DataIdentifier name="TyrePressures" did="0x0100" length="4">   (length in bytes)
//         <DataType name="FL" dataType="Pressure" startBit="0"/>     (placements; the same
//         <DataType name="FR" dataType="Pressure" startBit="8"/>      data type may appear twice)
//         <ReadSession name="Extended"/>                (by session name; one per session)
//         <ReadSecurityLevel name="L1"/>                (by name; any of them, none = no security)
//         <WriteSession name="Extended"/>               (the same for writing; no WriteSession =
//         <WriteSecurityLevel name="L1"/>                not writable, no ReadSession = not readable)
//         <Comment>Pressure of all four tyres</Comment>
//       </DataIdentifier>
//     </DataIdentifiers>
//     <Sessions>                                     (always has the default session, 0x01)
//       <Session name="Default" sessionType="0x01" p2="50" p2Star="5000">
//         <Comment>Normal operation</Comment>
//       </Session>
//     </Sessions>
//     <SecurityLevels>
//       <SecurityLevel name="Extended" requestSeed="0x01" seedLength="4" keyLength="4" maxAttempts="3">
//         <AllowedSession name="Extended"/>         (by session name; one per session)
//         <Comment>Unlocks writing of calibration data</Comment>
//       </SecurityLevel>
//     </SecurityLevels>
//     <ResetTypes>
//       <ResetType name="Hard Reset" resetType="0x01">
//         <AllowedSession name="Extended"/>         (by session name; one per session)
//         <SecurityLevel name="Extended"/>          (by name; any of them allows it, none = no security)
//         <Comment>Restarts the ECU like a power cycle</Comment>
//       </ResetType>
//       <ResetType name="Rapid Shutdown" resetType="0x04" powerDownTime="10"/>  (s; only on 0x04, 255 = not available)
//     </ResetTypes>
//   </UDScribe>
//
// Ids are not saved; they are assigned again when the file is loaded.
namespace XmlStorage {

// On failure these return false and put a human-readable reason in *errorMessage.
bool save(const Database &database, const QString &filePath, QString *errorMessage);

// The database is only changed if the whole file was read successfully.
bool load(Database &database, const QString &filePath, QString *errorMessage);

} // namespace XmlStorage

} // namespace usc

#endif // USCCORE_XMLSTORAGE_H
