#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>

namespace Utils
{
    static DWORD FindProcessID() {
        const std::wstring processName = L"eldenring.exe";

        DWORD processID = 0;

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);

            if (Process32First(hSnapshot, &pe32)) {
                do {
                    if (processName == pe32.szExeFile) {
                        processID = pe32.th32ProcessID;
                        break;
                    }
                } while (Process32Next(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }
        return processID;
    }

    static HANDLE GetProcessHandle()
    {
        DWORD processID = FindProcessID();
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
        if (hProcess == NULL) {
            std::cerr << "Failed to open process. Error: " << GetLastError() << std::endl;
            return NULL;
        }

        BOOL isWow64 = FALSE;
        if (IsWow64Process(hProcess, &isWow64)) {
            if (!isWow64) {
                std::cout << "Target process is 64-bit." << std::endl;
            }
            else {
                std::cout << "Target process is 32-bit." << std::endl;
            }
        }
        else {
            std::cerr << "Failed to determine WOW64 status. Error: " << GetLastError() << std::endl;
        }

        return hProcess;
    }
}