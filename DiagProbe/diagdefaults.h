#ifndef DIAGDEFAULTS_H
#define DIAGDEFAULTS_H

#include <QByteArray>

// Values the diagnostic description does not (yet) contain, shared by the tester
// and the simulated ECU so both sides agree.

// S3server: without a request for this long the ECU falls back to the default session.
const int S3Time = 5000; // ms

// The tester sends TesterPresent (3E 80) this often while it is outside the default session.
const int TesterPresentInterval = 2000; // ms

// Added to P2 / P2* while the tester waits for a response, for the delay of the PC itself.
const int ResponseTimeMargin = 100; // ms

// Placeholder SecurityAccess key algorithm until DiagProbe can load the real one
// (it is a tester secret, so it is not part of the .uscxml):
//     key[i] = seed[i mod seedLength] XOR 0x5A XOR i
inline QByteArray computeKey(const QByteArray &seed, int keyLength)
{
    QByteArray key(keyLength, '\0');
    for (int i = 0; i < keyLength; ++i) {
        const quint8 seedByte = seed.isEmpty() ? 0 : quint8(seed[i % seed.size()]);
        key[i] = char(seedByte ^ 0x5A ^ quint8(i));
    }
    return key;
}

#endif // DIAGDEFAULTS_H
