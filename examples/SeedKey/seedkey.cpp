// The key algorithm of the example BodyECU (examples/BodyECU.uscxml), which UdpSim
// checks. It is a placeholder, not a secure algorithm; a real ECU has its own.
//
//     key[i] = seed[i mod seedLength] XOR 0x5A XOR i
//
// The same formula serves both levels (0x01 Extended, 0x11 Programming); a real
// algorithm often differs per level, which requestSeed tells apart.

#include "seedkey.h"

int computeKey(unsigned char requestSeed, const unsigned char *seed, int seedLength, unsigned char *key,
               int keyLength)
{
    if (requestSeed != 0x01 && requestSeed != 0x11)
        return 1; // not a level of the BodyECU
    if (seedLength <= 0)
        return 2;
    for (int i = 0; i < keyLength; ++i)
        key[i] = static_cast<unsigned char>(seed[i % seedLength] ^ 0x5A ^ i);
    return 0;
}
