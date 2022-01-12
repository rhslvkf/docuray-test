#include "stdafx.h"
#include "FilterDriverLoaderDlg.h"

extern CFilterDriverLoaderDlg *g_pMain;
extern HANDLE g_hAPIEventThreadC;
extern HANDLE g_hAPIEventThreadD;

#define API_BUFFER_SIZE			65536
#define API_SEARCH_ROOT_C		L"C:\\"
#define API_SEARCH_ROOT_D		L"D:\\"
DWORD WINAPI APIEventReadThreadC(LPVOID arg)
{
	WRITELOG(_T("[APIEventReadThread] Start."));
	try
	{
		HANDLE hDir = CreateFileW(API_SEARCH_ROOT_C, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		BYTE pBuffer[API_BUFFER_SIZE];
		//BYTE* pBuffer = (PBYTE)malloc(cbBuffer);
		BOOL bWatchSubtree = TRUE;
		DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;
		DWORD bytesReturned;
		WCHAR temp[MAX_PATH] = { 0 };
		WCHAR action[16] = { 0 };

		for (;;)
		{
			FILE_NOTIFY_INFORMATION* pfni;
			BOOL fOk = ReadDirectoryChangesW(hDir, pBuffer, API_BUFFER_SIZE, bWatchSubtree, dwNotifyFilter, &bytesReturned, 0, 0);
			if (!fOk)
			{
				DWORD dwLastError = GetLastError();
				WRITELOG(_T("[APIEventReadThread] Error : %d"), dwLastError);
				break;
			}

			pfni = (FILE_NOTIFY_INFORMATION*)pBuffer;

			int cnt = 1;
			do
			{
				printf("NextEntryOffset(%d)\n", pfni->NextEntryOffset);
				switch (pfni->Action)
				{
				case FILE_ACTION_ADDED:
					wsprintf(action, L"Created");
					break;
				case FILE_ACTION_REMOVED:
					wsprintf(action, L"Removed");
					break;
				case FILE_ACTION_MODIFIED:
					wsprintf(action, L"Modified");
					break;
				case FILE_ACTION_RENAMED_OLD_NAME:
					wsprintf(action, L"Renamed");
					break;
				default:
					wsprintf(action, L"Unknown");
					break;
				}
				memset(temp, 0, MAX_PATH);
				wcsncpy_s(temp, pfni->FileName, min(pfni->FileNameLength / 2, MAX_PATH));

				if (pfni->Action == FILE_ACTION_RENAMED_OLD_NAME)
				{
					pfni = (FILE_NOTIFY_INFORMATION*)((PBYTE)pfni + pfni->NextEntryOffset);

					WCHAR temp_new[MAX_PATH] = { 0 };
					memset(temp_new, 0, MAX_PATH);
					wcsncpy_s(temp_new, pfni->FileName, min(pfni->FileNameLength / 2, MAX_PATH));
					WRITEALOG(_T("@@[%d][%s] %s%s (<- %s%s)"), cnt++, action, API_SEARCH_ROOT_C, temp_new, API_SEARCH_ROOT_C, temp);
				}
				else
				{
					WRITEALOG(_T("@@[%d][%s] %s%s"), cnt++, action, API_SEARCH_ROOT_C, temp);
				}


				pfni = (FILE_NOTIFY_INFORMATION*)((PBYTE)pfni + pfni->NextEntryOffset);
			} while (pfni->NextEntryOffset > 0);
		}

	}
	catch (...)
	{
		WRITELOG(_T("[APIEventReadThread] exception!!! - %ld"), GetLastError());
	}
	WRITELOG(_T("[APIEventReadThread] End."));
	return  0;
}
DWORD WINAPI APIEventReadThreadD(LPVOID arg)
{
	WRITELOG(_T("[APIEventReadThread] Start."));
	try
	{
		HANDLE hDir = CreateFileW(API_SEARCH_ROOT_D, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		BYTE pBuffer[API_BUFFER_SIZE];
		BOOL bWatchSubtree = TRUE;
		DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;
		DWORD bytesReturned;
		WCHAR temp[MAX_PATH] = { 0 };
		WCHAR action[16] = { 0 };

		for (;;)
		{
			FILE_NOTIFY_INFORMATION* pfni;
			BOOL fOk = ReadDirectoryChangesW(hDir, pBuffer, API_BUFFER_SIZE, bWatchSubtree, dwNotifyFilter, &bytesReturned, 0, 0);
			if (!fOk)
			{
				DWORD dwLastError = GetLastError();
				WRITELOG(_T("[APIEventReadThread] Error : %d"), dwLastError);
				break;
			}

			pfni = (FILE_NOTIFY_INFORMATION*)pBuffer;

			int cnt = 1;
			do
			{
				printf("NextEntryOffset(%d)\n", pfni->NextEntryOffset);
				switch (pfni->Action)
				{
				case FILE_ACTION_ADDED:
					wsprintf(action, L"Created");
					break;
				case FILE_ACTION_REMOVED:
					wsprintf(action, L"Removed");
					break;
				case FILE_ACTION_MODIFIED:
					wsprintf(action, L"Modified");
					break;
				case FILE_ACTION_RENAMED_OLD_NAME:
					wsprintf(action, L"Renamed");
					break;
				default:
					wsprintf(action, L"Unknown");
					break;
				}
				memset(temp, 0, MAX_PATH);
				wcsncpy_s(temp, pfni->FileName, min(pfni->FileNameLength / 2, MAX_PATH));

				if (pfni->Action == FILE_ACTION_RENAMED_OLD_NAME)
				{
					pfni = (FILE_NOTIFY_INFORMATION*)((PBYTE)pfni + pfni->NextEntryOffset);

					WCHAR temp_new[MAX_PATH] = { 0 };
					memset(temp_new, 0, MAX_PATH);
					wcsncpy_s(temp_new, pfni->FileName, min(pfni->FileNameLength / 2, MAX_PATH));
					WRITEALOG(_T("@@[%d][%s] %s%s (<- %s%s)"), cnt++, action, API_SEARCH_ROOT_D, temp_new, API_SEARCH_ROOT_D, temp);
				}
				else
				{
					WRITEALOG(_T("@@[%d][%s] %s%s"), cnt++, action, API_SEARCH_ROOT_D, temp);
				}


				pfni = (FILE_NOTIFY_INFORMATION*)((PBYTE)pfni + pfni->NextEntryOffset);
			} while (pfni->NextEntryOffset > 0);
		}

	}
	catch (...)
	{
		WRITELOG(_T("[APIEventReadThread] exception!!! - %ld"), GetLastError());
	}
	WRITELOG(_T("[APIEventReadThread] End."));
	return  0;
}

void CFilterDriverLoaderDlg::StartAPIEventReadThread()
{
	if (g_hAPIEventThreadC || g_hAPIEventThreadD) StopAPIEventReadThread();

	DWORD thread_id;
	//g_hAPIEventThreadC = CreateThread(NULL, 0, APIEventReadThreadC, this, 0, &thread_id);
	g_hAPIEventThreadD = CreateThread(NULL, 0, APIEventReadThreadD, this, 0, &thread_id);
}

void CFilterDriverLoaderDlg::StopAPIEventReadThread()
{
	if (g_hAPIEventThreadC != NULL)
	{
		TerminateThread(g_hAPIEventThreadC, 0);
		CloseHandle(g_hAPIEventThreadC);
		g_hAPIEventThreadC = NULL;
	}
	if (g_hAPIEventThreadD != NULL)
	{
		TerminateThread(g_hAPIEventThreadD, 0);
		CloseHandle(g_hAPIEventThreadD);
		g_hAPIEventThreadD = NULL;
	}
}