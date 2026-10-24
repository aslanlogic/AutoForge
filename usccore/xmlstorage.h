#ifndef XMLSTORAGE_H
#define XMLSTORAGE_H

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

#endif // XMLSTORAGE_H
