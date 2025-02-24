#include "MemoryUtility.h"
#include <vector>
#include <thread>
#include <chrono>
#include "Utils.h"
#include <Psapi.h>

const char* gameManPattern = "\x48\x8B\x05\x00\x00\x00\x00\x80\xB8\x00\x00\x00\x00\x0D\x0F\x94\xC0\xC3";
const char* gameManMask = "xxx????xx????xxxx";
const char* gameDataManPattern = "\x48\x8B\x0D\x00\x00\x00\x00\x48\x85\xC9\x74\x11\xB8";
const char* gameDataManMask = "xxx????xxxxxx";
const char* worldChrManPattern = "\x48\x8B\x05\x00\x00\x00\x00\x48\x85\xC0\x74\x0F\x48\x39\x88";
const char* worldChrManMask = "xxx????xxxxxxx";

DWORD_PTR MemoryUtility::gameManAddress = 0;
DWORD_PTR MemoryUtility::gameDataManAddress = 0;
DWORD_PTR MemoryUtility::worldChrManAddress = 0;

using namespace Utils;

int MemoryUtility::ReadInt32(DWORD_PTR address) {
    if (address == 0) {
        return static_cast<int>(0);
    }

    int value = static_cast<int>(0);
    __try {
        value = *(int*)address;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int>(0);
    }

    return value;
}

long long MemoryUtility::ReadInt64(DWORD_PTR address) {
    if (address == 0) {
        return static_cast<long long>(0);
    }

    long long value = static_cast<long long>(0);
    __try {
        value = *(long long*)address;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<long long>(0);
    }

    return value;
}


BYTE MemoryUtility::ReadByte(DWORD_PTR address) {
    if (address == 0) {
        return static_cast<BYTE>(0);
    }

    BYTE value = static_cast<BYTE>(0);
    __try {
        value = *(BYTE*)address;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<BYTE>(0);
    }

    return value;
}

std::string MemoryUtility::ReadString(DWORD_PTR address, int length) {
    if (address == 0) {
        return "";
    }
    if (!IsValidAddress(address)) {
        return "";
    }

    std::vector<wchar_t> buffer(length);
    for (int i = 0; i < length; ++i) {
        if (IsBadReadPtr((void*)(address + i * sizeof(wchar_t)), sizeof(wchar_t))) {
            return "";
        }
        buffer[i] = *(wchar_t*)(address + i * sizeof(wchar_t));
    }
    std::wstring wstr(buffer.begin(), buffer.end());
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::string MemoryUtility::ReadPlayerName(int playerIndex) {
    if (worldChrManAddress == 0) {
        return "";
    }

    std::string value = "";
    DWORD_PTR mainAddressValue = ReadInt64(worldChrManAddress);

    if (!IsValidAddress(mainAddressValue)) {
        return "";
    }
    mainAddressValue = ReadInt64(mainAddressValue + 0x10EF8);

    if (!IsValidAddress(mainAddressValue)) {
        return "";
    }
    mainAddressValue = ReadInt64(mainAddressValue + (playerIndex * 0x10));

    if (!IsValidAddress(mainAddressValue)) {
        return "";
    }
    mainAddressValue = ReadInt64(mainAddressValue + 0x580);

    if (!IsValidAddress(mainAddressValue)) {
        return "";
    }
    value = ReadString(mainAddressValue + 0x9C, 32);

    return value;
}


int MemoryUtility::CountNetPlayers() {
    int netPlayerCount = 0;
    int maxPlayers = 10;

    for (int i = 0; i <= maxPlayers; ++i) {
        std::string playerName = ReadPlayerName(i);
        if (playerName != "") {
            netPlayerCount++;
        }
        else {
            break;
        }
    }

    return netPlayerCount;
}

DWORD_PTR MemoryUtility::GetModuleBaseAddress()
{
    DWORD processID = FindProcessID();
    DWORD_PTR   baseAddress = 0;
    HANDLE      processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    HMODULE* moduleArray;
    LPBYTE      moduleArrayBytes;
    DWORD       bytesRequired;

    if (processHandle)
    {
        if (EnumProcessModules(processHandle, NULL, 0, &bytesRequired))
        {
            if (bytesRequired)
            {
                moduleArrayBytes = (LPBYTE)LocalAlloc(LPTR, bytesRequired);

                if (moduleArrayBytes)
                {
                    unsigned int moduleCount;

                    moduleCount = bytesRequired / sizeof(HMODULE);
                    moduleArray = (HMODULE*)moduleArrayBytes;

                    if (EnumProcessModules(processHandle, moduleArray, bytesRequired, &bytesRequired))
                    {
                        baseAddress = (DWORD_PTR)moduleArray[0];
                    }

                    LocalFree(moduleArrayBytes);
                }
            }
        }

        CloseHandle(processHandle);
    }

    return baseAddress;
}

DWORD_PTR MemoryUtility::FindPattern(DWORD_PTR base, DWORD size, const char* pattern, const char* mask) {
    DWORD patternLength = (DWORD)strlen(mask);
    for (DWORD i = 0; i < size - patternLength; i++) {
        bool found = true;
        for (DWORD j = 0; j < patternLength; j++) {
            if (mask[j] != '?' && pattern[j] != *(char*)(base + i + j)) {
                found = false;
                break;
            }
        }
        if (found) {
            return base + i;
        }
    }
    return 0;
}

DWORD_PTR MemoryUtility::CalculateAddress(HANDLE hProcess, DWORD_PTR base, const char* pattern, const char* mask, int patternOffset, int addressAdjustment) {
    MODULEINFO moduleInfo = { 0 };
    if (!GetModuleInformation(hProcess, reinterpret_cast<HMODULE>(base), &moduleInfo, sizeof(moduleInfo))) {
        std::cout << "GetModuleInformation failed." << std::endl;
        return 0;
    }
    DWORD moduleSize = moduleInfo.SizeOfImage;
    std::vector<byte> moduleMemory(moduleSize);
    ReadProcessMemory(hProcess, (LPCVOID)base, moduleMemory.data(), moduleSize, NULL);

    DWORD_PTR patternAddress = FindPattern((DWORD_PTR)moduleMemory.data(), moduleSize, pattern, mask);
    if (patternAddress == 0) {
        std::cout << "Pattern not found." << std::endl;
        return 0;
    }

    int offset = *(int*)(patternAddress + patternOffset);

    return base + (patternAddress - (DWORD_PTR)moduleMemory.data()) + offset + addressAdjustment;
}

long MemoryUtility::ReadLocalPlayerLevel() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x68);
}

long MemoryUtility::ReadPlayTime() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    return ReadInt32(mainAddressValue + 0xA0);
}

long MemoryUtility::ReadDeathCount() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    return ReadInt32(mainAddressValue + 0x94);
}

long MemoryUtility::ReadRunesHeld() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x6C);
}

long MemoryUtility::ReadLastGraceLocationId() {
    if (gameManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameManAddress);
    return ReadInt32(mainAddressValue + 0xB6C);
}

int MemoryUtility::ReadPlayerClassId() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0xBF);

    BYTE playerClass = ReadByte(mainAddressValue);

    return static_cast<int>(playerClass);
}

long MemoryUtility::ReadStrengthAttr() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x48);
}

long MemoryUtility::ReadDexterityAttr() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x4C);
}

long MemoryUtility::ReadIntelligenceAttr() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x50);
}

long MemoryUtility::ReadFaithAttr() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x54);
}

long MemoryUtility::ReadArcaneAttr() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x58);
}

long MemoryUtility::ReadVigorAttr() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x3C);
}

long MemoryUtility::ReadMindAttr() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x40);
}

long MemoryUtility::ReadEnduranceAttr() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x44);
}

long MemoryUtility::ReadLocalPlayerHealth() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x10);
}

long MemoryUtility::ReadLocalPlayerMana() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x1C);
}
long MemoryUtility::ReadLocalPlayerStamina() {
    if (gameDataManAddress == 0) {
        return 0;
    }
    DWORD_PTR mainAddressValue = ReadInt64(gameDataManAddress);
    mainAddressValue = ReadInt64(mainAddressValue + 0x08);
    return ReadInt32(mainAddressValue + 0x2C);
}


bool MemoryUtility::IsValidAddress(DWORD_PTR address) {
    if (address == 0) {
        return false;
    }

    __try {
        volatile char value = *(char*)address;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return true;
}

void MemoryUtility::initialize()
{
    std::string localPlayerName = "";
    do {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        DWORD_PTR moduleBase = GetModuleBaseAddress();
        std::cout << "Base Module: " << std::hex << moduleBase << std::endl;
        gameManAddress = CalculateAddress(GetProcessHandle(), moduleBase, gameManPattern, gameManMask, 3, 7);
        std::cout << "gameManAddress: " << std::hex << gameManAddress << std::endl;
        gameDataManAddress = CalculateAddress(GetProcessHandle(), moduleBase, gameDataManPattern, gameDataManMask, 3, 7);
        std::cout << "gameDataManAddress: " << std::hex << gameDataManAddress << std::endl;
        worldChrManAddress = CalculateAddress(GetProcessHandle(), moduleBase, worldChrManPattern, worldChrManMask, 3, 7);
        std::cout << "worldChrManAddress: " << std::hex << worldChrManAddress << std::endl;
    } while (!IsValidAddress(gameManAddress) || !IsValidAddress(gameDataManAddress) || !IsValidAddress(worldChrManAddress));

    do
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        localPlayerName = ReadPlayerName(0);

    } while (localPlayerName == "");

    std::cout << "MemoryUtility initialized" << std::endl;

}