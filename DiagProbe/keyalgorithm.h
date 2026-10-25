#ifndef KEYALGORITHM_H
#define KEYALGORITHM_H

#include <QByteArray>
#include <QLibrary>
#include <QString>

// A key algorithm plugin (see seedkey.h), loaded from a shared library.
class KeyAlgorithm
{
public:
    // Loads the library and finds its computeKey(). On failure nothing is loaded
    // (a library loaded before stays unloaded too) and *errorMessage says why.
    bool load(const QString &filePath, QString *errorMessage);
    void unload();
    bool isLoaded() const { return m_computeKey != nullptr; }
    QString filePath() const { return m_filePath; }

    // The key for a seed of the level with this requestSeed; false and why if the library says no.
    bool computeKey(quint8 requestSeed, const QByteArray &seed, int keyLength, QByteArray *key,
                    QString *errorMessage) const;

private:
    using ComputeKey = int (*)(unsigned char, const unsigned char *, int, unsigned char *, int);

    QLibrary m_library;
    ComputeKey m_computeKey = nullptr;
    QString m_filePath;
};

#endif // KEYALGORITHM_H
