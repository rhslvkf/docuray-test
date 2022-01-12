#include "stdafx.h"
#include "FilterDriverLoaderDlg.h"

#define LOGGER_BUFFER_SIZE 1024*20
void CFilterDriverLoaderDlg::WriteLog(const TCHAR *format_string, ...)
{
#ifndef RENEWAL
	va_list args;
	DWORD buffer_size = 10240;
	TCHAR s_logbuf[LOGGER_BUFFER_SIZE];

	va_start(args, format_string);
	buffer_size = _vsntprintf_s(s_logbuf, buffer_size / sizeof(TCHAR), format_string, args);
	va_end(args);

	s_logbuf[buffer_size] = 0;

	CString sTime;
	SYSTEMTIME time;
	GetLocalTime(&time);
	long tick = GetCurrentTime();
	sTime.Format(_T("[%02d:%02d:%02d.%03d]"), time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

	CString sLog;
	m_edtLog.GetWindowText(sLog);
	sLog += sTime + s_logbuf;
	sLog += "\r\n";
	m_edtLog.SetWindowTextW(sLog);
	m_edtLog.LineScroll(m_edtLog.GetLineCount());
#endif
}

void CFilterDriverLoaderDlg::WriteLogD(const TCHAR *format_string, ...)
{
	va_list args;
	DWORD buffer_size = 10240;
	TCHAR s_logbuf[LOGGER_BUFFER_SIZE];

	va_start(args, format_string);
	buffer_size = _vsntprintf_s(s_logbuf, buffer_size / sizeof(TCHAR), format_string, args);
	va_end(args);

	s_logbuf[buffer_size] = 0;
#ifdef RENEWAL
	CAtlString sLineData = s_logbuf;
	g_cFilterDriverData.SetListCtrlData(sLineData);

	CString sTime = _T("");
	AfxExtractSubString(sTime, sLineData, 0, _T('|'));

	CString sReaction = _T("");
	AfxExtractSubString(sReaction, sLineData, 1, _T('|'));

	CString sProcessName = _T("");
	AfxExtractSubString(sProcessName, sLineData, 2, _T('|'));

	CString sPID = _T("");
	AfxExtractSubString(sPID, sLineData, 3, _T('|'));

	CString sTargetFilePath = _T("");
	AfxExtractSubString(sTargetFilePath, sLineData, 4, _T('|'));

	CString sDestinationPath = _T("");
	AfxExtractSubString(sDestinationPath, sLineData, 5, _T('|'));
	
	// by piero1452 filtering
	CAtlString sTotalFilePath = sTargetFilePath + sDestinationPath;
	if (FilterManager(sProcessName, sTotalFilePath) == TRUE)
	{
		int nIndex = m_lstEvD.GetItemCount();
		m_lstEvD.InsertItem(LVIF_TEXT, nIndex, sTime, 0, 0, 0, 0);
		m_lstEvD.SetItem(nIndex, 1, LVIF_TEXT, sReaction, 0, 0, 0, 0);
		m_lstEvD.SetItem(nIndex, 2, LVIF_TEXT, sProcessName, 0, 0, 0, 0);
		m_lstEvD.SetItem(nIndex, 3, LVIF_TEXT, sPID, 0, 0, 0, 0);
		m_lstEvD.SetItem(nIndex, 4, LVIF_TEXT, sTotalFilePath, 0, 0, 0, 0);

		if (m_bAutoScrollD) 
		{
			CRect cRect;
			m_lstEvD.GetItemRect(0, cRect, LVIR_BOUNDS);
			CSize szHeight(0, cRect.Height() * nIndex);
			m_lstEvD.Scroll(szHeight);
			m_lstEvD.SetScrollPos(SB_VERT, nIndex, FALSE);
		}
	}
#else
	CString sTime;
	SYSTEMTIME time;
	GetLocalTime(&time);
	long tick = GetCurrentTime();
	sTime.Format(_T("[%02d:%02d:%02d.%03d]"), time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

	int ix = m_lstEvD.InsertString(-1, sTime + s_logbuf);
	if (m_bAutoScrollD) m_lstEvD.SetTopIndex(ix);
#endif
}

void CFilterDriverLoaderDlg::WriteLogA(const TCHAR *format_string, ...)
{
#ifndef RENEWAL
	va_list args;
	DWORD buffer_size = 10240;
	TCHAR s_logbuf[LOGGER_BUFFER_SIZE];

	va_start(args, format_string);
	buffer_size = _vsntprintf_s(s_logbuf, buffer_size / sizeof(TCHAR), format_string, args);
	va_end(args);

	s_logbuf[buffer_size] = 0;

	CString sTime;
	SYSTEMTIME time;
	GetLocalTime(&time);
	long tick = GetCurrentTime();
	sTime.Format(_T("[%02d:%02d:%02d.%03d]"), time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

	m_lstEvA.InsertString(-1, sTime + s_logbuf);
#endif
}

BOOL CFilterDriverLoaderDlg::FilterManager(CAtlString sProcessName, CAtlString sTotalFilePath)
{
	CAtlString sFilterFilePath = _T(""), sFilterProcessData = _T("");
	sFilterFilePath = g_cFilterDriverData.GetFilterFilePathData();
	sFilterProcessData = g_cFilterDriverData.GetFilterProcessData();
	if (sFilterFilePath.IsEmpty() && sFilterProcessData.IsEmpty())
	{
		return TRUE;
	}

	if (sFilterFilePath.IsEmpty())
	{
		if (sProcessName.Find(sFilterProcessData) > -1)
		{
			return TRUE;
		}
	}

	if (sFilterProcessData.IsEmpty())
	{
		if (sTotalFilePath.Find(sFilterFilePath) > -1)
		{
			return TRUE;
		}
	}

	if (sProcessName.Find(sFilterProcessData) > -1 && sTotalFilePath.Find(sFilterFilePath) > -1)
	{
		return TRUE;
	}

	return FALSE;
}