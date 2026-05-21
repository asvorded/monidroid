#define UNICODE
#define _UNICODE

#include <Windows.h>
#include <stdlib.h>

int wmain(int argc, WCHAR* argv[]) {
    if (argc != 2) return -1;

    DWORD targetPid = _wtoi(argv[1]);

    if (!AttachConsole(targetPid)) {
        return -1;
    }

    SetConsoleCtrlHandler(NULL, TRUE);
    GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
    FreeConsole();

    return 0;
}