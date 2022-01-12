#include "stdafx.h"
#include "FilterDriverLoaderDlg.h"

extern CFilterDriverLoaderDlg *g_pMain;
extern HANDLE g_hDriverEvent;
extern HANDLE g_hDriverEventThread;

DWORD WINAPI DrvEventReadThread(LPVOID arg)
{
	CMiniFilterManager *pDrvManager = (CMiniFilterManager*)arg;
	DWORD dwModulePID = 0;
	::GetWindowThreadProcessId(g_pMain->m_hWnd, &dwModulePID);
	WRITELOG(_T("[DrvEventReadThread] Start."));
	try
	{
		while (TRUE)
		{
			//wait...
			DWORD ret = WaitForSingleObject(g_hDriverEvent, INFINITE);
			if (ret == WAIT_FAILED)
			{
				WRITELOG(_T("WaitForSingleObject Failed. error=%d"), GetLastError());
				return 0;
			}
			else if (ret == WAIT_ABANDONED)
			{
				ResetEvent(g_hDriverEvent);
				continue;
			}
			else if (ret == WAIT_TIMEOUT)
			{
				continue;
			}

			ResetEvent(g_hDriverEvent);

			PIOCP_MSG_FROM_DRIVER_LOG pEvents = NULL;
			int cnt = pDrvManager->GetFileEvents(&pEvents);

			if (cnt > 0 && pEvents != NULL)
			{
				for (int i = 0; i < cnt; i++)
				{
					PIOCP_MSG_FROM_DRIVER_LOG event = &pEvents[i];
					
					DWORD dwTargetPID = (DWORD)event->uProcessID;
					if(dwTargetPID == dwModulePID)
					{
						continue;
					}

					if (g_pMain->m_bViewBMSEvent == FALSE && g_pMain->DefaultFiltering(dwTargetPID, event->wszProcessName) == FALSE)
					{
						continue;
					}

					CString sAction = L"";
					if (event->uEventFlags & DRIVER_EVENT_FLAG_CREATE) sAction += L"Created";
					if (event->uEventFlags & DRIVER_EVENT_FLAG_MODIFY) { if (sAction.GetLength() > 0) sAction += ",";  sAction += L"Modified"; }
					if (event->uEventFlags & DRIVER_EVENT_FLAG_RENAME) { if (sAction.GetLength() > 0) sAction += ",";  sAction += L"Renamed"; }
					if (event->uEventFlags & DRIVER_EVENT_FLAG_RENAME_EX) { if (sAction.GetLength() > 0) sAction += ",";  sAction += L"Renamed(Ex)"; }
					if (event->uEventFlags & DRIVER_EVENT_FLAG_REMOVE) { if (sAction.GetLength() > 0) sAction += ",";  sAction += L"Removed"; }
					if (event->uEventFlags & DRIVER_EVENT_FLAG_READ) { if (sAction.GetLength() > 0) sAction += ",";  sAction += L"Read"; }
					if (event->uEventFlags & DRIVER_EVENT_FLAG_EXECUTE) { if (sAction.GetLength() > 0) sAction += ",";  sAction += L"Excute"; }

					CString sTargetFilePath = L"", sBeforeFilePath = L"", sDest = L"";
					sTargetFilePath = event->wszFilePath;
					sBeforeFilePath = event->wszBeforeFilePath;

					if (event->uEventFlags & DRIVER_EVENT_FLAG_RENAME || event->uEventFlags & DRIVER_EVENT_FLAG_RENAME_EX)
					{
						sDest.Format(_T(" (<- %s)"), sBeforeFilePath);
					}

					BOOL bShowLog = TRUE;

					//bShowLog = FALSE;
					//if (event->uEventFlags & DRIVER_EVENT_FLAG_EXECUTE)
					//{
					//	bShowLog = TRUE;
					//}
					
					// Testing
					//bShowLog = FALSE;
					//if (event->uEventFlags & DRIVER_EVENT_FLAG_READ)
					//{
					//	CString sPath = event->wszFilePath;
					//	sPath = sPath.MakeLower();
					//	if (sPath.Find(L"d:\\work\\codeguardtest\\st300ri_v1040\\") == 0) bShowLog = TRUE;
					//}
					// Testing
#ifdef RENEWAL
					SYSTEMTIME time; GetLocalTime(&time); 
					WRITEDLOG(_T("[%02d:%02d:%02d.%03d]|[%d] %s|%s|%d|%s%s"), 
						time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
						i + 1, sAction, 
						event->wszProcessName,
						event->uProcessID,
						sTargetFilePath,
						sDest);
#else
					if (bShowLog)
					{
						WRITEDLOG(_T("##[%d][%s,%d][%s] %s%s"),
						i + 1,
						sAction,
						event->uProcessID,
						event->wszProcessName,
						sTargetFilePath,
						sDest
						);
					}
#endif
				}

				free(pEvents);
				pEvents = NULL;
			}
		}
	}
	catch (...)
	{
		WRITELOG(_T("[FolderChangeMonitorThread] exception!!! - %ld"), GetLastError());
	}

	WRITELOG(_T("[DrvEventReadThread] End."));
	return  0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// MiniFilter Operations

BOOL CFilterDriverLoaderDlg::StartDriver()
{
	//m_FilterManager.SetCallback(&OnFilterEvent);

	DWORD dwRet = m_FilterManager.SafeStartFilterDriver(TRUE);
	WRITELOG(_T("[LoadDriver] MiniFilter driver started. dwRet=%d"), dwRet);
	if (dwRet == MINIFILTER_LOAD_SUCCESS)
	{
		WRITELOG(_T("[LoadDriver] ����̹� �ε� ����!"));
		m_bMiniFilterLoaded = TRUE;
		return TRUE;
	}

	if (dwRet == MINIFILTER_LOAD_ERROR_ACCESS_DENIED)
	{
		// UAC���� user ������ ��� �� ������ �߻��� �� �ִ�.
		WRITELOG(_T("[LoadDriver] ����̹� �ε� ����: ���� ������ MiniFilter�� load���� ����"));
	}
	else if (dwRet != MINIFILTER_LOAD_ERROR_NO_DRIVER_FILE)
	{
		// ������ �����ϴµ� �ε忡 ������ ���� �ٽ� �õ��ؾ� �ϹǷ� ���� ó���ؾ� ������
		// ��Ȯ�� ���ε��� �ľ� �� ����, ������ �Ǳ� �������� ���� ó������ �ʰ� skip�Ѵ�.
		WRITELOG(_T("[LoadDriver] ����̹� �ε� ����: �� �� ���� ����: %d"), dwRet);
	}
	return FALSE;
}

BOOL CFilterDriverLoaderDlg::StopDriver()
{
	BOOL ret = m_FilterManager.StopFilterDriver();
	WRITELOG(_T("[UnloadDriver] StopFilterDriver ȣ��:%d"), ret);
	if (ret == FALSE)
	{
		WRITELOG(_T("[UnloadDriver] StopFilterDriver ����:0x%0x"), GetLastError());
		return FALSE;
	}

	m_FilterManager.UnRegisterFilterDriverServ();
	WRITELOG(_T("[UnloadDriver] UnRegisterFilterDriverServ() ȣ��:%d"), ret);
	if (ret == FALSE)
	{
		WRITELOG(_T("[UnloadDriver] UnRegisterFilterDriverServ ����:0x%0x"), GetLastError());
		return FALSE;
	}

	WRITELOG(_T("[UnloadDriver] ����̹� Unload ����!"));
	m_bMiniFilterLoaded = FALSE;

	return TRUE;
}

BOOL CFilterDriverLoaderDlg::ConnectToDriver()
{
	DWORD ret = m_FilterManager.ConnectCommPort();
	if (SUCCEEDED(ret))
	{
		WRITELOG(_T("[ConnectToDriver] ����̹� Connect ����!"));
		return TRUE;
	}
	return FALSE;
}

void ResetDriverConnection()
{

}

void CFilterDriverLoaderDlg::SendMsgToDriver(CString strSend)
{
	DWORD dwRet = m_FilterManager.ConnectCommPort();
	WRITELOG(_T("[SendFltMessage] ConnectCommPort result=0x%x"), dwRet);

	TCHAR bufApp[MAX_PATH];
	wcsncpy_s(bufApp, strSend, strSend.GetLength());
	HRESULT hr = m_FilterManager.SendTextToDriver(MINIFILTER_CMD_SEND_TEXT, bufApp, _tcslen(bufApp));
	WRITELOG(_T("[SendFltMessage] SendFltMessage result=0x%x"), hr);
}

void CFilterDriverLoaderDlg::StartDriverEventReadThread()
{
	WRITELOG(_T("[StartDriverEventReadThread] Start"));
	if (g_hDriverEventThread) StopDriverEventReadThread();

	g_hDriverEvent = CreateEvent(0, FALSE, FALSE, 0);
	WRITELOG(_T("[StartDriverEventReadThread] g_hDriverEvent=%x"), g_hDriverEvent);
	m_FilterManager.SetEventHandle(g_hDriverEvent);

	DWORD thread_id;
	g_hDriverEventThread = CreateThread(NULL, 0, DrvEventReadThread, &m_FilterManager, 0, &thread_id);
	WRITELOG(_T("[StartDriverEventReadThread] End. g_hDriverEventThread=%x"), g_hDriverEventThread);
}

void CFilterDriverLoaderDlg::StopDriverEventReadThread()
{
	WRITELOG(_T("[StopDriverEventReadThread] Start"));
	if (g_hDriverEventThread != NULL)
	{
		TerminateThread(g_hDriverEventThread, 0);
		CloseHandle(g_hDriverEventThread);
		g_hDriverEventThread = NULL;
	}
	if (g_hDriverEvent != NULL)
	{
		CloseHandle(g_hDriverEvent);
		g_hDriverEvent = NULL;
	}
	WRITELOG(_T("[StopDriverEventReadThread] End"));
}