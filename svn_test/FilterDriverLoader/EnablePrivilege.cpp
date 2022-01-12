#include "stdafx.h"
#include "EnablePrivilege.h"


#include "FilterDriverLoaderDlg.h"
extern CFilterDriverLoaderDlg *g_pMain;

CEnablePrivilege::CEnablePrivilege(void)
{
}


CEnablePrivilege::~CEnablePrivilege(void)
{
}

BOOL CEnablePrivilege::EnablePrivilege(LPCTSTR szPrivilege)
{
	BOOL bResult = FALSE;

	HANDLE hToken = NULL;
	if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken) == FALSE)
	{
		WRITELOG(_T("[EnablePrivilege] OpenProcessToken Err - %d"), GetLastError());
		return bResult; 
	}

	TOKEN_PRIVILEGES pTokenCurrent = {0,};
	pTokenCurrent.PrivilegeCount = 1;
	pTokenCurrent.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
	if(LookupPrivilegeValue(NULL, szPrivilege, &pTokenCurrent.Privileges[0].Luid) == TRUE)
	{
		DWORD dwTokenSize = sizeof(TOKEN_PRIVILEGES);
		DWORD dwActualSize = 0;
		TOKEN_PRIVILEGES tTokenOld = {0,};
		if(AdjustTokenPrivileges(hToken, FALSE, &pTokenCurrent, dwTokenSize, &tTokenOld, &dwActualSize) == TRUE)
		{
			bResult = TRUE;
		}
		else
		{
			bResult = FALSE;
			WRITELOG(_T("[EnablePrivilege] AdjustTokenPrivileges Err - %d"), GetLastError());
		}
	}
	else
	{
		bResult = FALSE; 
		WRITELOG(_T("[EnablePrivilege] LookupPrivilegeValue Err - %d"), GetLastError());
	}

	if (hToken != NULL)
	{
		CloseHandle(hToken);
	}

	WRITELOG(_T("[EnablePrivilege] EnablePrivilege(%s) End. result=%d"), szPrivilege, bResult);
	return bResult;
}

BOOL CEnablePrivilege::EnablePrivilege(LPCTSTR szPrivilege, DWORD dwPID)
{
	BOOL bResult = FALSE;

	HANDLE hProcess = ::OpenProcess(MAXIMUM_ALLOWED, FALSE, dwPID);

	HANDLE hToken = NULL;
	if(OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken) == FALSE)
	{
		WRITELOG(_T("[EnablePrivilege] OpenProcessToken Err - %d"), GetLastError());
		return bResult; 
	}

	TOKEN_PRIVILEGES pTokenCurrent = {0,};
	pTokenCurrent.PrivilegeCount = 1;
	pTokenCurrent.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
	if(LookupPrivilegeValue(NULL, szPrivilege, &pTokenCurrent.Privileges[0].Luid) == TRUE)
	{
		DWORD dwOld = sizeof(TOKEN_PRIVILEGES);
		TOKEN_PRIVILEGES tTokenOld = {0,};
		if(AdjustTokenPrivileges(hToken, FALSE, &pTokenCurrent, dwOld, &tTokenOld, &dwOld) == TRUE)
		{
			bResult = TRUE;
		}
		else
		{
			bResult = FALSE;
			WRITELOG(_T("[EnablePrivilege] AdjustTokenPrivileges Err - %d"), GetLastError());
		}
	}
	else
	{
		bResult = FALSE; 
		WRITELOG(_T("[EnablePrivilege] LookupPrivilegeValue Err - %d"), GetLastError());
	}

	CloseHandle(hToken);
	CloseHandle(hProcess);

	return bResult;
}