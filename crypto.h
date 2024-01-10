#ifndef CRYPTO_H
#define CRYPTO_H

#include <QByteArray>

typedef quint8 Block[4][4];

class AES128
{

public:

    void init(const QByteArray &key, const QByteArray &iv);
    void cbcEncrypt(QByteArray &buffer);
    void cbcDecrypt(QByteArray &buffer);

private:

    quint8 m_roundKey[176], m_iv[16];

    void addRoundKey(Block *block, quint8 round);

    void replaceBytes(Block *block, bool invert = false);
    void shiftRows(Block *block, bool invert = false);
    void mixColumns(Block *block, bool invert = false);

    void encryptBlock(Block *block);
    void dercyptBlock(Block *block);

};

class DH
{

public:

    DH(void);

    inline quint32 prime(void) { return m_prime; }
    inline void setPrime(quint32 value) { m_prime = value; }

    inline quint32 generator(void) { return m_generator; }
    inline void setGenerator(quint32 value) { m_generator = value; }

    inline quint32 sharedKey(void) { return power(m_generator, m_seed, m_prime); }
    inline quint32 privateKey(quint32 key) { return power(key, m_seed, m_prime); }

private:

    quint32 m_seed, m_prime, m_generator;

    quint32 multiply(quint32 a, quint32 b, quint32 m);
    quint32 power(quint32 a, quint32 b, quint32 m);

};

#endif
