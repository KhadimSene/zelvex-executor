#include "assembler.h"

Assembler::Assembler() {}

Assembler::~Assembler() {
    close();
}

bool Assembler::open(int arch, int mode) {
    close();
    ks_err err = ks_open(static_cast<ks_arch>(arch), mode, &m_ks);
    if (err != KS_ERR_OK) {
        m_lastError = QString("ks_open failed: %1").arg(ks_strerror(err));
        m_ks = nullptr;
        return false;
    }
    return true;
}

void Assembler::close() {
    if (m_ks) {
        ks_close(m_ks);
        m_ks = nullptr;
    }
}

QByteArray Assembler::assemble(const QString& asmCode, quint64 address, QString* errorOut) {
    if (!m_ks) {
        if (!open()) {
            if (errorOut) *errorOut = m_lastError;
            return {};
        }
    }

    QByteArray codeBytes = asmCode.toUtf8();
    unsigned char* encode = nullptr;
    size_t size = 0;
    unsigned long long count = 0;

    int err = ks_asm(m_ks, codeBytes.constData(), address, &encode, &size, &count);
    if (err != 0) {
        m_lastError = QString("ks_asm failed: %1").arg(ks_strerror(ks_errno(m_ks)));
        if (errorOut) *errorOut = m_lastError;
        return {};
    }

    QByteArray result(reinterpret_cast<char*>(encode), static_cast<int>(size));
    ks_free(encode);
    return result;
}
