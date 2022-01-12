#include "stdafx.h"


/*
CAtlMap<CAtlString, int> g_NetworkDriveMap;

CString NetworkPathToDrivePath(TCHAR *bufPath)
{
CString sDrvPath = bufPath;

DWORD drives = GetLogicalDrives();
for (int drive_index = 0; drives != 0; drive_index++)
{
if (drives & 1)
{
CString sDriveRoot = _T("");
sDriveRoot.Format(_T("%c:\\"), 'A' + drive_index);
int drive_type = GetDriveType(sDriveRoot);

if (drive_type == DRIVE_REMOTE)
{
WCHAR Buffer[1024];
DWORD dwBufferLength = 1024;
UNIVERSAL_NAME_INFO * unameinfo;
REMOTE_NAME_INFO *remotenameinfo;

unameinfo = (UNIVERSAL_NAME_INFO *)&Buffer;
DWORD dwRetVal = WNetGetUniversalName(sDriveRoot, REMOTE_NAME_INFO_LEVEL, (LPVOID)unameinfo, &dwBufferLength);
if (dwRetVal == NO_ERROR)
{
// bufPath: "\Device\Mup\192.168.1.61\vm\abc.txt"
// lpUniversalName: "\\192.168.1.61\vm\"
unameinfo->lpUniversalName += 2;
//WRITELOG(_T("[NetworkPathToDrivePath] s1=%s"), bufPath + _tcslen(NETWORK_PATH_HEADER));
//WRITELOG(_T("[NetworkPathToDrivePath] s2=%s"), unameinfo->lpUniversalName);
//WRITELOG(_T("[NetworkPathToDrivePath] len1=%d"), _tcslen(bufPath) - _tcslen(NETWORK_PATH_HEADER));
//WRITELOG(_T("[NetworkPathToDrivePath] len2=%d"), _tcslen(unameinfo->lpUniversalName) );

int result_cmp = wcsncmp(bufPath + _tcslen(NETWORK_PATH_HEADER), unameinfo->lpUniversalName,
min(_tcslen(bufPath) - _tcslen(NETWORK_PATH_HEADER), _tcslen(unameinfo->lpUniversalName)));
//WRITELOG(_T("[NetworkPathToDrivePath] univeralName=%s, drive=%s, compare_result=%d"), unameinfo->lpUniversalName, sDriveRoot, result_cmp);

if (result_cmp == 0)
{
sDrvPath.Format(_T("%s%s"), sDriveRoot, bufPath + _tcslen(NETWORK_PATH_HEADER) + _tcslen(unameinfo->lpUniversalName));
WRITELOG(_T("[NetworkPathToDrivePath] univeralName=[%s] -> drive=[%s]"), unameinfo->lpUniversalName, sDriveRoot);
}

//sDriveInfo.Format(_T("%s"), unameinfo->lpUniversalName);
}
}
}
drives >>= 1;
}
return sDrvPath;
}

CAtlString NetworkPathToDrivePath2(CAtlString sFilePath)
{
CString sNewFilePath = sFilePath;

WRITELOG(_T("[NetworkPathToDrivePath2] sFilePath=%s"), sFilePath);
int drv_index = -1;
POSITION pos = g_NetworkDriveMap.GetStartPosition();
while (pos)
{
g_NetworkDriveMap.GetNext(pos);
CAtlString sPath = g_NetworkDriveMap.GetKeyAt(pos);
WRITELOG(_T("[NetworkPathToDrivePath2] sPath(pos)=%s"), sPath);
if (sFilePath.Find(sPath) == 0)
{
drv_index = g_NetworkDriveMap.GetValueAt(pos);
sFilePath.Delete(0, sPath.GetLength()-1);
sNewFilePath.Format(_T("%c:%s"), 'A' + drv_index, sFilePath);
WRITELOG(_T("[NetworkPathToDrivePath2] Match!! index=%d, newPath=%s"), drv_index, sNewFilePath);
}
}

return sNewFilePath;
}

#define MAX_UNIVERSAL_NAME_LENGTH	1024
void CFilterDriverLoaderDlg::UpdateDriveSet()
{
g_NetworkDriveMap.RemoveAll();

DWORD drives = GetLogicalDrives();
for (int drive_index = 0; drives != 0; drive_index++)
{
if (drives & 1)
{
CString sDriveRoot = _T("");
sDriveRoot.Format(_T("%c:\\"), 'A' + drive_index);
int drive_type = GetDriveType(sDriveRoot);

if (drive_type == DRIVE_REMOTE)
{
WCHAR Buffer[MAX_UNIVERSAL_NAME_LENGTH];
DWORD dwBufferLength = MAX_UNIVERSAL_NAME_LENGTH;
UNIVERSAL_NAME_INFO *unameinfo;
unameinfo = (UNIVERSAL_NAME_INFO *)&Buffer;
DWORD dwRetVal = WNetGetUniversalName(sDriveRoot, REMOTE_NAME_INFO_LEVEL, (LPVOID)unameinfo, &dwBufferLength);
if (dwRetVal == NO_ERROR)
{
CAtlString sDriveUniveralName;
sDriveUniveralName.Format(_T("%s"), unameinfo->lpUniversalName);
g_NetworkDriveMap.SetAt(sDriveUniveralName, drive_index);
WRITELOG(_T("[UpdateDriveSet] %s : %d (%ws)"), sDriveRoot, drive_type, sDriveUniveralName);
}
}
}
// ��� 1��Ʈ �̵�(= ���� ����̺� üũ)
drives >>= 1;
//m_nDrivePos++;
}

}

// DeviceChange �޽���(OLD : XP�� ���� ��ƾ)
LRESULT CFilterDriverLoaderDlg::OnDriveSetChanged(WPARAM wParam, LPARAM lParam)
{
WriteLog(_T("[OnDeviceSetChanged] Start"));
UpdateDriveSet();

return 0L;
}
*/