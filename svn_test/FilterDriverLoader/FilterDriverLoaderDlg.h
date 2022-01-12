
// FilterDriverLoaderDlg.h : ��� ����
//

#pragma once
#include "afxwin.h"
#include "resource.h"
#include "MiniFilterManager.h"

// CFilterDriverLoaderDlg ��ȭ ����
class CFilterDriverLoaderDlg : public CDialogEx
{
// �����Դϴ�.
public:
	CFilterDriverLoaderDlg(CWnd* pParent = NULL);	// ǥ�� �������Դϴ�.

// ��ȭ ���� �������Դϴ�.
#ifdef RENEWAL
	enum { IDD = IDD_RENEWAL_DEBUG_TOOL };
#else
	enum { IDD = IDD_FILTERDRIVERLOADER_DIALOG };
#endif
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV �����Դϴ�.

// �����Դϴ�.
protected:
	HICON m_hIcon;

	// ������ �޽��� �� �Լ�
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
	CEdit m_edtLog;
	
#ifdef RENEWAL
	CListCtrl m_lstEvD;
	CListCtrl m_lstEvA;
#else
	CListBox m_lstEvD;
	CListBox m_lstEvA;
#endif
	BOOL m_bAutoScrollD;
	BOOL m_bViewBMSEvent;
	CString m_sFltFilePath;
	CString m_sFltProcessName;
	CStatusBarCtrl m_cStatusBar;

	void InitRenewalDialog();
	void WriteLogFileAndView(CAtlString sWriteData);
	BOOL DefaultFiltering(DWORD dwPID, CAtlString sProcessName);
	BOOL FilterManager(CAtlString sProcessName, CAtlString sTotalFilePath);

	void WriteLog(const TCHAR *format_string, ...);
	void WriteLogD(const TCHAR *format_string, ...);
	void WriteLogA(const TCHAR *format_string, ...);

	void ResizeDisplayControls(int cx=-1, int cy=-1);
	BOOL StartDriver();
	BOOL StopDriver();
	BOOL ConnectToDriver();
	void SendMsgToDriver(CString msg);

	void StartDriverEventReadThread();
	void StopDriverEventReadThread();

	void StartAPIEventReadThread();
	void StopAPIEventReadThread();
	
private:
	CMiniFilterManager m_FilterManager;
	BOOL m_bMiniFilterLoaded;
public:
	afx_msg void OnDestroy();
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnBnClickedButton3();
	afx_msg void OnBnClickedButton4();
	afx_msg LRESULT OnDriveSetChanged(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedButton5();
	afx_msg void OnBnClickedButton6();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedButton7();
	afx_msg void OnBnClickedButton8();
	afx_msg void OnBnClickedButton9();
	afx_msg void OnBnClickedButton10();
	afx_msg void OnLbnDblclkList1();
	afx_msg void OnLbnDblclkList2();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedCheck1();
	afx_msg void OnBnClickedCheck2();
	afx_msg void OnSearchText();
};
