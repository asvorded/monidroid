#define UNICODE
#define _UNICODE

#include <windows.h>
#include <tchar.h>
#include <PathCch.h>

#include "monidroid/logger.h"

struct ProcInfo {
    HANDLE serverToken = NULL;
    PROCESS_INFORMATION procInfo { };

    ~ProcInfo() {
        if (serverToken) CloseHandle(serverToken);
        if (procInfo.hProcess) CloseHandle(procInfo.hProcess);
        if (procInfo.hThread) CloseHandle(procInfo.hThread);
    }
};

constexpr auto SERVICE_NAME = TEXT(MD_SERVICE_NAME);

WCHAR exe[MAX_PATH] { };

SERVICE_STATUS          g_svcStatus;
SERVICE_STATUS_HANDLE   g_svcStatusHandle = NULL;
HANDLE                  g_stopEvent = NULL;

DWORD WINAPI SvcCtrlHandler(DWORD, DWORD, LPVOID, LPVOID);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

void ReportSvcStatus(DWORD, DWORD, DWORD);
void SvcReportEvent(LPTSTR);

template <size_t Size>
static void GetExePath(WCHAR (&str)[Size], const WCHAR* exeName) {
    GetModuleFileNameW(nullptr, str, Size);
    PathCchRemoveFileSpec(str, Size);
    wcscat_s(str, L"\\");
    wcscat_s(str, exeName);
}

int wmain(int argc, WCHAR *argv[]) {
    if (argc > 2) {
        return -1;
    }

    GetExePath(exe, TEXT(MD_SERVER_EXE_NAME));
    if (argc == 2) {
        (void)_wfreopen(argv[1], L"w", stdout);
    }

    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { (TCHAR*)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(DispatchTable)) {
        Monidroid::TaggedLog(SERVICE_NAME, TEXT("StartServiceCtrlDispatcher() failed, error code {}"), GetLastError());
    }
}

DWORD LaunchServerInSession(DWORD sessionId, ProcInfo *procInfo) {
    HANDLE thisToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE, &thisToken)) {
        return GetLastError();
    }

    // Duplicate our own LocalSystem token
    HANDLE serverToken;
    if (!DuplicateTokenEx(thisToken, TOKEN_ALL_ACCESS, nullptr, SecurityImpersonation, TokenPrimary, &serverToken)) {
        CloseHandle(thisToken);
        return GetLastError();
    }
    CloseHandle(thisToken);

    if (!SetTokenInformation(serverToken, TokenSessionId, &sessionId, sizeof(sessionId))) {
        CloseHandle(serverToken);
        return GetLastError();
    }

    STARTUPINFO startup {
        .cb = sizeof(startup),
        .lpDesktop = (LPTSTR)TEXT("Winsta0\\Default"),
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdInput = GetStdHandle(STD_INPUT_HANDLE),
        .hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE),
        .hStdError = GetStdHandle(STD_ERROR_HANDLE),
    };

    TCHAR cmd[MAX_PATH];
    wcscpy_s(cmd, exe);

    if (!CreateProcessAsUser(serverToken, nullptr, cmd, nullptr, nullptr,
        TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        nullptr, nullptr, &startup, &procInfo->procInfo)) {
        CloseHandle(serverToken);
        return GetLastError();
    }

    procInfo->serverToken = serverToken;
    return NO_ERROR;
}

bool TryStopServer(const ProcInfo &procInfo) {
    STARTUPINFO startup {
        .cb = sizeof(startup),
        .lpDesktop = (LPTSTR)TEXT("Winsta0\\Default"),
    };

    WCHAR cmd[MAX_PATH];
    GetExePath(cmd, TEXT(MD_TERMINATOR_EXE_NAME));
    wcscat_s(cmd, L" ");
    wcscat_s(cmd, std::to_wstring(procInfo.procInfo.dwProcessId).c_str());
        
    PROCESS_INFORMATION pi;
    if (!CreateProcessAsUser(procInfo.serverToken, nullptr, cmd, nullptr, nullptr,
        TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        nullptr, nullptr, &startup, &pi)) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exit_code == 0;
}

void WINAPI SvcMain(DWORD argc, LPTSTR* argv) {
    g_svcStatusHandle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, SvcCtrlHandler, nullptr);
    if (!g_svcStatusHandle) {
        Monidroid::TaggedLog(SERVICE_NAME, TEXT("[BUG] Incorrect service name registered with sc"));
        return;
    }

    g_svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_svcStatus.dwServiceSpecificExitCode = 0;

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    g_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (g_stopEvent == NULL) {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    DWORD code = NO_ERROR;
    while (WaitForSingleObject(g_stopEvent, 5000) != WAIT_OBJECT_0) {
        DWORD sessionId = WTSGetActiveConsoleSessionId();
        if (sessionId != 0xFFFFFFFF) {
            ProcInfo procInfo;
            code = LaunchServerInSession(sessionId, &procInfo);
            if (code == NO_ERROR) {
                HANDLE handles[] = { g_stopEvent, procInfo.procInfo.hProcess };
                DWORD result = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);

                if (result != WAIT_OBJECT_0 + 1) {
                    // Error occured or stop requested
                    if (!TryStopServer(procInfo)) {
                        code = ERROR_PROCESS_ABORTED;
                        TerminateProcess(procInfo.procInfo.hProcess, code);
                    }
                } else {
                    // Server suddenly stopped
                    GetExitCodeProcess(procInfo.procInfo.hProcess, &code);
                }
            } else {
                Monidroid::TaggedLog(SERVICE_NAME, TEXT("Failed to start server, error code {}"), code);
            }
            break;
        }
    }

    ReportSvcStatus(SERVICE_STOPPED, code, 0);
}

DWORD WINAPI SvcCtrlHandler(DWORD dwCtrl, DWORD eventType, LPVOID data, LPVOID context) {
    switch (dwCtrl) {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        SetEvent(g_stopEvent);
        
        //ReportSvcStatus(g_svcStatus.dwCurrentState, NO_ERROR, 0);
        break;
    case SERVICE_CONTROL_SESSIONCHANGE: {
        PWTSSESSION_NOTIFICATION notification = (PWTSSESSION_NOTIFICATION)data;
        if (eventType == WTS_CONSOLE_CONNECT) {
            Monidroid::TaggedLog(SERVICE_NAME, TEXT("Session {} started"), notification->dwSessionId);
        } else if (eventType == WTS_CONSOLE_DISCONNECT) {
            Monidroid::TaggedLog(SERVICE_NAME, TEXT("Session {} ended"), notification->dwSessionId);
        }
    }
        break;
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }

    return NO_ERROR;
}

VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint) {
    static DWORD dwCheckPoint = 1;

    g_svcStatus.dwCurrentState = dwCurrentState;
    g_svcStatus.dwWin32ExitCode = dwWin32ExitCode;
    g_svcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        g_svcStatus.dwControlsAccepted = 0;
    else
        g_svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;

    if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
        g_svcStatus.dwCheckPoint = 0;
    else
        g_svcStatus.dwCheckPoint = dwCheckPoint++;

    SetServiceStatus(g_svcStatusHandle, &g_svcStatus);
}
