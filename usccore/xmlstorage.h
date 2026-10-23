#ifndef XMLSTORAGE_H
#define XMLSTORAGE_H

#include <QString>

namespace usc {

class Database;

// Reads and writes a Database as a UDScribe file (extension .uscxml), which is XML:
//
//   <?xml version="1.0" encoding="UTF-8"?>
//   <UDScribe version="1">
//     <SecurityLevels>
//       <SecurityLevel name="Extended" requestSeed="0x01" seedLength="4" keyLength="4" maxAttempts="3">
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
