#include "Main.h"

#include "Installer.h"
#include "Adapter.h"
#include "DesktopNotificationManagerCompat.h"

#include <iostream>
#include <list>
#include <array>
#include <codecvt>

#include <Windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <strsafe.h>
#include <dxgi1_5.h>
#include <d3d11_3.h>
#include <wincodec.h>

#include "../MonidroidInfo/Monidroid.h"

#include <NotificationActivationCallback.h>
#include <windows.ui.notifications.h>

using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::UI::Notifications;
using namespace Microsoft::WRL;

SERVICE_STATUS_HANDLE hServiceStatus;
SERVICE_STATUS serviceStatus;

SOCKET serverSocket = INVALID_SOCKET;
struct addrinfo* serverAddress = NULL;

SOCKET echoSocket = INVALID_SOCKET;
struct addrinfo* echoAddress = NULL;

HANDLE hCommunicationThread;
HANDLE hEchoThread;

CRITICAL_SECTION syncRoot;
std::list<ClientInfo*> clients;

HANDLE hAdapter = INVALID_HANDLE_VALUE;

void ServiceMain(DWORD argc, LPWSTR* argv);
void AppMain();
DWORD CommunicationMain(void* clientInfo);
DWORD EchoMain(void* unused);
void HandleCreateThreadFailure(ClientInfo* pClientInfo);
void HandlerProc(DWORD dwControl);

int InitService();
void FinalizeService();

/*
* ---- Notification support
*/
class DECLSPEC_UUID("9BB6DF6C-F7E7-43F0-9DA4-F6578839E551") NotificationActivator WrlSealed WrlFinal
	: public RuntimeClass<RuntimeClassFlags<ClassicCom>, INotificationActivationCallback>
{
public:
	virtual HRESULT STDMETHODCALLTYPE Activate(
		_In_ LPCWSTR appUserModelId,
		_In_ LPCWSTR invokedArgs,
		_In_reads_(dataCount) const NOTIFICATION_USER_INPUT_DATA * data,
		ULONG dataCount) override
	{
		return S_OK;
	}
};

CoCreatableClass(NotificationActivator);

HRESULT InitNotifications() {

	HRESULT hr = DesktopNotificationManagerCompat::RegisterAumidAndComServer(
		L"MaksimDadush.Monidroid", __uuidof(NotificationActivator)
	);
	if (SUCCEEDED(hr)) {
		hr = DesktopNotificationManagerCompat::RegisterActivator();
	}
	return hr;
}

HRESULT SendConnectNotification(LPCWSTR modelName) {
	// Construct XML
	ComPtr<IXmlDocument> doc;

	LPWSTR xml = (LPWSTR)calloc(1024, 2);
	if (!xml) {
		return E_OUTOFMEMORY;
	}
	StringCchPrintfW(xml, 1024,
		L"<toast><visual><binding template='ToastGeneric'><text>Device %s connected</text></binding></visual></toast>",
		modelName
	);

	HRESULT hr = DesktopNotificationManagerCompat::CreateXmlDocumentFromString(xml, &doc);
	if (SUCCEEDED(hr)) {
		// See full code sample to learn how to inject dynamic text, buttons, and more

		// Create the notifier
		// Desktop apps must use the compat method to create the notifier.
		ComPtr<IToastNotifier> notifier;
		hr = DesktopNotificationManagerCompat::CreateToastNotifier(&notifier);
		if (SUCCEEDED(hr)) {
			// Create the notification itself (using helper method from compat library)
			ComPtr<IToastNotification> toast;
			hr = DesktopNotificationManagerCompat::CreateToastNotification(doc.Get(), &toast);
			if (SUCCEEDED(hr)) {
				// And show it!
				hr = notifier->Show(toast.Get());
			}
		}
	}

	free(xml);
	return hr;
}

/*
* ---- Helper functions
*/
DWORD SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege) {
	TOKEN_PRIVILEGES tp;
	LUID luid;

	if (!LookupPrivilegeValueW(NULL, lpszPrivilege, &luid)) {
		return GetLastError();
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	if (bEnablePrivilege)
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes = 0;

	if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), 
		(PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL)) {
		return GetLastError();
	}

	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
		return ERROR_NOT_ALL_ASSIGNED;
	}

	return 0;
}


int ReceiveNeedByteCount(SOCKET socket, char* resultBuf, int bufSize, int needBytes, int* actualReceived = nullptr) {
	char* recvBuf = (char*)malloc(bufSize);
	int bytesReceived = -1;
	int currentSize = 0;

	while (bytesReceived != 0 && needBytes > 0) {
		bytesReceived = recv(socket, recvBuf, needBytes, 0);
		if (bytesReceived == SOCKET_ERROR) {
			break;
		}
		memcpy(resultBuf + currentSize, recvBuf, bytesReceived);
		needBytes -= bytesReceived;
		currentSize += bytesReceived;
	}

	free(recvBuf);
	if (actualReceived)
		*actualReceived = currentSize;
	return WSAGetLastError();
}

template <typename T> void CoSafeRelease(T** ppT) {
	if (*ppT) {
		(*ppT)->Release();
		*ppT = nullptr;
	}
}

void MainDebugPrint(const wchar_t* format, ...) {
	wchar_t buffer[1024];
	va_list args;
	va_start(args, format);
	vswprintf(buffer, 1024, format, args);
	va_end(args);
	OutputDebugStringW(buffer);
}

void ReportServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.
	serviceStatus.dwCurrentState = dwCurrentState;
	serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
	serviceStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		serviceStatus.dwControlsAccepted = 0;
	else serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
		serviceStatus.dwCheckPoint = 0;
	else serviceStatus.dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM.
	SetServiceStatus(hServiceStatus, &serviceStatus);
}

const char* const HelpMessage = MAKE_MONIDROID_VERSION_MESSAGE("Monidroid Windows server version ", "") "\n"
"Options:\n"
"--help            Print help" "\n"
"--install         Install as a Windows service" "\n"
"--uninstall       Uninstall Windows service" "\n"
"--no-service      Start as a console application" "\n"
"\n"
;

/*
* ===== ENTRY POINT =====
*/
int main(int argc, char** argv) {
	if (argc == 2) {
		std::string command(argv[1]);
		if (command == "--install") {
			InstallService();
		} else if (command == "--uninstall") {
			UninstallService();
		} else if (command == "--help") {
			std::cout << HelpMessage;
		} else if (command == "--no-service") {
			std::cout << "Starting..." << '\n';

			int code = InitService();

			if (code != 0) {
				std::cout << "Error while starting." << '\n';
				FinalizeService();
				return -1;
			}

			std::cout << "Started. Accepting connections..." << '\n';

			AppMain();

			FinalizeService();

			return 0;
		} else {
			std::cout << "Unknown option. Use --help to get available options." << '\n';
		}
		return 0;
	}

	SERVICE_TABLE_ENTRYW dispatchTable[] = {
		{ (LPWSTR)MY_SERVICE_NAME, ServiceMain },
		{ NULL, NULL },
	};

	// start the service
	if (!StartServiceCtrlDispatcherW(dispatchTable)) {
		return GetLastError();
	}

	return 0;
}

void ServiceMain(DWORD argc, LPWSTR* argv) {
	hServiceStatus = RegisterServiceCtrlHandlerW(MY_SERVICE_NAME, HandlerProc);

	// initialization
	serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	serviceStatus.dwServiceSpecificExitCode = 0;

	ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 10'000);

	// section INIT
	int code = InitService();

	ReportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

	// section MAIN
	if (code == 0) {
		AppMain();
	}

	ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);	
	
	// section FINALIZE
	FinalizeService();

	ReportServiceStatus(SERVICE_STOPPED, code, 0);
}

void HandlerProc(DWORD dwControl) {
	switch (dwControl) {
	case SERVICE_CONTROL_CONTINUE:
		ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 10'000);
		break;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	case SERVICE_CONTROL_STOP:
		shutdown(serverSocket, SD_BOTH);
		closesocket(serverSocket);
		shutdown(echoSocket, SD_BOTH);
		closesocket(echoSocket);
		break;
	default:
		break;
	}
}

/*
* Section INIT
*/
#pragma region INIT
int InitServerSocket() {
	// startup sockets
	int code;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// get local address info
	code = getaddrinfo(NULL, MONIDROID_PORT_SZ, &hints, &serverAddress);
	if (code != 0) {
		return code;
	}

	serverSocket = socket(serverAddress->ai_family, serverAddress->ai_socktype, serverAddress->ai_protocol);
	if (serverSocket == INVALID_SOCKET) {
		return WSAGetLastError();
	}

	code = bind(serverSocket, serverAddress->ai_addr, (int)serverAddress->ai_addrlen);
	if (code == SOCKET_ERROR) {
		return WSAGetLastError();
	}

	code = listen(serverSocket, 5);
	if (code == SOCKET_ERROR) {
		return WSAGetLastError();
	}

	return 0;
}

int InitEchoSocket() {
	int code;

	echoSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (echoSocket == SOCKET_ERROR) return WSAGetLastError();

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MONIDROID_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	code = bind(echoSocket, (sockaddr*)&addr, sizeof(addr));
	if (code == SOCKET_ERROR) return WSAGetLastError();

	BOOL broadcast = TRUE;
	setsockopt(echoSocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(BOOL));
	if (code == SOCKET_ERROR) return WSAGetLastError();

	hEchoThread = CreateThread(NULL, 0, EchoMain, NULL, 0, NULL);

	return 0;
}

int InitService() {
	WSADATA wsaData;
	int code = 0;

	// Init privileges to duplicate D3D handles
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
		return GetLastError();
	}
	code = SetPrivilege(hToken, SE_DEBUG_NAME, TRUE);
	if (code != 0) {
		return code;
	}

	code = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (code != 0) {
		return code;
	}

	code = InitServerSocket();
	if (code) {
		return code;
	}

	InitEchoSocket();

	// COM
	CoInitialize(NULL);

	// Notifications
	InitNotifications();

	// Synchronization
	InitializeCriticalSection(&syncRoot);

	// Init adapter
	DWORD err = MonidroidInitGraphicsAdapter(&hAdapter);
	if (hAdapter == INVALID_HANDLE_VALUE) {
		return err;
	}

	return 0;
}
#pragma endregion

/*
* Section MAIN
*/
#pragma region MAIN
void AppMain() {
	bool accepting = true;

	// accepting connections
	while (accepting) {
		SOCKET clientSocket = accept(serverSocket, NULL, NULL);
		if (clientSocket != INVALID_SOCKET) {
			OutputDebugStringW(L"--- New client connected\n");

			ClientInfo* pClientInfo = new ClientInfo(clientSocket);

			EnterCriticalSection(&syncRoot);
			clients.push_back(pClientInfo);
			LeaveCriticalSection(&syncRoot);

			hCommunicationThread = CreateThread(NULL, 0, CommunicationMain, pClientInfo, 0, NULL);
			if (hCommunicationThread == NULL) {
				HandleCreateThreadFailure(pClientInfo);
			}
		} else {
			accepting = false;
			OutputDebugStringW(L"### Ended acceping connections\n");
		}
	}
}

/// <summary>
/// Indentifying device (step 1)
/// </summary>
DWORD IdentifyDevice(ClientInfo *pClientInfo) {
	SOCKET clientSocket = pClientInfo->clientSocket;
	LPCSTR welcomeWord = WELCOME_WORD;

	const int BUF_SIZE = 256;
	const int intSize = 4;
	char welcomeBuf[WELCOME_WORD_LEN + intSize];
	int mainLen = 0;
	int err;
	
	// WELCOME + model length
	err = ReceiveNeedByteCount(clientSocket, welcomeBuf, WELCOME_WORD_LEN + intSize, WELCOME_WORD_LEN + intSize);
	if (err) {
		return err;
	}

	// compare WELCOME and received
	if (memcmp(welcomeBuf, welcomeWord, WELCOME_WORD_LEN) != 0) {
		return ERROR_INVALID_DATA;
	}

	// next 4 bytes = model length
	int cModel;
	memcpy(&cModel, welcomeBuf + WELCOME_WORD_LEN, 4);

	// get model name
	LPWSTR szModelName = (LPWSTR)calloc(cModel + 1, 2);
	szModelName[cModel] = L'\0';
	err = ReceiveNeedByteCount(clientSocket, (char*)szModelName, cModel * 2, cModel * 2);
	if (err) {
		return err;
	}

	SendConnectNotification(szModelName);

	// next 12 bytes = display settings
	int settings[3];
	err = ReceiveNeedByteCount(clientSocket, (char*)settings, 3 * intSize, 3 * intSize);

	pClientInfo->SetupClient(settings[0], settings[1], settings[2]);

	return 0;
}

/// <summary>
/// Sending I/O request to adapter (step 2)
/// </summary>
DWORD ConnectMonitor(ClientInfo* pClientInfo) {
	ADAPTER_MONITOR_INFO monitorInfo = {};
	ADAPTER_MONITOR_INFO monitorInfoOut = {};
	monitorInfo.monitorNumberBySocket = pClientInfo->clientSocket;
	monitorInfo.width = pClientInfo->width;
	monitorInfo.height = pClientInfo->height;
	monitorInfo.hertz = pClientInfo->hertz;

	DWORD bytesReceived = 0;

	if (!DeviceIoControl(hAdapter, IOCTL_IDDCX_MONITOR_CONNECT,
		&monitorInfo, sizeof(monitorInfo), &monitorInfoOut, sizeof(monitorInfoOut), &bytesReceived, NULL)) {
		return GetLastError();
	}

	pClientInfo->connectorIndex = monitorInfoOut.connectorIndex;
	pClientInfo->adapterLuid = monitorInfoOut.adapterLuid;
	pClientInfo->driverProcessId = monitorInfoOut.driverProcessId;

	return 0;
}

/// <summary>
/// Disconnect monitor (step 4)
/// </summary>
DWORD DisconnectMonitor(ClientInfo* pClientInfo) {
	ADAPTER_MONITOR_INFO monitorInfo = {};
	ADAPTER_MONITOR_INFO monitorInfoOut = {};
	monitorInfo.monitorNumberBySocket = pClientInfo->clientSocket;
	monitorInfo.connectorIndex = pClientInfo->connectorIndex;

	DWORD bytesReceived = 0;

	if (!DeviceIoControl(hAdapter, IOCTL_IDDCX_MONITOR_DISCONNECT,
		&monitorInfo, sizeof(monitorInfo), &monitorInfoOut, sizeof(monitorInfoOut), &bytesReceived, NULL)) {
		return GetLastError();
	}

	return 0;
}

/// <summary>
/// Send frames (step 3)
/// </summary>
HRESULT SendFrames(ClientInfo* pClientInfo) {
	std::array<D3D_FEATURE_LEVEL, 7> featureLevels {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
	};

	// Try open driver process to handle duplicating
	HANDLE hDriverProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pClientInfo->driverProcessId);
	if (hDriverProcess == NULL) {
	    return HRESULT_FROM_WIN32(GetLastError());
	}
	HANDLE hMyProcess = GetCurrentProcess();

	ComPtr<IDXGIFactory5> pDxgiFactory;
	ComPtr<IDXGIAdapter> pAdapter;
	ComPtr<ID3D11Device> pDevice0;
	ComPtr<ID3D11DeviceContext> pDeviceContext0;
	ComPtr<ID3D11Device3> pDevice;
	ComPtr<ID3D11DeviceContext3> pDeviceContext;

	ComPtr<IWICImagingFactory> pWicFactory;

	// Create D3D device to process frames
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&pDxgiFactory));

	if (SUCCEEDED(hr)) {
		hr = pDxgiFactory->EnumAdapterByLuid(pClientInfo->adapterLuid, IID_PPV_ARGS(&pAdapter));
	}

	if (SUCCEEDED(hr)) {
		hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT,
			featureLevels.data(), static_cast<UINT>(featureLevels.size()),
			D3D11_SDK_VERSION, &pDevice0, NULL, &pDeviceContext0
		);
	}

	// Get D3D 11.1 interfaces
	if (SUCCEEDED(hr)) {
		hr = pDevice0->QueryInterface(IID_PPV_ARGS(&pDevice));
	}

	if (SUCCEEDED(hr)) {
		hr = pDeviceContext0->QueryInterface(IID_PPV_ARGS(&pDeviceContext));
	}

	// Create WIC factory
	if (SUCCEEDED(hr)) {
		hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWicFactory));
	}

	// Receive frames from driver
	FRAME_MONITOR_INFO frameInfo { .connectorIndex = pClientInfo->connectorIndex, .hDriverHandle = nullptr };
	FRAME_MONITOR_INFO frameInfoOut { };
	DWORD bytesReceived = -1;

	bool sending = SUCCEEDED(hr);
	while (sending) {
		if (DeviceIoControl(hAdapter, IOCTL_IDDCX_REQUEST_FRAME,
			&frameInfo, sizeof(frameInfo), &frameInfoOut, sizeof(frameInfoOut), &bytesReceived, nullptr))
		{
			HANDLE hFrameHandle { };

			if (!DuplicateHandle(
				hDriverProcess, frameInfoOut.hDriverHandle,
				hMyProcess, &hFrameHandle,
				0, FALSE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS
			)) {
				CloseHandle(frameInfoOut.hDriverHandle);
				continue;
			}

			ComPtr<ID3D11Texture2D> pSharedTexture;
			hr = pDevice->OpenSharedResource1(hFrameHandle, IID_PPV_ARGS(&pSharedTexture));
			if (SUCCEEDED(hr)) {
				// Create staging texture
				// 1. Cannot create STAGING and SHARED_NTHANDLE texture
				// 2. ID3D11Device3::ReadFromSubresource does not work on ID3D11Device3 different from driver's ID3D11Device3
				D3D11_TEXTURE2D_DESC desc { };
				pSharedTexture->GetDesc(&desc);
				desc.Usage = D3D11_USAGE_STAGING;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				desc.BindFlags = 0;
				desc.MiscFlags = 0;

				ComPtr<ID3D11Texture2D> pTexture;

				hr = pDevice->CreateTexture2D(&desc, nullptr, &pTexture);
				if (FAILED(hr)) {
					continue;
				}
				pDeviceContext->CopyResource(pTexture.Get(), pSharedTexture.Get());
				
				// Get bytes
				D3D11_MAPPED_SUBRESOURCE mappedResource { };
				hr = pDeviceContext->Map(pTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
				
				// Start creating picture
				IWICBitmap* pRawBitmap = nullptr;
				IWICBitmapEncoder* pEncoder = nullptr;
				IStream* pOutStream = nullptr;
				IWICBitmapFrameEncode* pFrame = nullptr;

				hr = pWicFactory->CreateBitmapFromMemory(
					desc.Width, desc.Height, GUID_WICPixelFormat32bppRGBA,
					mappedResource.RowPitch, mappedResource.DepthPitch,
					static_cast<BYTE*>(mappedResource.pData), &pRawBitmap
				);
				hr = pWicFactory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &pEncoder);

				hr = CreateStreamOnHGlobal(nullptr, TRUE, &pOutStream);
				hr = pEncoder->Initialize(pOutStream, WICBitmapEncoderNoCache);
				
				hr = pEncoder->CreateNewFrame(&pFrame, nullptr);
				hr = pFrame->Initialize(nullptr);
				hr = pFrame->SetSize(desc.Width, desc.Height);
				auto formatGUID = GUID_WICPixelFormat32bppRGBA;
				hr = pFrame->SetPixelFormat(&formatGUID);
				hr = pFrame->WriteSource(pRawBitmap, nullptr);

				hr = pFrame->Commit();
				hr = pEncoder->Commit();

				pDeviceContext->Unmap(pTexture.Get(), 0);

				if (SUCCEEDED(hr)) {
					// Send picture
					HGLOBAL hGlobal = nullptr;
					hr = GetHGlobalFromStream(pOutStream, &hGlobal);
					char* jpegData = (char*)GlobalLock(hGlobal);
					int jpegSize = GlobalSize(hGlobal);

					//STATSTG stat;
					//pStream->Stat(&stat, STATFLAG_NONAME);
					//int size = stat.cbSize.QuadPart;
					ULONG bufSize = FRAME_WORD_LEN + sizeof(jpegSize) + jpegSize;
					//ULONG actual;

					auto sendBuffer = std::make_unique<char[]>(bufSize);

					memcpy(sendBuffer.get(), FRAME_WORD, FRAME_WORD_LEN);
					memcpy(sendBuffer.get() + FRAME_WORD_LEN, (void*)&jpegSize, sizeof(jpegSize));
					memcpy(sendBuffer.get() + FRAME_WORD_LEN + sizeof(jpegSize), _Notnull_ jpegData, jpegSize);

					int bytesSent = send(pClientInfo->clientSocket, sendBuffer.get(), bufSize, 0);
					if (bytesSent == 0 || bytesSent == SOCKET_ERROR) {
						sending = false;
					}

					GlobalUnlock(hGlobal);
				}
				CloseHandle(hFrameHandle);
				CoSafeRelease(&pFrame);
				CoSafeRelease(&pOutStream);
				CoSafeRelease(&pEncoder);
				CoSafeRelease(&pRawBitmap);
			}
		} else {
			// No frame, send black screen
			int size = 0;
			int intSize = 4;
			ULONG bufSize = FRAME_WORD_LEN + intSize;

			char* sendBuffer = new char[bufSize];
			memcpy(sendBuffer, FRAME_WORD, FRAME_WORD_LEN);
			memcpy(sendBuffer + FRAME_WORD_LEN, (void*)&size, intSize);

			int res = send(pClientInfo->clientSocket, sendBuffer, bufSize, 0);
			if (res == 0 || res == SOCKET_ERROR) {
				sending = false;
			}

			delete[] sendBuffer;
		}
	}

	CloseHandle(hDriverProcess);
	CloseHandle(hMyProcess);

	return hr;
}

/// <summary>
/// Communication thread main function
/// </summary>
/// <param name="param">Pointer to ClientInfo structure</param>
DWORD WINAPI CommunicationMain(void* param) {
	std::unique_ptr pClientInfo = std::unique_ptr<ClientInfo>(static_cast<ClientInfo*>(param));
	param = nullptr;
	SOCKET clientSocket = pClientInfo->clientSocket;

	// COM
	if (FAILED(CoInitialize(nullptr))) {
		return HRESULT_CODE(E_UNEXPECTED);
	}

	// 1. Identify device
	DWORD code = IdentifyDevice(pClientInfo.get());
	if (code != 0) {
		return code;
	}

	// 2. Send I/O request to adapter
	code = ConnectMonitor(pClientInfo.get());
	if (code != 0) {
		return code;
	}

	// 3. Get frames from driver and send to device
	SendFrames(pClientInfo.get());

	// 4. End communication
	DisconnectMonitor(pClientInfo.get());
end:
	EnterCriticalSection(&syncRoot);
	clients.remove(pClientInfo.get());
	LeaveCriticalSection(&syncRoot);

	CoUninitialize();
	return 0;
}

/// <summary>
/// Function for automatic server scanning by clients
/// </summary>
DWORD EchoMain(void* unused) {
	bool accepting = true;
	int responseHeaderSize = strlen(SERVER_ECHO_WORD);

	// Setup buffer with computer name
	DWORD cCompName = 0;
	GetComputerNameExW(ComputerNameDnsHostname, NULL, &cCompName);
    std::wstring compName(cCompName, 0);
	//auto compName = std::unique_ptr<wchar_t[]>(new wchar_t[cCompName]);
	GetComputerNameExW(ComputerNameDnsHostname, compName.data(), &cCompName);

    // cCompName how does not includes NULL character
	int sendBufSize = responseHeaderSize + 4 + cCompName * 2;
	auto sendBuf = std::unique_ptr<char[]>(new char[sendBufSize]);
	memcpy(sendBuf.get(), SERVER_ECHO_WORD, responseHeaderSize);
	memcpy(sendBuf.get() + responseHeaderSize, &cCompName, 4);
	memcpy(sendBuf.get() + responseHeaderSize + 4, compName.data(), cCompName * sizeof(wchar_t));

	//auto buf = std::unique_ptr<char[]>(new char[headerSize]);
	std::string buf(CLIENT_ECHO_WORD);
	while (accepting) {
		sockaddr addr;
		int addrlen = sizeof(sockaddr);
		int bytesReceived = recvfrom(echoSocket, buf.data(), buf.size(), 0, &addr, &addrlen);
		if (bytesReceived == 0 || bytesReceived == SOCKET_ERROR) {
			MainDebugPrint(L"recvfrom failed with code %d\n", WSAGetLastError());
			accepting = false;
		} else if (bytesReceived == buf.size() && buf == CLIENT_ECHO_WORD) {
			int bytesSent = sendto(echoSocket, sendBuf.get(), sendBufSize, 0, &addr, addrlen);
			if (bytesSent == SOCKET_ERROR) {
				accepting = false;
			}
		}
	}
	return 0;
}

void HandleCreateThreadFailure(ClientInfo* pClientInfo) {
	EnterCriticalSection(&syncRoot);
	clients.remove(pClientInfo);
	LeaveCriticalSection(&syncRoot);

	delete pClientInfo;
}

#pragma endregion

/*
* Section FINALIZE
*/
#pragma region FINALIZE
void FinalizeService() {
	// server socket
	if (serverSocket != INVALID_SOCKET) {
		closesocket(serverSocket);
		serverSocket = INVALID_SOCKET;
	}
	if (serverAddress != NULL) {
		freeaddrinfo(serverAddress);
		serverAddress = NULL;
	}

	// echo socket
	if (echoSocket != INVALID_SOCKET) {
		closesocket(echoSocket);
		echoSocket = INVALID_SOCKET;
	}
	if (echoAddress != NULL) {
		freeaddrinfo(echoAddress);
		echoAddress = NULL;
	}

	// 
	if (hAdapter != INVALID_HANDLE_VALUE) {
		CloseHandle(hAdapter);
		hAdapter = INVALID_HANDLE_VALUE;
	}

	if (hCommunicationThread != NULL) {
		CloseHandle(hCommunicationThread);
		hCommunicationThread = NULL;
	}
	if (hEchoThread != NULL) {
		CloseHandle(hEchoThread);
		hEchoThread = NULL;
	}

	CoUninitialize();

	DeleteCriticalSection(&syncRoot);

	WSACleanup();
}

#pragma endregion
