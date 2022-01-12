#include "stdafx.h"
#include "MiniFilterManager.h"
#include "EnablePrivilege.h"

#include "FilterDriverLoaderDlg.h"
extern CFilterDriverLoaderDlg *g_pMain;

#if (( (OSVER(NTDDI_VERSION) == NTDDI_WIN2K) && (SPVER(NTDDI_VERSION) >= SPVER(NTDDI_WIN2KSP4) )) || \
                          ((OSVER(NTDDI_VERSION) == NTDDI_WINXP) && (SPVER(NTDDI_VERSION) >= SPVER(NTDDI_WINXPSP2))) || \
                          ((OSVER(NTDDI_VERSION) == NTDDI_WS03)  && (SPVER(NTDDI_VERSION) >= SPVER(NTDDI_WS03SP1))) ||  \
                          (NTDDI_VERSION >= NTDDI_VISTA))
#endif

#if OSVER(NTDDI_VERSION) == NTDDI_WS03
#endif

#if SPVER(NTDDI_VERSION) >= SPVER(NTDDI_WS03SP1)
#endif


BOOL FileExists(CString strFileName)
{
	HANDLE h_bitmap_file = INVALID_HANDLE_VALUE;
	h_bitmap_file = CreateFile(strFileName, GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(h_bitmap_file == INVALID_HANDLE_VALUE)
	{
		strFileName = _T("\\\\?\\") + strFileName;
		h_bitmap_file = CreateFile(strFileName, GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	if (h_bitmap_file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(h_bitmap_file);
		return TRUE;
	}
	return FALSE;
}


CString GetExePath()
{
	HMODULE hModule = GetModuleHandleW(NULL);
	TCHAR path[MAX_PATH];
	GetModuleFileNameW(hModule, path, MAX_PATH);
	CString sPath = path;

	int p = sPath.ReverseFind('\\');
	if (p > 0)
	{
		sPath = sPath.Left(p+1);
	}
	return sPath;
}

CMiniFilterManager::CMiniFilterManager()
{
	
	m_hPort = INVALID_HANDLE_VALUE;
	m_hCompletion = INVALID_HANDLE_VALUE;
	m_sServiceName = SERVICE_NAME;
	IsWow64Process(GetCurrentProcess(), &m_bIsWow64);
	if (m_bIsWow64)
	{
		m_sServiceName = SERVICE_NAME_X64;
	}
	m_bConnected = FALSE;
	m_dwExitThreadCode = EXIT_THREAD;
	InitializeCriticalSection(&m_hPortCS);
	InitializeCriticalSection(&m_LogListCS);

	m_hEvent = NULL;
	//CallbackToParent = NULL;
	//m_LogList.RemoveAll();
}


CMiniFilterManager::~CMiniFilterManager(void)
{
	DeleteCriticalSection(&m_hPortCS);
	//DeleteCriticalSection(&m_LogListCS);

	m_dwExitThreadCode = EXIT_THREAD;

	CleanupConnection();

	//WRITELOG(_T("[CMiniFilterManager::~CMiniFilterManager] Complete"));
	//m_LogList.RemoveAll();
}


DWORD CMiniFilterManager::SafeStartFilterDriver(BOOL bConnectPort)
{
	WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] Start"));

	StopFilterDriver();

	CString sDriverPath = GetDriverFilePath();
	if(!FileExists(sDriverPath))
	{
		WRITELOG(_T("[CMiniFilterManager::CreateServiceAndSetRegistry] 드라이버 파일 없음"));
		return MINIFILTER_LOAD_ERROR_NO_DRIVER_FILE;
	}

	int nCnt = 0;
	BOOL bRet = FALSE;
	DWORD dwRegisterError = 0;
	while (TRUE)
	{
		bRet = RegisterFilterDriverServ(sDriverPath);
		if(bRet == TRUE) break;

		dwRegisterError = m_dwLastError;

		Sleep(500);
		WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] RegisterFilterDriverServ Count - %d"), nCnt);
		if (nCnt++ >= 5)
		{
			UnRegisterFilterDriverServ();
		}
		if (nCnt++ >= 10)
		{
			WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] RegisterFilterDriverServ Err"));
			break;
		}
	}
	if(bRet == FALSE)
	{
		WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] #Error. RegisterFilterDriverServ fail. lasterror=%d"), dwRegisterError);
		if(dwRegisterError == ERROR_ACCESS_DENIED)
		{
			return MINIFILTER_LOAD_ERROR_ACCESS_DENIED;
		}
		return MINIFILTER_LOAD_ERROR_REGISTER_SERVICE_FAIL;
	}

	BOOL bIsEnablePrivilege = FALSE;
	CEnablePrivilege cEnablePrivilege;
	bIsEnablePrivilege = cEnablePrivilege.EnablePrivilege(SE_LOAD_DRIVER_NAME);
	if (bIsEnablePrivilege == FALSE)
	{
		WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] EnablePrivilege Fail"));
	}

	nCnt = 0;
	bRet = FALSE;
	while (TRUE)
	{
		bRet = StartFilterDriver();
		if(bRet == TRUE) break;

		Sleep(500);
		WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] StartFilterDriver Count - %d"), nCnt);
		if (nCnt++ == 5)
		{
			UnRegisterFilterDriverServ();
			RegisterFilterDriverServ(sDriverPath);
		}
		if (nCnt == 10)
		{
			WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] StartFilterDriver Err"));
			break;
		}
	}
	if(bRet == FALSE)
	{
		WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] # Error. StartFilterDriver Fail"));
		return MINIFILTER_LOAD_ERROR_START_SERVICE_FAIL;
	}

	WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] Success."));

	if (bConnectPort)
	{
		ConnectCommPort();
	}
	return MINIFILTER_LOAD_SUCCESS;
}

void CMiniFilterManager::SetRegistry()
{
	// Altitude 설정.
	HKEY hKey;
	CString sSubKey;
	sSubKey.Format(_T("SYSTEM\\CurrentControlSet\\Services\\%s\\"), m_sServiceName);
	sSubKey += _T("Instances");
	if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, sSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
	{
		return;
	}

	if (RegSetValueEx(hKey, _T("DefaultInstance"), 0, REG_SZ, (LPBYTE)(LPCTSTR)m_sServiceName, m_sServiceName.GetLength() * sizeof(TCHAR)) != ERROR_SUCCESS)
	{
		return;
	}
	RegCloseKey(hKey);

	sSubKey += _T("\\");
	sSubKey += m_sServiceName;
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, sSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
	{
		return;
	}

	CString sAltitude;
	sAltitude = FILTER_ALTITUDE;
	if(RegSetValueEx(hKey, _T("Altitude"), 0, REG_SZ, (LPBYTE)sAltitude.GetBuffer(), (sAltitude.GetLength()+1)*sizeof(TCHAR)) != ERROR_SUCCESS)
	{
		return;
	}

	CString sFlags;
	DWORD dwValue = 0;
	if(RegSetValueEx(hKey, _T("Flags"), 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		return;
	}

	RegCloseKey(hKey);
}

BOOL CMiniFilterManager::StartFilterDriver()
{
	SC_HANDLE hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hManager == NULL)
	{
		WRITELOG(_T("[CMiniFilterManager::StartFilterDriver] OpenSCManager Err - 0x%08x"), GetLastError());
		return FALSE;
	}

	SC_HANDLE hService = OpenService(hManager, m_sServiceName, SERVICE_ALL_ACCESS);
	if (hService == NULL)
	{
		WRITELOG(_T("[CMiniFilterManager::StartFilterDriver] OpenService Err - 0x%08x"), GetLastError());
		CloseServiceHandle(hManager);

		return FALSE;
	}

	DWORD e = GetLastError();
	if (StartService(hService, 0, NULL) == FALSE)
	{
		DWORD dwErr = GetLastError();
		if (dwErr == ERROR_SERVICE_ALREADY_RUNNING)
		{
			CloseServiceHandle(hService);
			CloseServiceHandle(hManager);
			WRITELOG(_T("[CMiniFilterManager::StartFilterDriver] #### 미니필터 드라이버 이미 로드됨 ####"));
			return TRUE;
		}
		//0xB7(183)=ERROR_ALREADY_EXISTS
		CloseServiceHandle(hService);
		CloseServiceHandle(hManager);
		WRITELOG(_T("[CMiniFilterManager::StartFilterDriver] StartService Err - 0x%08x"), dwErr);
		return FALSE;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hManager);
	WRITELOG(_T("[CMiniFilterManager::StartFilterDriver] #### 드라이버 로드 성공 ####"));

	return TRUE;
}

BOOL CMiniFilterManager::IsDriverRunning()
{
	//WRITELOG(L"[CMiniFilterManager::IsDriverRunning] Start.");
	DWORD dwError = 0;

	SC_HANDLE hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	//WRITELOG(L"[CMiniFilterManager::IsDriverRunning] OpenSCManager. hManager=%x", hManager);
	if (hManager == NULL)
	{
		dwError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::IsDriverRunning] OpenSCManager Err - 0x%08x"), dwError);
		return FALSE;
	}
	//WRITELOG(_T("[CMiniFilterManager::IsDriverRunning] OpenSCManager Success"));

	SC_HANDLE hService = OpenService(hManager, m_sServiceName, SERVICE_ALL_ACCESS);
	if (hService == NULL)
	{
		CloseServiceHandle(hManager);
		dwError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::IsDriverRunning] OpenService Err - 0x%08x"), dwError);
		return FALSE;
	}
	//WRITELOG(_T("[CMiniFilterManager::IsDriverRunning] OpenService Success"));

	SERVICE_STATUS stStatus;
	BOOL bRet = QueryServiceStatus(hService, &stStatus);
	if (bRet != TRUE)
	{
		CloseServiceHandle(hService);
		CloseServiceHandle(hManager);
		dwError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::IsDriverRunning] QueryServiceStatus Err - 0x%08x"), dwError);
		return FALSE;
	}
	//WRITELOG(_T("[CMiniFilterManager::IsDriverRunning] QueryServiceStatus Success. status=%d"), stStatus.dwCurrentState);

	CloseServiceHandle(hService);
	CloseServiceHandle(hManager);

	if (stStatus.dwCurrentState != SERVICE_RUNNING)
	{
		return FALSE;
	}
	return TRUE;
}


BOOL CMiniFilterManager::StopFilterDriver()
{
	WRITELOG(L"[CMiniFilterManager::StopFilterDriver] Start.");
	DWORD dwError = 0;
	SC_HANDLE hManager = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);
	WRITELOG(L"[CMiniFilterManager::StopFilterDriver] OpenSCManager. hManager=%x", hManager);
	if (hManager == NULL)
	{
		dwError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] OpenSCManager Err - 0x%08x"), dwError);

		return FALSE;
	}
	WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] OpenSCManager Success"));

	SC_HANDLE hService = OpenService(hManager, m_sServiceName, SERVICE_ALL_ACCESS);
	if (hService == NULL)
	{
		CloseServiceHandle(hManager);
		dwError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] OpenService Err - 0x%08x"), dwError);

		return FALSE;
	}
	WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] OpenService Success"));

	SERVICE_STATUS stStatus;
	BOOL bRet = QueryServiceStatus(hService, &stStatus);
	if (bRet != TRUE)
	{
		CloseServiceHandle(hService);
		CloseServiceHandle(hManager);
		dwError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] QueryServiceStatus Err - 0x%08x"), dwError);

		return FALSE;
	}
	WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] QueryServiceStatus Success. status=%d"), stStatus.dwCurrentState);

	WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] stStatus.dwCurrentState=%d"), stStatus.dwCurrentState);
	if (stStatus.dwCurrentState != SERVICE_STOPPED)
	{
		bRet = ControlService(hService, SERVICE_CONTROL_STOP, &stStatus);
		WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] ControlService result=%d"), bRet);

		// 드라이버 중지시 스레드도 종료한다.
		m_dwExitThreadCode = EXIT_THREAD;
	}
	WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] ControlService Success"));

	if(m_hCompletion != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hCompletion);
		m_hCompletion = INVALID_HANDLE_VALUE;
	}
	WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] CloseServiceHandle Success"));

	if(m_hPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hPort);
		m_hPort = INVALID_HANDLE_VALUE;
	}
	WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] CloseServiceHandle Start"));

	CloseServiceHandle(hService);
	CloseServiceHandle(hManager);
	WRITELOG(_T("[CMiniFilterManager::StopFilterDriver] CloseServiceHandle Success"));

	WRITELOG(L"[StopFilterDriver] End");
	return bRet;
}

BOOL CMiniFilterManager::RegisterFilterDriverServ(CString sDriverPath)
{
	SC_HANDLE hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hManager == NULL)
	{
		m_dwLastError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::RegisterFilterDriverServ] OpenSCManager Err - 0x%08x"), m_dwLastError);
		return FALSE;
	}
	
	SC_HANDLE hService = OpenService(hManager, m_sServiceName, SERVICE_ALL_ACCESS);
	if (hService == NULL)
	{
		if (CreateServiceAndSetRegistry(sDriverPath))
		{
			WRITELOG(_T("[CMiniFilterManager::RegisterFilterDriverServ] #### 미니필터 드라이버 등록 성공 ####"));
		}
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hManager);

	WRITELOG(_T("[CMiniFilterManager::RegisterFilterDriverServ] Success"));
	return TRUE;
}

BOOL CMiniFilterManager::ExistsMiniFilterDriver()
{
	return FileExists(GetDriverFilePath());
}

CString CMiniFilterManager::GetDriverFilePath()
{
	CString sExePath = GetExePath();
	CString sDriverPath;
	if (m_bIsWow64)
	{
		sDriverPath.Format(_T("%s%s"), sExePath, FILTER_DRIVER_NAME_X64);
	}
	else
	{
		sDriverPath.Format(_T("%s%s"), sExePath, FILTER_DRIVER_NAME);
	}
	WRITELOG(_T("[CMiniFilterManager::GetDriverFilePath] sDriverFilePath=%s"), sDriverPath);

	return sDriverPath;
}

BOOL CMiniFilterManager::CreateServiceAndSetRegistry(CString sDriverPath)
{
	DWORD dwTagID = 4, dwError = 0;
	SC_HANDLE hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hManager == NULL)
	{
		dwError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::CreateServiceAndSetRegistry] OpenSCManager Err - 0x%08x"), dwError);
		return FALSE;
	}

	SC_HANDLE hService = CreateService(hManager, m_sServiceName, m_sServiceName,
		SERVICE_ALL_ACCESS, SERVICE_FILE_SYSTEM_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
		sDriverPath, _T("FSFilter Activity Monitor"), &dwTagID, NULL, NULL, NULL);
	if (hService == NULL)
	{
		CloseServiceHandle(hManager);
		dwError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::CreateServiceAndSetRegistry] CreateService Err - 0x%08x"), dwError);

		return FALSE;
	}

	SetRegistry();

	CloseServiceHandle(hService);

	hService = OpenService(hManager, m_sServiceName, SERVICE_ALL_ACCESS);
	if (hService == NULL)
	{
		dwError = GetLastError();
		CloseServiceHandle(hManager);
		WRITELOG(_T("[CMiniFilterManager::CreateServiceAndSetRegistry] OpenService Err - 0x%08x"), dwError);
		return FALSE;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hManager);
	WRITELOG(_T("[CMiniFilterManager::CreateServiceAndSetRegistry] Success"));
	return TRUE;
}

BOOL CMiniFilterManager::UnRegisterFilterDriverServ()
{
	BOOL bRet = FALSE;
	
	DWORD dwError = 0;
	SC_HANDLE hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hManager == NULL)
	{
		dwError = GetLastError();
		WRITELOG(_T("[CMiniFilterManager::UnRegisterFilterDriverServ] OpenSCManager Err - 0x%08x"), dwError);
		return FALSE;
	}

	SC_HANDLE hService = OpenService(hManager, m_sServiceName, SERVICE_ALL_ACCESS);
	if (hService != NULL)
	{
		WRITELOG(_T("[CMiniFilterManager::UnRegisterFilterDriverServ] OpenSCManager Err - 0x%08x"), dwError);
		bRet = DeleteService(hService);
	}

	CloseServiceHandle(hService);
	WRITELOG(_T("[CMiniFilterManager::UnRegisterFilterDriverServ] CloseServiceHandle(hService)"));
	CloseServiceHandle(hManager);
	WRITELOG(_T("[CMiniFilterManager::UnRegisterFilterDriverServ] CloseServiceHandle(hManager)"));

	return bRet;
}

DWORD CMiniFilterManager::GetFilterDriverStatus()
{
	DWORD dwRet = 0;

	SC_HANDLE hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hManager == NULL)
	{
		WRITELOG(_T("[CMiniFilterManager::GetFilterDriverStatus] OpenSCManager Err - 0x%08x"), GetLastError());
		return dwRet;
	}

	SC_HANDLE hService = OpenService(hManager, m_sServiceName, SERVICE_ALL_ACCESS);
	if (hService == NULL)
	{
		CloseServiceHandle(hManager);
		WRITELOG(_T("[CMiniFilterManager::GetFilterDriverStatus] OpenService Err - 0x%08x"), GetLastError());

		return dwRet;
	}

	SERVICE_STATUS tStatus;
	ControlService(hService, SERVICE_CONTROL_INTERROGATE, &tStatus);
	if (tStatus.dwCurrentState == SERVICE_RUNNING)
	{
		dwRet = SERVICE_RUNNING;
	}
	else if (tStatus.dwCurrentState == SERVICE_STOPPED)
	{
		dwRet = SERVICE_STOPPED;
	}
	else
	{
		WRITELOG(_T("[CMiniFilterManager::GetFilterDriverStatus] CurrentState - %d"), tStatus.dwCurrentState);
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hManager);

	return dwRet;
}

DWORD CMiniFilterManager::ConnectCommPort()
{
	if (m_hPort != NULL && m_hPort != INVALID_HANDLE_VALUE)
	{
		WRITELOG(_T("[CMiniFilterManager::ConnectCommPort] Already connected"));
		return S_OK;
	}

	WRITELOG(_T("[CMiniFilterManager::ConnectCommPort] Start. m_hPort=%x"), m_hPort);

	HANDLE hPort = m_hPort;
	HRESULT hr = ERROR_SUCCESS;

	if(hPort == INVALID_HANDLE_VALUE)
	{
		BOOL bIsEnablePrivilege = FALSE;
		CEnablePrivilege cEnablePrivilege;
		bIsEnablePrivilege = cEnablePrivilege.EnablePrivilege(SE_LOAD_DRIVER_NAME);
		if (bIsEnablePrivilege == FALSE)
		{
			WRITELOG(_T("[CMiniFilterManager::InitFilterDriver] EnablePrivilege Fail"));
		}

		DWORD dwResult = 0;
		SECURITY_ATTRIBUTES sa;
		PSECURITY_DESCRIPTOR pSD = NULL;
		// 2015.03.11 by keede122, SECURITY_ATTRIBUTES 구조체 초기화 수정
	//	InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
		pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
		if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION))
		{
			dwResult = GetLastError();
			WRITELOG(_T("[CMiniFilterManager::ConnectCommPort] InitializeSecurityDescriptor Error : %d"),dwResult);
		}
		sa.lpSecurityDescriptor = pSD;
		WRITELOG(_T("[CMiniFilterManager::ConnectCommPort] sa.lpSecurityDescriptor = pSD"));
		hr = FilterConnectCommunicationPort(IOCP_PORT_NAME, 0, NULL, 0, &sa, &hPort);
		WRITELOG(_T("[CMiniFilterManager::ConnectCommPort] FilterConnectCommunicationPort result=%x"), hr);
		if (hr != S_OK)
		{
			WRITELOG(_T("[CMiniFilterManager::ConnectCommPort] FilterConnectCommunicationPort Err - 0x%08x. hPort=%x"), hr, hPort);
			return hr;
		}

		m_hCompletion = CreateIoCompletionPort(hPort, NULL, 0, SECUWORKS_DEFAULT_THREAD_COUNT);
		WRITELOG(_T("[CMiniFilterManager::ConnectCommPort] CreateIoCompletionPort m_hCompletion=%x"), m_hCompletion);
		if(m_hCompletion == NULL)
		{
			CloseHandle(hPort);
			hr = HRESULT_FROM_WIN32(GetLastError());
			WRITELOG(_T("[CMiniFilterManager::ConnectCommPort] CreateIoCompletionPort Err - 0x%08x"), hr);
			return hr;
		}
		EnterCriticalSection(&m_hPortCS);
		m_hPort = hPort;
		LeaveCriticalSection(&m_hPortCS);
		WRITELOG(_T("[CMiniFilterManager::ConnectCommPort] Final. m_hPort=%x"), m_hPort);

		m_bConnected = TRUE;
		StartReadFilterMsgThread();
	}

	return hr;
}

void CMiniFilterManager::CleanupConnection()
{
	m_bConnected = FALSE;
	if (m_hCompletion != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hCompletion);
		m_hCompletion = INVALID_HANDLE_VALUE;
	}
	if (m_hPort != INVALID_HANDLE_VALUE && m_hPort != NULL)
	{
		CloseHandle(m_hPort);
		m_hPort = INVALID_HANDLE_VALUE;
	}
}

HRESULT CMiniFilterManager::SendTextToDriver(ULONG uCommand, TCHAR *buffer, ULONG dataLen)
{
	WRITELOG(_T("[CMiniFilterManager::SendTextToDriver] Start. command=%d, dataLen=%d, buffer=%s"), uCommand, dataLen, buffer);

	HRESULT hr = ConnectCommPort();
	if (hr != S_OK)
	{
		WRITELOG(_T("[CMiniFilterManager::SendTextToDriver] ConnectCommPort Err - 0x%08x"), hr);
		return hr;
	}

	IOCP_MSG_TO_DRIVER_SENDTEXT msg;
	msg.uCommand = uCommand;
	msg.uSize = dataLen;
	wcsncpy_s(msg.szBuffer, buffer, dataLen);
	msg.szBuffer[dataLen] = 0;

	hr = SendFltMessage(&msg, sizeof(ULONG) + dataLen*sizeof(TCHAR) + sizeof(TCHAR));

	WRITELOG(_T("[CMiniFilterManager::SendTextToDriver] End. hr=%x"), hr);
	return hr;
}

HRESULT CMiniFilterManager::SendFltMessage(LPVOID lpData, DWORD dwDataSize)
{
	WRITELOG(_T("[CMiniFilterManager::SendFltMessage] Start. m_hPort=%x, lpPolicyData=%x, dwDataSize=%d"), m_hPort, lpData, dwDataSize);
	HRESULT hr = S_FALSE;

	if(lpData != NULL)
	{
		DWORD dwRet = -1;
		hr = FilterSendMessage(m_hPort, lpData, dwDataSize, NULL, 0, &dwRet);
		if (hr != S_OK)
		{
			WRITELOG(_T("[CMiniFilterManager::SendFltMessage] FilterSendMessage Err - 0x%08x"), hr);
		}
	}

	return hr;
}

UINT WINAPI ReadFilterMsgThread(LPVOID arg)
{
	CMiniFilterManager *pCDriverHandler = (CMiniFilterManager*)arg;

	pCDriverHandler->m_dwExitThreadCode = ENTER_THREAD;

	// pCDriverHandler->ReadFilterMsg();

	PGRADIUS_LOG_MESSAGE pLogMsgFromDriver = NULL;
	PGRADIUS_LOG_MESSAGE pLogMsg = NULL;
	PIOCP_MSG_FROM_DRIVER_LOG pLAFromDriver;
	GRADIUS_REPLY_MESSAGE replyMsg;
	LPOVERLAPPED pOvlp;
	DWORD dwOutSize;
	BOOL bResult;
	ULONG_PTR pKey;

	WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] Start."));

	while(TRUE)
	{
		bResult = GetQueuedCompletionStatus(pCDriverHandler->m_hCompletion, &dwOutSize, &pKey, &pOvlp, INFINITE);
		//WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] Completion. bResult=%d"), bResult);
		if (bResult == FALSE)
		{
			if (pCDriverHandler->m_dwExitThreadCode == EXIT_THREAD)
			{
				WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] GetQueuedCompletionStatus fail. thread end."));
				break;
			}
			if (pCDriverHandler->m_hCompletion == INVALID_HANDLE_VALUE)
			{
				WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] GetQueuedCompletionStatus fail. m_hCompletion is invalid."));
				break;
			}
			
			/*
			HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
			WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] GetQueuedCompletionStatus FAIL - %d(%x)"), GetLastError(), hr);

			HANDLE hPort = pCDriverHandler->m_hPort;
			hr = FilterConnectCommunicationPort(IOCP_PORT_NAME, 0, NULL, 0, NULL, &hPort);
			if (hr != S_OK)
			{
				WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] FilterConnectCommunicationPort Err - 0x%08x"), hr);
				continue;
			}

			pCDriverHandler->m_hCompletion = CreateIoCompletionPort(hPort, NULL, 0, SECUWORKS_DEFAULT_THREAD_COUNT);
			if(pCDriverHandler->m_hCompletion == NULL)
			{
				CloseHandle(hPort);
				hr = HRESULT_FROM_WIN32(GetLastError());
				WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] CreateIoCompletionPort Err - 0x%08x"), hr);
				continue;
			}

			//EnterCriticalSection(&pCDriverHandler->m_hPortCS);
			pCDriverHandler->m_hPort = hPort;
			//LeaveCriticalSection(&pCDriverHandler->m_hPortCS);
			continue;
			*/
		}

		long st = GetTickCount();

		pLogMsg = CONTAINING_RECORD(pOvlp, GRADIUS_LOG_MESSAGE, Ovlp);
		//WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] Rcvd. pLogMsg=%x"), pLogMsg);

		pLAFromDriver = &pLogMsg->la;
		memset(&pLogMsg->Ovlp, 0, sizeof(OVERLAPPED));

		//WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] about to FilterGetMessage"));
		HRESULT hr = FilterGetMessage(pCDriverHandler->m_hPort, &pLogMsg->MessageHeader, FIELD_OFFSET(GRADIUS_LOG_MESSAGE, Ovlp), &pLogMsg->Ovlp);
		if(hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING))
		{
			WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] FilterGetMessage FAIL - 0x%x"), hr);
			continue;
		}

		PIOCP_MSG_FROM_DRIVER_LOG pLog = new IOCP_MSG_FROM_DRIVER_LOG;
		memset(pLog, 0, sizeof(IOCP_MSG_FROM_DRIVER_LOG));
		memcpy(pLog, pLAFromDriver, sizeof(IOCP_MSG_FROM_DRIVER_LOG));

		replyMsg.ReplyHeader.Status = 0;
		replyMsg.ReplyHeader.MessageId = pLogMsg->MessageHeader.MessageId;
		replyMsg.Reply.SafeToOpen = bResult;

		hr = FilterReplyMessage(pCDriverHandler->m_hPort, (PFILTER_REPLY_HEADER)&replyMsg, sizeof(replyMsg.ReplyHeader) + sizeof(replyMsg.Reply));
		if (hr != S_OK)
		{
			WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] FilterReplyMessage FAIL - 0x%x"), hr);
		}
		//WRITELOG(_T("[CMiniFilterManager::ReadFilterMsgThread] Rcvd & Replied. eplaps=%d"), GetTickCount() - st);

		pCDriverHandler->AddTailLogList(pLog);
		if (pCDriverHandler->m_hEvent != NULL)
		{
			SetEvent(pCDriverHandler->m_hEvent);
		}
	}

	FREE_MEMORY(pLogMsg);
	pCDriverHandler->m_hPort = INVALID_HANDLE_VALUE;
	pCDriverHandler->CleanupConnection();
	WRITELOG(_T("[ReadFilterMsgThread] End."));

	return 0;
}

void CMiniFilterManager::StartReadFilterMsgThread()
{
	WRITELOG(_T("[CMiniFilterManager::StartReadFilterMsgThread] Start"));

	//HANDLE hThreads[SECUWORKS_DEFAULT_THREAD_COUNT];
	int nThreadCnt = 0;
	PGRADIUS_LOG_MESSAGE pSecuworksLog = NULL;

	for(; nThreadCnt<SECUWORKS_DEFAULT_THREAD_COUNT; nThreadCnt++) 
	{
		m_hThreads  = (HANDLE)_beginthreadex(NULL, 0, ReadFilterMsgThread, this, 0, NULL);
		if (m_hThreads == NULL)
		{
			WRITELOG(_T("[CMiniFilterManager::StartReadFilterMsgThread] Failed create thread. nThreadCnt=%d, error=%d"), nThreadCnt, GetLastError());
			goto StartReadFilterMsgThreadCleanUp;
		}
		WRITELOG(_T("[CMiniFilterManager::StartReadFilterMsgThread] _beginthreadex"));


		for(int nReqCnt=0; nReqCnt<SECUWORKS_DEFAULT_REQUEST_COUNT; nReqCnt++)
		{
			pSecuworksLog = (PGRADIUS_LOG_MESSAGE)malloc(sizeof(GRADIUS_LOG_MESSAGE));
			WRITELOG(_T("[CMiniFilterManager::StartReadFilterMsgThread] pSecuworksLog=%x, size=%d"), pSecuworksLog, sizeof(GRADIUS_LOG_MESSAGE));
			if(pSecuworksLog == NULL)
			{
				WRITELOG(_T("[CMiniFilterManager::StartReadFilterMsgThread] Failed malloc pSecuworksLog. nThreadCnt=%d, nReqCnt=%d, error=%d"), nThreadCnt, nReqCnt, GetLastError());
				goto StartReadFilterMsgThreadCleanUp;
			}

			memset(&pSecuworksLog->Ovlp, 0, sizeof(OVERLAPPED));

			HRESULT hr = FilterGetMessage(m_hPort, &pSecuworksLog->MessageHeader, FIELD_OFFSET(GRADIUS_LOG_MESSAGE, Ovlp), &pSecuworksLog->Ovlp);
			WRITELOG(_T("[CMiniFilterManager::StartReadFilterMsgThread] FilterGetMessage result=%x"), hr);
			if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING))
			{
				WRITELOG(_T("[CMiniFilterManager::StartReadFilterMsgThread] FilterGetMessage - %d"), hr);
				FREE_MEMORY(pSecuworksLog);
				goto StartReadFilterMsgThreadCleanUp;
			}

		}

		CloseHandle(m_hThreads);
		m_hThreads = INVALID_HANDLE_VALUE;
	}
	WRITELOG(_T("[CMiniFilterManager::StartReadFilterMsgThread] End"));

	// WaitForMultipleObjectsEx(nThreadCnt, hThreads, TRUE, INFINITE, FALSE);

StartReadFilterMsgThreadCleanUp:
	CloseHandle(m_hThreads);
	m_hThreads = INVALID_HANDLE_VALUE;
	return;
// 	if(pSecuworksLog != NULL)
// 	{
// 		free(pSecuworksLog);
// 	}

 	
// 	CloseServiceHandle(m_hCompletion);
}


DWORD CMiniFilterManager::GetLogListCount()
{ 
	DWORD dwRet = 0;
	EnterCriticalSection(&m_LogListCS);
	dwRet = m_LogList.GetCount();
	LeaveCriticalSection(&m_LogListCS);

	return dwRet;
}

void CMiniFilterManager::GetLogDataAtHead(PIOCP_MSG_FROM_DRIVER_LOG *pLA)
{
	EnterCriticalSection(&m_LogListCS);
	*pLA = m_LogList.GetHead();
	LeaveCriticalSection(&m_LogListCS);
}

void CMiniFilterManager::RemoveLogListHead(PIOCP_MSG_FROM_DRIVER_LOG *pLA)
{
	EnterCriticalSection(&m_LogListCS);
	*pLA = m_LogList.RemoveHead();
	LeaveCriticalSection(&m_LogListCS);
}

// \Device\Mup\192.168.1.61\Shared\A.xlsx
#define NETWORK_PATH_HEADER		_T("Device\\Mup:")
void CMiniFilterManager::RemoveNetworkHeaderInPath(PWCHAR p)
{
	if (p == NULL) return;

	int len_header = wcslen(NETWORK_PATH_HEADER);
	if (wcsncmp(p + 1, NETWORK_PATH_HEADER, len_header) != 0) return;

	wcsncpy(p + 1, p + len_header + 1, (ULONG)(wcslen(p) - len_header));
}

int CMiniFilterManager::GetFileEvents(PIOCP_MSG_FROM_DRIVER_LOG *pEvents)
{
	int cnt = GetLogListCount();
	if (cnt == 0) return 0;

	int data_len = sizeof(IOCP_MSG_FROM_DRIVER_LOG);
	*pEvents = (PIOCP_MSG_FROM_DRIVER_LOG)malloc(cnt * data_len);
	//WRITELOG(_T("[GetFileEvents] *pEvents=%x, size=%d"), *pEvents, cnt*data_len);
	if (*pEvents == NULL)
	{
		return -1;
	}

	for (int i = 0; i < cnt; i++)
	{
		PIOCP_MSG_FROM_DRIVER_LOG pLA;
		RemoveLogListHead(&pLA);
		if (pLA == NULL) continue;

		if (pLA->wszFilePath[0] == '\\')
		{
			RemoveNetworkHeaderInPath(pLA->wszFilePath);
		}
		if (pLA->wszBeforeFilePath[0] == '\\')
		{
			WRITELOG(_T("wszBeforeFilePath : %s"), pLA->wszBeforeFilePath);
			RemoveNetworkHeaderInPath(pLA->wszBeforeFilePath);
		}
		
		PBYTE pDest = (PBYTE)*pEvents + data_len*i;
		void *pSrc = pLA;
		memcpy(pDest, pSrc, data_len);

		delete pLA;
		pLA = NULL;
	}

	return cnt;
}