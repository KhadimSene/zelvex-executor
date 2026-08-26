#pragma once

#include <keystone/keystone.h>
#include <QByteArray>
#include <QString>
#include <cstring>

class Assembler {
public:
    Assembler();
    ~Assembler();

    bool open(int arch = KS_ARCH_X86, int mode = KS_MODE_32);
    void close();

    QByteArray assemble(const QString& asmCode, quint64 address = 0, QString* errorOut = nullptr);

    bool isOpen() const { return m_ks != nullptr; }
    QString lastError() const { return m_lastError; }

    static QByteArray floatToBytes(float value) {
        QByteArray b(4, 0);
        memcpy(b.data(), &value, 4);
        return b;
    }

    static QByteArray int32ToBytes(qint32 value) {
        QByteArray b(4, 0);
        memcpy(b.data(), &value, 4);
        return b;
    }

    static QByteArray int16ToBytes(qint16 value) {
        QByteArray b(2, 0);
        memcpy(b.data(), &value, 2);
        return b;
    }

    static QByteArray int8ToBytes(qint8 value) {
        return QByteArray(1, static_cast<char>(value));
    }

    static quint32 floatToUint32(float value) {
        quint32 bits;
        memcpy(&bits, &value, 4);
        return bits;
    }

private:
    ks_engine* m_ks = nullptr;
    QString m_lastError;
};
