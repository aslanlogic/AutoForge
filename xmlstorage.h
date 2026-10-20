#ifndef XMLSTORAGE_H
#define XMLSTORAGE_H

#include <QString>

class Database;

// Reads and writes a Database as a FrameForge XML file:
//
//   <?xml version="1.0" encoding="UTF-8"?>
//   <FrameForge version="1">
//     <Signals>
//       <Signal name="EngineSpeed" length="16" byteOrder="motorola" valueType="unsigned"
//               unit="rpm" initialValue="0" factor="0.25" offset="0" minimum="0" maximum="16383.75">
//         <Comment>Crankshaft speed</Comment>
//       </Signal>
//     </Signals>
//     <Messages>
//       <Message name="EngineData" type="can-standard" frameId="0x100" length="8"
//                txMethod="cyclic" cycleTime="100">
//         <SignalRef name="EngineSpeed" startBit="7"/>
//         <Comment>Engine status</Comment>
//       </Message>
//     </Messages>
//   </FrameForge>
//
// Signal and message ids are not saved; they are assigned again when the file is loaded.
namespace XmlStorage {

// On failure these return false and put a human-readable reason in *errorMessage.
bool save(const Database &database, const QString &filePath, QString *errorMessage);

// The database is only changed if the whole file was read successfully.
bool load(Database &database, const QString &filePath, QString *errorMessage);

} // namespace XmlStorage

#endif // XMLSTORAGE_H
