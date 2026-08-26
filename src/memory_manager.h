#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QMap>

// Shared memory structure (must match DLL side)
struct LuaSharedMemory {
    static const int MAX_CODE = 16384;
    static const int MAX_OUTPUT = 32768;

    volatile LONG command;      // 0=idle, 1=execute, 2=exit
    volatile LONG done;         // 0=pending, 1=complete
    volatile LONG error;        // 0=ok, 1=error
    volatile LONG cancel;       // 1 = abort the running script (Stop button)
    volatile LONG outLen;       // live write cursor for streaming partial output
    char code[MAX_CODE];
    char output[MAX_OUTPUT];
};

struct ModuleInfo {
    QString name;
    QString fullPath;
    quint64 baseAddress = 0;
    quint64 size = 0;
};

struct PatchRecord {
    quint64 address;
    QByteArray originalBytes;
    QByteArray patchBytes;
};

class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    bool attach(const QString& processName);
    void detach();
    bool isAttached() const { return m_process != nullptr; }
    DWORD processId() const { return m_processId; }
    QString processName() const { return m_processName; }

    static bool isProcessRunning(const QString& processName);
    static DWORD getProcessIdByName(const QString& processName);
    static DWORD findProcessWithModule(const QStringList& moduleNames);

    ModuleInfo getModuleInfo(const QString& moduleName) const;
    QList<ModuleInfo> listModules() const;
    bool hasModule(const QString& moduleName) const;
    QStringList dumpModuleExports(const QString& moduleName) const;

    quint64 resolvePointer(quint64 baseAddress, const QList<qint64>& offsets) const;

    float readFloat(quint64 address) const;
    double readDouble(quint64 address) const;
    qint32 readInt(quint64 address) const;
    quint32 readUInt(quint64 address) const;
    qint16 readShort(quint64 address) const;
    quint8 readByte(quint64 address) const;
    QByteArray readBytes(quint64 address, size_t size) const;

    bool writeFloat(quint64 address, float value);
    bool writeDouble(quint64 address, double value);
    bool writeInt(quint64 address, qint32 value);
    bool writeUInt(quint64 address, quint32 value);
    bool writeShort(quint64 address, qint16 value);
    bool writeByte(quint64 address, quint8 value);
    bool writeBytes(quint64 address, const QByteArray& data);

    quint64 allocateRemote(size_t size);
    bool freeRemote(quint64 address);

    QList<quint64> aobScan(const QString& moduleName, const QByteArray& pattern, const QByteArray& mask = QByteArray()) const;
    quint64 aobScanSingle(const QString& moduleName, const QByteArray& pattern, const QByteArray& mask = QByteArray()) const;

    bool lookupAobCache(const QString& moduleName, const QByteArray& pattern, const QByteArray& mask, quint64& result) const;
    void cacheAobScan(const QString& moduleName, const QByteArray& pattern, const QByteArray& mask, quint64 address);
    void clearAobCache();
    void preScanAllAobs(const QList<QPair<QString, QPair<QByteArray, QByteArray>>>& scans);

    bool backupAndPatch(quint64 address, const QByteArray& patchBytes);
    bool restorePatch(quint64 address);
    bool isPatched(quint64 address) const { return m_patches.contains(address); }
    void restoreAllPatches();

    quint64 parseAddress(const QString& addressStr) const;
    quint64 resolveExport(const QString& moduleName, const QString& symbolName) const;
    quint64 resolvePdbSymbol(const QString& moduleName, const QString& symbolName) const;

    HWND findMainWindow(DWORD procId) const;
    bool setWindowTopMost(HWND hwnd, bool topMost);

    // DLL injection
    bool injectDll(const QString& dllPath);
    bool injectDllFromResource(const QString& dllPath); // writes to temp then injects

    // Call an exported function in an already-injected DLL
    bool callDllFunction(const QString& dllPath, int ordinal);

    // Shared memory for DLL communication
    bool initSharedMemory();
    void cleanupSharedMemory();
    LuaSharedMemory* getSharedMemory() const { return m_pShared; }

    HANDLE handle() const { return m_process; }
    bool is32BitTarget() const { return m_is32Bit; }

private:
    HANDLE m_process = nullptr;
    DWORD m_processId = 0;
    QString m_processName;
    bool m_is32Bit = false;
    QMap<quint64, PatchRecord> m_patches;
    mutable QMap<QByteArray, quint64> m_aobCache;
    mutable QMap<QString, ModuleInfo> m_moduleCache;

    // DLL injection shared memory
    HANDLE m_hMapFile = nullptr;
    LuaSharedMemory* m_pShared = nullptr;

    bool readRemoteMemory(quint64 address, void* buffer, size_t size) const;
    bool writeRemoteMemory(quint64 address, const void* buffer, size_t size);
    bool patchProtection(quint64 address, size_t size, DWORD newProtect, DWORD* oldProtect);
};
