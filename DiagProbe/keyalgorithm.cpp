#include "keyalgorithm.h"

bool KeyAlgorithm::load(const QString &filePath, QString *errorMessage)
{
    unload();
    m_library.setFileName(filePath);
    if (!m_library.load()) {
        *errorMessage = m_library.errorString();
        return false;
    }
    m_computeKey = reinterpret_cast<ComputeKey>(m_library.resolve("computeKey"));
    if (!m_computeKey) {
        *errorMessage = "The library has no computeKey() function (see DiagProbe/seedkey.h).";
        m_library.unload();
        return false;
    }
    m_filePath = filePath;
    return true;
}

void KeyAlgorithm::unload()
{
    m_computeKey = nullptr;
    m_filePath.clear();
    if (m_library.isLoaded())
        m_library.unload();
}

bool KeyAlgorithm::computeKey(quint8 requestSeed, const QByteArray &seed, int keyLength, QByteArray *key,
                              QString *errorMessage) const
{
    if (!m_computeKey) {
        *errorMessage = "No key algorithm selected (File → Select Key Algorithm).";
        return false;
    }
    key->fill('\0', keyLength);
    const int result = m_computeKey(requestSeed, reinterpret_cast<const unsigned char *>(seed.constData()),
                                    seed.size(), reinterpret_cast<unsigned char *>(key->data()), keyLength);
    if (result != 0) {
        *errorMessage = QString("The key algorithm could not compute a key (it returned %1).").arg(result);
        return false;
    }
    return true;
}
