#pragma once
#include <atlstr.h>


class CEnablePrivilege
{
public:
	CEnablePrivilege(void);
	~CEnablePrivilege(void);
public:
	BOOL EnablePrivilege(LPCTSTR szPrivilege);
	BOOL EnablePrivilege(LPCTSTR szPrivilege, DWORD dwPID);
};