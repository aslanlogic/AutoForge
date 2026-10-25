#ifndef DIAGPROBE_SEEDKEY_H
#define DIAGPROBE_SEEDKEY_H

/*
 * The interface of a DiagProbe key algorithm plugin: a shared library (.so on
 * Linux, .dll on Windows, .dylib on macOS) that computes the SecurityAccess (0x27)
 * key for a seed. Key algorithms are tester secrets, so they are not part of the
 * .uscxml; select the library with File > Select Key Algorithm in DiagProbe.
 *
 * Plain C, so a plugin can be written in C or C++ without Qt. A C++ plugin includes
 * this header so the function gets C linkage. See examples/SeedKey for one.
 */

#ifdef _WIN32
#define SEEDKEY_EXPORT __declspec(dllexport)
#else
#define SEEDKEY_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Computes the key for a seed the ECU sent.
 *
 *   requestSeed  the security level's requestSeed sub-function (0x01, 0x11, ...), so
 *                one library can serve every level of an ECU
 *   seed         seedLength bytes, as received in the positive response 67 <requestSeed> <seed>
 *   key          room for keyLength bytes (the level's keyLength from the .uscxml); fill all of them
 *
 * Returns 0 on success, anything else if the library cannot compute the key (for
 * example an unknown level); DiagProbe then sends no key.
 */
SEEDKEY_EXPORT int computeKey(unsigned char requestSeed, const unsigned char *seed, int seedLength,
                              unsigned char *key, int keyLength);

#ifdef __cplusplus
}
#endif

#endif /* DIAGPROBE_SEEDKEY_H */
