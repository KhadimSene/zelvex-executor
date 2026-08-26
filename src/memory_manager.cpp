#include "memory_manager.h"
#include <QRegularExpression>
#include <dbghelp.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>

MemoryManager::MemoryManager() {}

MemoryManager::~MemoryManager() {
    detach();
}

bool MemoryManager::attach(const QString& processName) {
    detach();

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            QString name = QString::fromWCharArray(pe.szExeFile);
            if (name.compare(processName, Qt::CaseInsensitive) == 0) {
                m_processId = pe.th32ProcessID;
                m_processName = processName;
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    if (!found) return false;

    m_process = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE, m_processId);
    if (!m_process) {
        m_process = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
            PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD | PROCESS_SUSPEND_RESUME,
            FALSE, m_processId);
    }
    if (!m_process) {
        m_process = OpenProcess(
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            FALSE, m_processId);
    }

    if (!m_process) return false;

    // Detect if target is 32-bit
    BOOL isWow64 = FALSE;
    IsWow64Process(m_process, &isWow64);
    m_is32Bit = isWow64;

    return true;
}

void MemoryManager::detach() {
    restoreAllPatches();
    clearAobCache();
    m_moduleCache.clear();
    cleanupSharedMemory();
    if (m_process) {
        CloseHandle(m_process);
        m_process = nullptr;
    }
    m_processId = 0;
    m_processName.clear();
}

ModuleInfo MemoryManager::getModuleInfo(const QString& moduleName) const {
    if (!m_process) return ModuleInfo{};

    QString lookupKey = moduleName.toLower();
    if (m_moduleCache.contains(lookupKey))
        return m_moduleCache[lookupKey];

    ModuleInfo info{};
    HMODULE hMods[1024];
    DWORD cbNeeded;
    DWORD listFlags = m_is32Bit ? LIST_MODULES_32BIT : LIST_MODULES_64BIT;
    if (EnumProcessModulesEx(m_process, hMods, sizeof(hMods), &cbNeeded, listFlags)) {
        int count = cbNeeded / sizeof(HMODULE);
        wchar_t szModName[MAX_PATH];
        for (int i = 0; i < count; i++) {
            if (GetModuleFileNameExW(m_process, hMods[i], szModName, MAX_PATH)) {
                QString fullName = QString::fromWCharArray(szModName);
                QString baseName = fullName.section('\\', -1);
                MODULEINFO mi;
                if (GetModuleInformation(m_process, hMods[i], &mi, sizeof(mi))) {
                    ModuleInfo cachedInfo;
                    cachedInfo.name = baseName;
                    cachedInfo.fullPath = fullName;
                    cachedInfo.baseAddress = reinterpret_cast<quint64>(mi.lpBaseOfDll);
                    cachedInfo.size = mi.SizeOfImage;
                    m_moduleCache[baseName.toLower()] = cachedInfo;
                    if (baseName.compare(moduleName, Qt::CaseInsensitive) == 0) {
                        info = cachedInfo;
                    }
                }
            }
        }
    }
    return info;
}

QList<ModuleInfo> MemoryManager::listModules() const {
    QList<ModuleInfo> modules;
    if (!m_process) return modules;

    HMODULE hMods[1024];
    DWORD cbNeeded;
    DWORD listFlags = m_is32Bit ? LIST_MODULES_32BIT : LIST_MODULES_64BIT;
    if (EnumProcessModulesEx(m_process, hMods, sizeof(hMods), &cbNeeded, listFlags)) {
        int count = cbNeeded / sizeof(HMODULE);
        wchar_t szModName[MAX_PATH];
        for (int i = 0; i < count; i++) {
            if (GetModuleFileNameExW(m_process, hMods[i], szModName, MAX_PATH)) {
                MODULEINFO mi;
                if (GetModuleInformation(m_process, hMods[i], &mi, sizeof(mi))) {
                    ModuleInfo info;
                    QString fullPath = QString::fromWCharArray(szModName);
                    info.name = fullPath.section('\\', -1);
                    info.fullPath = fullPath;
                    info.baseAddress = reinterpret_cast<quint64>(mi.lpBaseOfDll);
                    info.size = mi.SizeOfImage;
                    modules.append(info);
                }
            }
        }
    }
    return modules;
}

quint64 MemoryManager::resolvePointer(quint64 baseAddress, const QList<qint64>& offsets) const {
    if (offsets.isEmpty()) return baseAddress;

    QList<qint64> revOffsets = offsets;
    std::reverse(revOffsets.begin(), revOffsets.end());

    quint64 addr = 0;
    if (m_is32Bit) {
        quint32 ptr32 = 0;
        if (!readRemoteMemory(baseAddress, &ptr32, sizeof(ptr32)))
            return 0;
        addr = ptr32;
    } else {
        if (!readRemoteMemory(baseAddress, &addr, sizeof(addr)))
            return 0;
    }

    for (int i = 0; i < revOffsets.size() - 1; i++) {
        if (addr == 0) return 0;
        if (m_is32Bit) {
            quint32 ptr32 = 0;
            if (!readRemoteMemory(addr + revOffsets[i], &ptr32, sizeof(ptr32)))
                return 0;
            addr = ptr32;
        } else {
            quint64 nextAddr = 0;
            if (!readRemoteMemory(addr + revOffsets[i], &nextAddr, sizeof(nextAddr)))
                return 0;
            addr = nextAddr;
        }
    }

    return addr + revOffsets.last();
}

bool MemoryManager::readRemoteMemory(quint64 address, void* buffer, size_t size) const {
    if (!m_process) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(m_process, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead)
           && bytesRead == size;
}

bool MemoryManager::writeRemoteMemory(quint64 address, const void* buffer, size_t size) {
    if (!m_process) return false;
    SIZE_T written = 0;
    return WriteProcessMemory(m_process, reinterpret_cast<LPVOID>(address), buffer, size, &written)
           && written == size;
}

float MemoryManager::readFloat(quint64 address) const {
    float val = 0;
    readRemoteMemory(address, &val, sizeof(val));
    return val;
}

double MemoryManager::readDouble(quint64 address) const {
    double val = 0;
    readRemoteMemory(address, &val, sizeof(val));
    return val;
}

qint32 MemoryManager::readInt(quint64 address) const {
    qint32 val = 0;
    readRemoteMemory(address, &val, sizeof(val));
    return val;
}

quint32 MemoryManager::readUInt(quint64 address) const {
    quint32 val = 0;
    readRemoteMemory(address, &val, sizeof(val));
    return val;
}

qint16 MemoryManager::readShort(quint64 address) const {
    qint16 val = 0;
    readRemoteMemory(address, &val, sizeof(val));
    return val;
}

quint8 MemoryManager::readByte(quint64 address) const {
    quint8 val = 0;
    readRemoteMemory(address, &val, sizeof(val));
    return val;
}

QByteArray MemoryManager::readBytes(quint64 address, size_t size) const {
    QByteArray buf(size, 0);
    readRemoteMemory(address, buf.data(), size);
    return buf;
}

bool MemoryManager::writeFloat(quint64 address, float value) {
    return writeRemoteMemory(address, &value, sizeof(value));
}

bool MemoryManager::writeDouble(quint64 address, double value) {
    return writeRemoteMemory(address, &value, sizeof(value));
}

bool MemoryManager::writeInt(quint64 address, qint32 value) {
    return writeRemoteMemory(address, &value, sizeof(value));
}

bool MemoryManager::writeUInt(quint64 address, quint32 value) {
    return writeRemoteMemory(address, &value, sizeof(value));
}

bool MemoryManager::writeShort(quint64 address, qint16 value) {
    return writeRemoteMemory(address, &value, sizeof(value));
}

bool MemoryManager::writeByte(quint64 address, quint8 value) {
    return writeRemoteMemory(address, &value, sizeof(value));
}

bool MemoryManager::writeBytes(quint64 address, const QByteArray& data) {
    return writeRemoteMemory(address, data.constData(), data.size());
}

quint64 MemoryManager::allocateRemote(size_t size) {
    if (!m_process) return 0;
    LPVOID addr = VirtualAllocEx(m_process, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    return reinterpret_cast<quint64>(addr);
}

bool MemoryManager::freeRemote(quint64 address) {
    if (!m_process) return false;
    return VirtualFreeEx(m_process, reinterpret_cast<LPVOID>(address), 0, MEM_RELEASE) != 0;
}

QList<quint64> MemoryManager::aobScan(const QString& moduleName, const QByteArray& pattern, const QByteArray& mask) const {
    QList<quint64> results;
    if (!m_process) return results;

    ModuleInfo mod = const_cast<MemoryManager*>(this)->getModuleInfo(moduleName);
    if (mod.baseAddress == 0) return results;

    QByteArray actualMask = mask;
    if (actualMask.isEmpty()) {
        actualMask = QByteArray(pattern.size(), '\xff');
        for (int i = 0; i < pattern.size(); i++) {
            if (static_cast<quint8>(pattern[i]) == 0x00)
                actualMask[i] = '\x00';
        }
    }

    const size_t chunkSize = 4096;
    for (quint64 offset = 0; offset < mod.size; offset += chunkSize - pattern.size() + 1) {
        size_t readSize = qMin(static_cast<size_t>(chunkSize), static_cast<size_t>(mod.size - offset));
        QByteArray memory = readBytes(mod.baseAddress + offset, readSize);
        if (memory.isEmpty()) continue;

        for (int i = 0; i <= memory.size() - pattern.size(); i++) {
            bool found = true;
            for (int j = 0; j < pattern.size(); j++) {
                if (actualMask[j] == '\xff' && memory[i + j] != pattern[j]) {
                    found = false;
                    break;
                }
            }
            if (found)
                results.append(mod.baseAddress + offset + i);
        }
    }
    return results;
}

quint64 MemoryManager::aobScanSingle(const QString& moduleName, const QByteArray& pattern, const QByteArray& mask) const {
    QByteArray key = moduleName.toUtf8() + '|' + pattern + '|' + mask;
    if (m_aobCache.contains(key))
        return m_aobCache.value(key);

    QList<quint64> results = aobScan(moduleName, pattern, mask);
    quint64 addr = results.isEmpty() ? 0 : results.first();
    if (addr != 0)
        m_aobCache[key] = addr;
    return addr;
}

bool MemoryManager::lookupAobCache(const QString& moduleName, const QByteArray& pattern, const QByteArray& mask, quint64& result) const {
    QByteArray key = moduleName.toUtf8() + '|' + pattern + '|' + mask;
    if (!m_aobCache.contains(key)) return false;
    result = m_aobCache.value(key);
    return true;
}

void MemoryManager::cacheAobScan(const QString& moduleName, const QByteArray& pattern, const QByteArray& mask, quint64 address) {
    QByteArray key = moduleName.toUtf8() + '|' + pattern + '|' + mask;
    m_aobCache[key] = address;
}

void MemoryManager::clearAobCache() {
    m_aobCache.clear();
}

void MemoryManager::preScanAllAobs(const QList<QPair<QString, QPair<QByteArray, QByteArray>>>& scans) {
    for (const auto& scan : scans) {
        quint64 addr = aobScanSingle(scan.first, scan.second.first, scan.second.second);
        Q_UNUSED(addr);
    }
}

bool MemoryManager::patchProtection(quint64 address, size_t size, DWORD newProtect, DWORD* oldProtect) {
    if (!m_process) return false;
    return VirtualProtectEx(m_process, reinterpret_cast<LPVOID>(address), size, newProtect, oldProtect) != 0;
}

bool MemoryManager::backupAndPatch(quint64 address, const QByteArray& patchBytes) {
    if (!m_process || m_patches.contains(address)) return false;

    DWORD oldProtect;
    if (!patchProtection(address, patchBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    QByteArray original = readBytes(address, patchBytes.size());
    PatchRecord record;
    record.address = address;
    record.originalBytes = original;
    record.patchBytes = patchBytes;

    bool ok = writeBytes(address, patchBytes);

    DWORD temp;
    patchProtection(address, patchBytes.size(), oldProtect, &temp);

    if (ok)
        m_patches[address] = record;

    return ok;
}

bool MemoryManager::restorePatch(quint64 address) {
    if (!m_process || !m_patches.contains(address)) return false;

    PatchRecord& record = m_patches[address];
    DWORD oldProtect;
    if (!patchProtection(address, record.originalBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    bool ok = writeBytes(address, record.originalBytes);

    DWORD temp;
    patchProtection(address, record.originalBytes.size(), oldProtect, &temp);

    if (ok) m_patches.remove(address);
    return ok;
}

void MemoryManager::restoreAllPatches() {
    QList<quint64> keys = m_patches.keys();
    for (quint64 addr : keys)
        restorePatch(addr);
}

quint64 MemoryManager::parseAddress(const QString& addressStr) const {
    // Format: "module+offset"
    static const QRegularExpression re("^([\\w\\.]+)\\+(\\w+)$");
    QRegularExpressionMatch match = re.match(addressStr);
    if (match.hasMatch()) {
        QString moduleName = match.captured(1);
        quint64 offset = match.captured(2).toULongLong(nullptr, 16);
        ModuleInfo mod = const_cast<MemoryManager*>(this)->getModuleInfo(moduleName);
        return mod.baseAddress + offset;
    }

    // Format: "module.symbolname" (no + sign)  - resolve via export/PDB
    static const QRegularExpression symRe("^([\\w\\.]+)\\.([\\w:]+)$");
    QRegularExpressionMatch symMatch = symRe.match(addressStr);
    if (symMatch.hasMatch()) {
        QString moduleName = symMatch.captured(1);
        QString symbolName = symMatch.captured(2);
        // Try with .dll extension first (most common), then without
        quint64 addr = resolveExport(moduleName + ".dll", symbolName);
        if (addr == 0) addr = resolvePdbSymbol(moduleName + ".dll", symbolName);
        if (addr == 0) addr = resolveExport(moduleName, symbolName);
        if (addr == 0) addr = resolvePdbSymbol(moduleName, symbolName);
        return addr;
    }

    // Raw hex address
    bool ok;
    quint64 addr = addressStr.toULongLong(&ok, 16);
    return ok ? addr : 0;
}

bool MemoryManager::isProcessRunning(const QString& processName) {
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (QString::fromWCharArray(pe.szExeFile).compare(processName, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

DWORD MemoryManager::getProcessIdByName(const QString& processName) {
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (QString::fromWCharArray(pe.szExeFile).compare(processName, Qt::CaseInsensitive) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

DWORD MemoryManager::findProcessWithModule(const QStringList& moduleNames) {
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    DWORD foundPid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            QString exe = QString::fromWCharArray(pe.szExeFile);
            HANDLE hProc = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                FALSE, pe.th32ProcessID);
            if (!hProc) continue;

            HMODULE hMods[1024];
            DWORD cbNeeded = 0;
            if (EnumProcessModulesEx(hProc, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_32BIT)) {
                int count = cbNeeded / sizeof(HMODULE);
                wchar_t szModName[MAX_PATH];
                for (int i = 0; i < count; i++) {
                    if (!GetModuleFileNameExW(hProc, hMods[i], szModName, MAX_PATH)) continue;
                    QString baseName = QString::fromWCharArray(szModName).section('\\', -1).toLower();
                    for (const QString& wanted : moduleNames) {
                        if (baseName == wanted.toLower()) {
                            foundPid = pe.th32ProcessID;
                            break;
                        }
                    }
                    if (foundPid) break;
                }
            }
            CloseHandle(hProc);
            if (foundPid) break;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return foundPid;
}

bool MemoryManager::hasModule(const QString& moduleName) const {
    if (!m_process) return false;
    return getModuleInfo(moduleName).baseAddress != 0;
}

quint64 MemoryManager::resolveExport(const QString& moduleName, const QString& symbolName) const {
    if (!m_process) return 0;

    QByteArray cacheKey = (moduleName + "!export!" + symbolName).toUtf8();
    if (m_aobCache.contains(cacheKey))
        return m_aobCache[cacheKey];

    quint64 modBase = getModuleInfo(moduleName).baseAddress;
    if (modBase == 0) return 0;

    IMAGE_DOS_HEADER dosHeader{};
    if (!readRemoteMemory(modBase, &dosHeader, sizeof(dosHeader))) return 0;
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) return 0;

    if (m_is32Bit) {
        IMAGE_NT_HEADERS32 ntHeaders{};
        if (!readRemoteMemory(modBase + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders))) return 0;
        if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) return 0;

        DWORD exportRva = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        DWORD exportSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        if (exportRva == 0) return 0;

        IMAGE_EXPORT_DIRECTORY exportDir{};
        if (!readRemoteMemory(modBase + exportRva, &exportDir, sizeof(exportDir))) return 0;

        QByteArray namePtrs(exportDir.NumberOfNames * sizeof(DWORD), 0);
        QByteArray ordinals(exportDir.NumberOfNames * sizeof(WORD), 0);
        QByteArray functions(exportDir.NumberOfFunctions * sizeof(DWORD), 0);

        readRemoteMemory(modBase + exportDir.AddressOfFunctions, functions.data(), functions.size());
        if (exportDir.AddressOfNames)
            readRemoteMemory(modBase + exportDir.AddressOfNames, namePtrs.data(), namePtrs.size());
        if (exportDir.AddressOfNameOrdinals)
            readRemoteMemory(modBase + exportDir.AddressOfNameOrdinals, ordinals.data(), ordinals.size());

        for (DWORD i = 0; i < exportDir.NumberOfNames; i++) {
            DWORD nameRva = 0;
            memcpy(&nameRva, namePtrs.data() + i * sizeof(DWORD), sizeof(DWORD));
            char nameBuf[256] = {};
            readRemoteMemory(modBase + nameRva, nameBuf, sizeof(nameBuf) - 1);
            QString foundName = QString::fromLatin1(nameBuf);

            if (foundName.contains(symbolName, Qt::CaseInsensitive)) {
                WORD ordinal = 0;
                memcpy(&ordinal, ordinals.data() + i * sizeof(WORD), sizeof(WORD));
                DWORD funcRva = 0;
                memcpy(&funcRva, functions.data() + ordinal * sizeof(DWORD), sizeof(DWORD));
                if (funcRva != 0)
                    return modBase + funcRva;
            }
        }
    } else {
        IMAGE_NT_HEADERS64 ntHeaders{};
        if (!readRemoteMemory(modBase + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders))) return 0;
        if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) return 0;

        DWORD exportRva = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        if (exportRva == 0) return 0;

        IMAGE_EXPORT_DIRECTORY exportDir{};
        if (!readRemoteMemory(modBase + exportRva, &exportDir, sizeof(exportDir))) return 0;

        QByteArray namePtrs(exportDir.NumberOfNames * sizeof(DWORD), 0);
        QByteArray ordinals(exportDir.NumberOfNames * sizeof(WORD), 0);
        QByteArray functions(exportDir.NumberOfFunctions * sizeof(DWORD), 0);

        readRemoteMemory(modBase + exportDir.AddressOfFunctions, functions.data(), functions.size());
        if (exportDir.AddressOfNames)
            readRemoteMemory(modBase + exportDir.AddressOfNames, namePtrs.data(), namePtrs.size());
        if (exportDir.AddressOfNameOrdinals)
            readRemoteMemory(modBase + exportDir.AddressOfNameOrdinals, ordinals.data(), ordinals.size());

        for (DWORD i = 0; i < exportDir.NumberOfNames; i++) {
            DWORD nameRva = 0;
            memcpy(&nameRva, namePtrs.data() + i * sizeof(DWORD), sizeof(DWORD));
            char nameBuf[256] = {};
            readRemoteMemory(modBase + nameRva, nameBuf, sizeof(nameBuf) - 1);
            QString foundName = QString::fromLatin1(nameBuf);

            if (foundName.contains(symbolName, Qt::CaseInsensitive)) {
                WORD ordinal = 0;
                memcpy(&ordinal, ordinals.data() + i * sizeof(WORD), sizeof(WORD));
                DWORD funcRva = 0;
                memcpy(&funcRva, functions.data() + ordinal * sizeof(DWORD), sizeof(DWORD));
                if (funcRva != 0)
                    return modBase + funcRva;
            }
        }
    }

    return 0;
}

quint64 MemoryManager::resolvePdbSymbol(const QString& moduleName, const QString& symbolName) const {
    if (!m_process) return 0;

    QByteArray cacheKey = (moduleName + "|" + symbolName).toUtf8();
    if (m_aobCache.contains(cacheKey))
        return m_aobCache[cacheKey];

    ModuleInfo info = getModuleInfo(moduleName);
    if (info.baseAddress == 0 || info.fullPath.isEmpty()) return 0;

    static bool dbghelpInit = false;
    if (!dbghelpInit) {
        SymInitialize(GetCurrentProcess(), NULL, TRUE);
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        dbghelpInit = true;
    }

    QByteArray pathBytes = info.fullPath.toLocal8Bit();
    DWORD64 base = SymLoadModule64(
        GetCurrentProcess(), NULL,
        pathBytes.constData(), NULL,
        info.baseAddress, static_cast<DWORD>(info.size)
    );
    if (base == 0) return 0;

    size_t bufSize = sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME;
    PIMAGEHLP_SYMBOL64 sym = reinterpret_cast<PIMAGEHLP_SYMBOL64>(malloc(bufSize));
    memset(sym, 0, bufSize);
    sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    sym->MaxNameLength = MAX_SYM_NAME;

    QByteArray nameBytes = symbolName.toLocal8Bit();
    quint64 result = 0;
    if (SymGetSymFromName64(GetCurrentProcess(), nameBytes.constData(), sym)) {
        result = sym->Address;
    }
    free(sym);
    if (result != 0)
        m_aobCache[cacheKey] = result;
    return result;
}

QStringList MemoryManager::dumpModuleExports(const QString& moduleName) const {
    QStringList exports;
    if (!m_process) return exports;

    quint64 modBase = getModuleInfo(moduleName).baseAddress;
    if (modBase == 0) return exports;

    IMAGE_DOS_HEADER dosHeader{};
    if (!readRemoteMemory(modBase, &dosHeader, sizeof(dosHeader))) return exports;
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) return exports;

    auto readExportNames = [&](auto& ntHeaders) -> QStringList {
        DWORD exportRva = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        DWORD exportSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        if (exportRva == 0) return exports;

        IMAGE_EXPORT_DIRECTORY exportDir{};
        if (!readRemoteMemory(modBase + exportRva, &exportDir, sizeof(exportDir))) return exports;

        QByteArray namePtrs(exportDir.NumberOfNames * sizeof(DWORD), 0);
        QByteArray ordinals(exportDir.NumberOfNames * sizeof(WORD), 0);

        if (exportDir.AddressOfNames)
            readRemoteMemory(modBase + exportDir.AddressOfNames, namePtrs.data(), namePtrs.size());
        if (exportDir.AddressOfNameOrdinals)
            readRemoteMemory(modBase + exportDir.AddressOfNameOrdinals, ordinals.data(), ordinals.size());

        QStringList result;
        for (DWORD i = 0; i < exportDir.NumberOfNames; i++) {
            DWORD nameRva = 0;
            memcpy(&nameRva, namePtrs.data() + i * sizeof(DWORD), sizeof(DWORD));
            char nameBuf[512] = {};
            readRemoteMemory(modBase + nameRva, nameBuf, sizeof(nameBuf) - 1);
            WORD ordinal = 0;
            memcpy(&ordinal, ordinals.data() + i * sizeof(WORD), sizeof(WORD));
            result.append(QString::fromLatin1(nameBuf));
        }
        return result;
    };

    if (m_is32Bit) {
        IMAGE_NT_HEADERS32 ntHeaders{};
        if (!readRemoteMemory(modBase + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders))) return exports;
        if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) return exports;
        exports = readExportNames(ntHeaders);
    } else {
        IMAGE_NT_HEADERS64 ntHeaders{};
        if (!readRemoteMemory(modBase + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders))) return exports;
        if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) return exports;
        exports = readExportNames(ntHeaders);
    }
    return exports;
}

HWND MemoryManager::findMainWindow(DWORD procId) const {
    struct EnumData { DWORD procId; HWND hwnd; };
    EnumData data = { procId, nullptr };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* data = reinterpret_cast<EnumData*>(lParam);
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);
        if (windowPid == data->procId && IsWindowVisible(hwnd)) {
            LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
            if (!(exStyle & WS_EX_TOOLWINDOW)) {
                data->hwnd = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&data));

    return data.hwnd;
}

bool MemoryManager::setWindowTopMost(HWND hwnd, bool topMost) {
    if (!hwnd) return false;
    return SetWindowPos(hwnd, topMost ? HWND_TOPMOST : HWND_NOTOPMOST,
                        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) != 0;
}

// ── DLL Injection ──

bool MemoryManager::injectDll(const QString& dllPath) {
    if (!m_process) return false;

    // Copy DLL to TEMP to avoid path/permission issues
    QString tempPath = QDir::tempPath() + "/zl_" + QFileInfo(dllPath).fileName();
    QFile::remove(tempPath);
    if (!QFile::copy(dllPath, tempPath)) {
        tempPath = dllPath;
    }

    std::wstring widePath = tempPath.toStdWString();

    // Allocate memory in target for DLL path
    SIZE_T pathSize = (widePath.size() + 1) * sizeof(wchar_t);
    LPVOID pRemotePath = VirtualAllocEx(m_process, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemotePath) return false;

    if (!WriteProcessMemory(m_process, pRemotePath, widePath.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(m_process, pRemotePath, 0, MEM_RELEASE);
        return false;
    }

    // Resolve LoadLibraryW from the TARGET process's own kernel32 export table.
    // Critical: Zelvex is 64-bit, the game is 32-bit (WoW64). GetProcAddress on
    // our local 64-bit kernel32 returns a 64-bit address which would be truncated
    // to garbage when CreateRemoteThread runs it in the 32-bit game.
    quint64 loadLibraryAddr = resolveExport("kernel32.dll", "LoadLibraryW");
    if (loadLibraryAddr == 0) {
        VirtualFreeEx(m_process, pRemotePath, 0, MEM_RELEASE);
        return false;
    }

    // Try CreateRemoteThread first
    HANDLE hThread = CreateRemoteThread(m_process, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryAddr), pRemotePath, 0, nullptr);

    // If that fails, try NtCreateThreadEx (bypasses some anti-cheat)
    if (!hThread) {
        typedef NTSTATUS(NTAPI *NtCreateThreadExFn)(
            PHANDLE, ACCESS_MASK, LPVOID, HANDLE, LPTHREAD_START_ROUTINE,
            LPVOID, BOOL, SIZE_T, SIZE_T, SIZE_T, LPVOID);

        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            auto NtCreateThreadEx = (NtCreateThreadExFn)GetProcAddress(hNtdll, "NtCreateThreadEx");
            if (NtCreateThreadEx) {
                NtCreateThreadEx(&hThread, THREAD_ALL_ACCESS, nullptr, m_process,
                    reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryAddr), pRemotePath, FALSE, 0, 0, 0, nullptr);
            }
        }
    }

    if (!hThread) {
        VirtualFreeEx(m_process, pRemotePath, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, 10000);

    DWORD loadResult = 0;
    GetExitCodeThread(hThread, &loadResult);
    CloseHandle(hThread);
    VirtualFreeEx(m_process, pRemotePath, 0, MEM_RELEASE);

    return (loadResult != 0);
}

bool MemoryManager::initSharedMemory() {
    if (m_pShared) return true;

    m_hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
        0, sizeof(LuaSharedMemory), "ZelvexLuaSharedMem");

    if (!m_hMapFile) return false;

    m_pShared = (LuaSharedMemory*)MapViewOfFile(
        m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LuaSharedMemory));

    if (!m_pShared) {
        CloseHandle(m_hMapFile);
        m_hMapFile = nullptr;
        return false;
    }

    memset(m_pShared, 0, sizeof(LuaSharedMemory));
    return true;
}

void MemoryManager::cleanupSharedMemory() {
    if (m_pShared) { UnmapViewOfFile(m_pShared); m_pShared = nullptr; }
    if (m_hMapFile) { CloseHandle(m_hMapFile); m_hMapFile = nullptr; }
}

// ── PE Parsing helpers (read DLL from disk, no LoadLibrary needed) ──

static int RvaToFileOffset(const BYTE* data, DWORD rva) {
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)data;
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(data + dos->e_lfanew);

    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, section++) {
        if (rva >= section->VirtualAddress &&
            rva < section->VirtualAddress + section->Misc.VirtualSize) {
            return (int)(rva - section->VirtualAddress + section->PointerToRawData);
        }
    }
    return -1;
}

static int GetExportOffsetFromFile(const QString& dllPath, int ordinal) {
    QFile f(dllPath);
    if (!f.open(QIODevice::ReadOnly)) return -1;
    QByteArray peData = f.readAll();
    f.close();

    const BYTE* p = (const BYTE*)peData.constData();
    if ((int)peData.size() < (int)sizeof(IMAGE_DOS_HEADER)) return -1;

    IMAGE_DOS_HEADER* pDOS = (IMAGE_DOS_HEADER*)p;
    if (pDOS->e_magic != IMAGE_DOS_SIGNATURE) return -1;

    if ((DWORD)pDOS->e_lfanew + sizeof(IMAGE_NT_HEADERS32) > (DWORD)peData.size()) return -1;
    IMAGE_NT_HEADERS32* pNT = (IMAGE_NT_HEADERS32*)(p + pDOS->e_lfanew);
    if (pNT->Signature != IMAGE_NT_SIGNATURE) return -1;

    DWORD exportRva = pNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!exportRva) return -1;

    int exportOffset = RvaToFileOffset(p, exportRva);
    if (exportOffset < 0) return -1;

    IMAGE_EXPORT_DIRECTORY* pExport = (IMAGE_EXPORT_DIRECTORY*)(p + exportOffset);

    DWORD funcTableRva = pExport->AddressOfFunctions;
    int funcTableOffset = RvaToFileOffset(p, funcTableRva);
    if (funcTableOffset < 0) return -1;

    DWORD* pFunctions = (DWORD*)(p + funcTableOffset);

    // For ordinal exports, the ordinal - base gives the index into AddressOfFunctions
    DWORD index = ordinal - pExport->Base;
    if (index >= pExport->NumberOfFunctions) return -1;

    return pFunctions[index];
}

bool MemoryManager::callDllFunction(const QString& dllPath, int ordinal) {
    if (!m_process) return false;

    FILE* flog = nullptr;
    fopen_s(&flog, "C:\\Users\\voidcpp\\Documents\\MiniWorld MOD\\inject_log.txt", "a");
    auto log = [&](const char* msg) { if (flog) { fprintf(flog, "%s\n", msg); fflush(flog); } };

    // Get function offset from DLL file (cross-architecture)
    int offset = GetExportOffsetFromFile(dllPath, ordinal);
    if (offset < 0) { log("GetExportOffsetFromFile failed"); if (flog) fclose(flog); return false; }
    char buf[256];
    snprintf(buf, sizeof(buf), "Export offset: 0x%X", offset);
    log(buf);

    // Find the DLL base in the remote process
    HMODULE hRemoteMods[1024];
    DWORD cbNeeded;
    if (!EnumProcessModulesEx(m_process, hRemoteMods, sizeof(hRemoteMods), &cbNeeded, LIST_MODULES_ALL)) {
        log("EnumProcessModulesEx failed");
        if (flog) fclose(flog);
        return false;
    }

    std::wstring dllName = QFileInfo(dllPath).fileName().toStdWString();
    char nameBuf[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, dllName.c_str(), -1, nameBuf, MAX_PATH, nullptr, nullptr);
    snprintf(buf, sizeof(buf), "Looking for DLL: %s", nameBuf);
    log(buf);

    HMODULE hRemoteMod = nullptr;
    for (unsigned int i = 0; i < cbNeeded / sizeof(HMODULE); i++) {
        wchar_t modName[MAX_PATH];
        if (GetModuleFileNameExW(m_process, hRemoteMods[i], modName, MAX_PATH)) {
            std::wstring modPath(modName);
            if (modPath.find(dllName) != std::wstring::npos) {
                hRemoteMod = hRemoteMods[i];
                break;
            }
        }
    }

    if (!hRemoteMod) { log("DLL not found in remote process"); if (flog) fclose(flog); return false; }
    log("DLL found in remote process");

    DWORD_PTR remoteFunc = (DWORD_PTR)hRemoteMod + offset;
    HANDLE hThread = CreateRemoteThread(m_process, nullptr, 0,
        (LPTHREAD_START_ROUTINE)remoteFunc, nullptr, 0, nullptr);
    if (!hThread) {
        snprintf(buf, sizeof(buf), "CreateRemoteThread failed, error=%lu", GetLastError());
        log(buf);
        if (flog) fclose(flog);
        return false;
    }

    WaitForSingleObject(hThread, 10000);
    CloseHandle(hThread);
    log("Remote thread completed");
    if (flog) fclose(flog);
    return true;
}
