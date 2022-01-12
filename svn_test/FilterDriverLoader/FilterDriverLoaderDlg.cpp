
// FilterDriverLoaderDlg.cpp : ���� ����
//

#include "stdafx.h"
#include "FilterDriverLoader.h"
#include "FilterDriverLoaderDlg.h"
#include "afxdialogex.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CFilterDriverLoaderDlg ��ȭ ����
int gViewMode;
CFilterDriverLoaderDlg *g_pMain;
CFilterDriverData g_cFilterDriverData;
HANDLE g_hAPIEventThreadC;
HANDLE g_hAPIEventThreadD;
DWORD WINAPI APIEventReadThreadC(LPVOID arg);
DWORD WINAPI APIEventReadThreadD(LPVOID arg);

HANDLE g_hDriverEventThread;
DWORD WINAPI DrvEventReadThread(LPVOID arg);

HANDLE g_hDriverEvent;
#define TIMER_INTERVAL_0	5000
CFilterDriverLoaderDlg::CFilterDriverLoaderDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CFilterDriverLoaderDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CFilterDriverLoaderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_edtLog);
	DDX_Control(pDX, IDC_LIST1, m_lstEvD);
	DDX_Control(pDX, IDC_LIST2, m_lstEvA);
	DDX_Text(pDX, IDC_EDIT5, m_sFltFilePath);
	DDX_Text(pDX, IDC_EDIT3, m_sFltProcessName);
}

BEGIN_MESSAGE_MAP(CFilterDriverLoaderDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_EN_CHANGE(IDC_EDIT5, OnSearchText)
	ON_EN_CHANGE(IDC_EDIT3, OnSearchText)
	//ON_MESSAGE(WM_DEVICECHANGE, OnDriveSetChanged)
	ON_BN_CLICKED(IDC_BUTTON1, &CFilterDriverLoaderDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CFilterDriverLoaderDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON3, &CFilterDriverLoaderDlg::OnBnClickedButton3)
	ON_BN_CLICKED(IDC_BUTTON4, &CFilterDriverLoaderDlg::OnBnClickedButton4)
	ON_BN_CLICKED(IDC_BUTTON5, &CFilterDriverLoaderDlg::OnBnClickedButton5)
	ON_BN_CLICKED(IDC_BUTTON6, &CFilterDriverLoaderDlg::OnBnClickedButton6)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BUTTON7, &CFilterDriverLoaderDlg::OnBnClickedButton7)
	ON_BN_CLICKED(IDC_BUTTON8, &CFilterDriverLoaderDlg::OnBnClickedButton8)
	ON_BN_CLICKED(IDC_BUTTON9, &CFilterDriverLoaderDlg::OnBnClickedButton9)
	ON_BN_CLICKED(IDC_BUTTON10, &CFilterDriverLoaderDlg::OnBnClickedButton10)
	ON_LBN_DBLCLK(IDC_LIST1, &CFilterDriverLoaderDlg::OnLbnDblclkList1)
	ON_LBN_DBLCLK(IDC_LIST2, &CFilterDriverLoaderDlg::OnLbnDblclkList2)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_CHECK1, &CFilterDriverLoaderDlg::OnBnClickedCheck1)
	ON_BN_CLICKED(IDC_CHECK2, &CFilterDriverLoaderDlg::OnBnClickedCheck2)
END_MESSAGE_MAP()


// CFilterDriverLoaderDlg �޽��� ó����

BOOL CFilterDriverLoaderDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	SetIcon(m_hIcon, TRUE); SetIcon(m_hIcon, FALSE);

	g_pMain = this;
	m_bMiniFilterLoaded = FALSE;
	
	// thread handle �ʱ�ȭ
	g_hAPIEventThreadC = NULL;
	g_hAPIEventThreadD = NULL;
	g_hDriverEvent = NULL;
	g_hDriverEventThread = NULL;

	// Check AutoScroll
	CheckDlgButton(IDC_CHECK1, TRUE);
	OnBnClickedCheck1();

	// API thread ����
	//OnBnClickedButton5();
	
	gViewMode = 0;
	
#ifdef RENEWAL
	InitRenewalDialog();
#else
	SetDlgItemText(IDC_EDIT2, _T("Some Text~~~"));

	// Watcher
	SetTimer(0, TIMER_INTERVAL_0, NULL);
	ShowWindow(SW_MAXIMIZE);
#endif
	SetTimer(1, NULL, NULL);
	
	return TRUE;
}

void CFilterDriverLoaderDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
		int cxIcon = GetSystemMetrics(SM_CXICON); int cyIcon = GetSystemMetrics(SM_CYICON);

		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CFilterDriverLoaderDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CFilterDriverLoaderDlg::InitRenewalDialog()
{
	CRect cRect; GetClientRect(&cRect);

	m_lstEvD.SetExtendedStyle(m_lstEvD.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_lstEvD.InsertColumn(1, L"Time", LVCFMT_CENTER, cRect.right * 0.08, -1);
	m_lstEvD.InsertColumn(2, L"Reaction", LVCFMT_LEFT, cRect.right * 0.095, -1);
	m_lstEvD.InsertColumn(3, L"ProcessName", LVCFMT_LEFT, cRect.right * 0.125, -1);
	m_lstEvD.InsertColumn(4, L"PID", LVCFMT_LEFT, cRect.right * 0.045, -1);
	m_lstEvD.InsertColumn(5, L"FilePath", LVCFMT_LEFT, cRect.right * 0.655, -1);
	m_lstEvD.ModifyStyle(LVS_TYPEMASK, LVS_REPORT); 
	
	m_lstEvD.MoveWindow(0, cRect.top + 30, cRect.right, cRect.bottom - 53);

	int nPart[2] = {cRect.right, -1};
	if (IsWindow(m_cStatusBar.m_hWnd) == FALSE)
	{
		m_cStatusBar.Create(WS_CHILD | WS_VISIBLE | CBRS_BOTTOM, CRect(cRect.left, cRect.bottom - 30, cRect.right, cRect.bottom), this, 0);
	}
	m_cStatusBar.SetParts(2, nPart);

	m_bViewBMSEvent = FALSE;
}

void CFilterDriverLoaderDlg::WriteLogFileAndView(CAtlString sWriteData)
{
	TCHAR szControllerPath[MAX_PATH] = {0, };
	GetModuleFileName(NULL, szControllerPath, MAX_PATH);

	CAtlString sLogFilePath = szControllerPath;
	sLogFilePath.Replace(PathFindFileName(szControllerPath), _T("Result.log"));

	HANDLE hFile = INVALID_HANDLE_VALUE;
	hFile = CreateFile(sLogFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return;
	}

	DWORD dwActualSize = 0;
	if (WriteFile(hFile, sWriteData, sWriteData.GetLength() * sizeof(TCHAR), &dwActualSize, NULL) == FALSE)
	{
		return;
	}
	CloseHandle(hFile);

	ShellExecute(NULL, _T("open"), _T("notepad.exe"), sLogFilePath, NULL, SW_SHOWNORMAL);
}

BOOL CFilterDriverLoaderDlg::DefaultFiltering(DWORD dwPID, CAtlString sProcessName)
{
	BOOL bRet = FALSE;
	HWND hModule = NULL;
	
	// DRM Controller 2.1
	hModule = ::FindWindow(_T("#32770"), _T(" DRM Controller 2.1"));
	if (hModule != NULL)
	{
		DWORD dwSourcePID = 0;
		GetWindowThreadProcessId(hModule, &dwSourcePID);

		if (dwPID == dwSourcePID)
		{
			return bRet;
		}
	}

	// Controller OTP Generator
	hModule = ::FindWindow(_T("#32770"), _T("[ Controller OTP Generator ]"));
	if (hModule != NULL)
	{
		DWORD dwSourcePID = 0;
		GetWindowThreadProcessId(hModule, &dwSourcePID);

		if (dwPID == dwSourcePID)
		{
			return bRet;
		}
	}

	if (
		// DRM
		sProcessName.CompareNoCase(_T("DocuRay365.exe")) == 0 || 
		sProcessName.CompareNoCase(_T("DRM_Starter.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DRScreenLock.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayLogin.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DRInstallAgent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayService.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayService_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayWatcher.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayWatcher_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayComm.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayTray.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayHookingFree.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRaySupport.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRaySysMon.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRaySysMon_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayAPI.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayAPI_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayPrintAgent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayPrintAgent_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("DocuRayMAC.exe")) == 0 || //
		// DLP
		sProcessName.CompareNoCase(_T("BMSIASServ.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSIASServ3.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSIASServ_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSIASServ3_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSIASServ2.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSCommAgent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSRec.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSRec_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSReactAgent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSysMon.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSysMon_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSFCIProc.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSFile2Agent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSMMAgent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSMMAgent_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSGreenAgent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSOEAgent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("FileChecker.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSSecUSB.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSIASServ_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSIASServ2_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSIASServ3_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSPrintAgent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSPrintAgent_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSLogin.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSLogin_x64.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSAssetManAgent.exe")) == 0 ||
		sProcessName.CompareNoCase(_T("BMSAssetManAgent_x64.exe")) == 0
		)
	{
		return bRet;
	}

	return TRUE;
}

// #����̹� ����
void CFilterDriverLoaderDlg::OnBnClickedButton1()
{
	BOOL bDriverRun = StartDriver();
	if (bDriverRun)
	{
		GetDlgItem(IDC_BUTTON1)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON2)->EnableWindow(TRUE);

		if (IsWindow(m_cStatusBar.m_hWnd))
		{
			CAtlString sStatus = _T("[ Driver ] : Load,  [ Port ] : Unconnected"); 
			m_cStatusBar.SetText(sStatus, 0, 0);

			RECT cRect;
			m_cStatusBar.GetWindowRect(&cRect);
			m_cStatusBar.ScreenToClient(&cRect); m_cStatusBar.InvalidateRect(&cRect);
		}
	}
	else
	{
		if (IsWindow(m_cStatusBar.m_hWnd))
		{
			CAtlString sStatus = _T("[ Driver ] : Unload,  [ Port ] : Unconnected"); 
			m_cStatusBar.SetText(sStatus, 0, 0);

			RECT cRect;
			m_cStatusBar.GetWindowRect(&cRect);
			m_cStatusBar.ScreenToClient(&cRect); m_cStatusBar.InvalidateRect(&cRect);
		}
	}
}

// #����̹� ����
void CFilterDriverLoaderDlg::OnBnClickedButton2()
{
	OnBnClickedButton9();
	StopDriverEventReadThread();
	StopDriver();

	GetDlgItem(IDC_BUTTON1)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);

	if (IsWindow(m_cStatusBar.m_hWnd))
	{
		CAtlString sStatus = _T("[ Driver ] : Unload,  [ Port ] : Unconnected"); 
		m_cStatusBar.SetText(sStatus, 0, 0);

		RECT cRect;
		m_cStatusBar.GetWindowRect(&cRect);
		m_cStatusBar.ScreenToClient(&cRect); m_cStatusBar.InvalidateRect(&cRect);
	}
}

// #����̹��� ����
void CFilterDriverLoaderDlg::OnBnClickedButton3()
{
	BOOL bConnected = ConnectToDriver();
	if (bConnected)
	{
		StartDriverEventReadThread();

		GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON9)->EnableWindow(TRUE);

		if (IsWindow(m_cStatusBar.m_hWnd))
		{
			CAtlString sStatus = _T("[ Driver ] : Load,  [ Port ] : Connected");
			m_cStatusBar.SetText(sStatus, 0, 0);

			RECT cRect;
			m_cStatusBar.GetWindowRect(&cRect);
			m_cStatusBar.ScreenToClient(&cRect); m_cStatusBar.InvalidateRect(&cRect);
		}
	}
}

// #����̹� ���� ����
void CFilterDriverLoaderDlg::OnBnClickedButton9()
{
	m_FilterManager.CleanupConnection();

	GetDlgItem(IDC_BUTTON3)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON9)->EnableWindow(FALSE);

	CAtlString sStatus = _T("[ Driver ] : Load,  [ Port ] : Unconnected");

	if (IsWindow(m_cStatusBar.m_hWnd))
	{
		m_cStatusBar.SetText(sStatus, 0, 0);

		RECT cRect;
		m_cStatusBar.GetWindowRect(&cRect);
		m_cStatusBar.ScreenToClient(&cRect); m_cStatusBar.InvalidateRect(&cRect);
	}
}

// #������ ��� �α����� ��� �� ǥ��
void CFilterDriverLoaderDlg::OnBnClickedButton10()
{
	UpdateData();
	if (m_sFltFilePath.IsEmpty() == FALSE || m_sFltProcessName.IsEmpty() == FALSE)
	{
		g_cFilterDriverData.SetFilterFilePathData(m_sFltFilePath);
		g_cFilterDriverData.SetFilterProcessData(m_sFltProcessName);
		m_lstEvD.DeleteAllItems();
	}

	CAtlString sTotalLogText = _T("");

	int nIndex = 0;
	POSITION pos = g_cFilterDriverData.m_listListCtrlData.GetHeadPosition();
	while (pos)
	{
		CAtlString sLineData = g_cFilterDriverData.m_listListCtrlData.GetNext(pos);
		if (sLineData.IsEmpty())
		{
			continue;
		}

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

		CAtlString sTotalFilePath = sTargetFilePath + sDestinationPath;
		if (sTotalFilePath.Find(m_sFltFilePath) > -1 && sProcessName.Find(m_sFltProcessName) > -1)
		{
			CAtlString sLogLineText = _T("");
			sReaction.Replace(_T(" "), _T(""));
			sLogLineText.Format(_T("%s %s %s %s %s\r\n"), sTime, sReaction, sProcessName, sPID, sTotalFilePath);

			sTotalLogText += sLogLineText;
		}
	}

	WriteLogFileAndView(sTotalLogText);
}

// ����̹��� data ������
void CFilterDriverLoaderDlg::OnBnClickedButton8()
{
	CString strSend = _T("");
	GetDlgItemText(IDC_EDIT2, strSend);
	SendMsgToDriver(strSend);
}

// ����׿� ȭ�� �����
void CFilterDriverLoaderDlg::OnBnClickedButton4()
{
#ifdef RENEWAL
	m_lstEvD.DeleteAllItems();
	m_lstEvA.DeleteAllItems();
	g_cFilterDriverData.ClearData();
#else
	m_edtLog.SetWindowText(_T(""));
	m_lstEvD.ResetContent();
	m_lstEvA.ResetContent();
#endif
}

void CFilterDriverLoaderDlg::OnDestroy()
{
	StopDriverEventReadThread();
	StopDriver();
	StopAPIEventReadThread();

	CDialog::OnDestroy();
}

// API event ����
void CFilterDriverLoaderDlg::OnBnClickedButton5()
{
	StartAPIEventReadThread();
}

// API event ����
void CFilterDriverLoaderDlg::OnBnClickedButton6()
{
	StopAPIEventReadThread();
}

// â resize event handler
void CFilterDriverLoaderDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	
#ifdef RENEWAL
	CRect rectCtl;
	if (IsWindow(m_lstEvD.m_hWnd))
	{
		m_lstEvD.GetWindowRect(&rectCtl); ScreenToClient(&rectCtl);
		m_lstEvD.MoveWindow(0, rectCtl.top, cx, cy - 53); m_lstEvD.InvalidateRect(&rectCtl);
	}
	
	if (IsWindow(m_cStatusBar.m_hWnd))
	{
		GetClientRect((&rectCtl));
		int nPart[2] = {rectCtl.right, -1};

		m_cStatusBar.SetParts(2, nPart);
		m_cStatusBar.MoveWindow(0, cy - 30, cx, cy); m_cStatusBar.InvalidateRect(&rectCtl);
	}
#else
	if (m_edtLog) m_edtLog.SetWindowPos(NULL, 12, cy-217, cx-25, 207, SWP_SHOWWINDOW);

	ResizeDisplayControls(cx, cy);
	if (GetDlgItem(IDCANCEL)) GetDlgItem(IDCANCEL)->SetWindowPos(NULL, cx - 100, 13, 0, 0, SWP_NOSIZE);
	if (GetDlgItem(IDC_BUTTON4)) GetDlgItem(IDC_BUTTON4)->SetWindowPos(NULL, cx - 192, 13, 0, 0, SWP_NOSIZE);
	if (GetDlgItem(IDC_CHECK1)) GetDlgItem(IDC_CHECK1)->SetWindowPos(NULL, cx - 278, 17, 0, 0, SWP_NOSIZE);
	if (GetDlgItem(IDC_BUTTON7)) GetDlgItem(IDC_BUTTON7)->SetWindowPos(NULL, cx - 284, 13, 0, 0, SWP_NOSIZE);
	if (GetDlgItem(IDC_BUTTON5)) GetDlgItem(IDC_BUTTON5)->SetWindowPos(NULL, (cx - 37) / 2 + 24, 13, 0, 0, SWP_NOSIZE);
	if (GetDlgItem(IDC_BUTTON6)) GetDlgItem(IDC_BUTTON6)->SetWindowPos(NULL, (cx - 37) / 2 + 116, 13, 0, 0, SWP_NOSIZE);
#endif
}
CAtlList<PIOCP_MSG_FROM_DRIVER_LOG> m_LogList;

// ����׿� ȭ�� ������
void CFilterDriverLoaderDlg::OnBnClickedButton7()
{

	PIOCP_MSG_FROM_DRIVER_LOG pN = new IOCP_MSG_FROM_DRIVER_LOG;
	pN->uProcessID = 999;
	m_LogList.AddHead(pN);

	//delete pN;
	//pN = NULL;
	m_LogList.RemoveAll();

	try
	{
		PIOCP_MSG_FROM_DRIVER_LOG p = m_LogList.RemoveHead();
		WRITELOG(_T("p=%x"), p);
		WRITELOG(_T("p->uProcessID=%d"), p->uProcessID);
	}
	catch (...)
	{
		WRITELOG(_T("Exception!!!"));
	}


	WRITEDLOG(_T("===========================================================================%x"), NTDDI_VERSION);
	WRITEALOG(_T("==========================================================================="));
}

void CFilterDriverLoaderDlg::ResizeDisplayControls(int cx, int cy)
{
	if (cx == -1 && cy == -1)
	{
		RECT rect;
		GetClientRect(&rect);
		cx = rect.right - rect.left;
		cy = rect.bottom - rect.top;
	}

	int width_left = (cx - 37) * 0.5;
	if (gViewMode == -1)
	{
		width_left = (cx - 37) * 0.9;
	}
	else if (gViewMode == 1)
	{
		width_left = (cx - 37) * 0.1;
	}
	int width_right = (cx - 37) - width_left;

	width_left = cx - 24;
	width_right = 0;
	if (m_lstEvD) m_lstEvD.SetWindowPos(NULL, 12, 50, width_left, cy - 276, SWP_SHOWWINDOW);
	//if (m_lstEvA) m_lstEvA.SetWindowPos(NULL, width_left + 24, 50, width_right, cy - 476, SWP_SHOWWINDOW);
}


void CFilterDriverLoaderDlg::OnLbnDblclkList1()
{
	if (gViewMode != -1) gViewMode = -1;
	else gViewMode = 0;
	ResizeDisplayControls();
}


void CFilterDriverLoaderDlg::OnLbnDblclkList2()
{
	if (gViewMode != 1) gViewMode = 1;
	else gViewMode = 0;
	ResizeDisplayControls();
}


void CFilterDriverLoaderDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 0)
	{
		KillTimer(0);
		BOOL bRunning = FALSE;// m_FilterManager.IsDriverRunning();
		WRITELOG(_T("[OnTimer] bRunning=%d"), bRunning);
		if (bRunning == FALSE)
		{
			//StartDriver();
			GetDlgItem(IDC_BUTTON1)->EnableWindow(TRUE);
			GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);
		}
		SetTimer(0, TIMER_INTERVAL_0, NULL);
	}
	else if (nIDEvent == 1)
	{
		KillTimer(1);
		OnBnClickedButton1();
		OnBnClickedButton3();
	}
	
	CDialogEx::OnTimer(nIDEvent);
}


void CFilterDriverLoaderDlg::OnBnClickedCheck1()
{
	m_bAutoScrollD = ((CButton*)GetDlgItem(IDC_CHECK1))->GetCheck();
}

void CFilterDriverLoaderDlg::OnBnClickedCheck2()
{
	m_bViewBMSEvent = ((CButton*)GetDlgItem(IDC_CHECK2))->GetCheck();
}

void CFilterDriverLoaderDlg::OnSearchText()
{
	UpdateData();
	g_cFilterDriverData.SetFilterFilePathData(m_sFltFilePath);
	g_cFilterDriverData.SetFilterProcessData(m_sFltProcessName);
	m_lstEvD.DeleteAllItems();

	int nIndex = 0;
	POSITION pos = g_cFilterDriverData.m_listListCtrlData.GetHeadPosition();
	while (pos)
	{
		CAtlString sLineData = g_cFilterDriverData.m_listListCtrlData.GetNext(pos);
		if (sLineData.IsEmpty())
		{
			continue;
		}

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

		CAtlString sTotalFilePath = sTargetFilePath + sDestinationPath;
		if (sTotalFilePath.Find(m_sFltFilePath) > -1 && sProcessName.Find(m_sFltProcessName) > -1)
		{
			m_lstEvD.InsertItem(LVIF_TEXT, nIndex, sTime, 0, 0, 0, 0);
			m_lstEvD.SetItem(nIndex, 1, LVIF_TEXT, sReaction, 0, 0, 0, 0);
			m_lstEvD.SetItem(nIndex, 2, LVIF_TEXT, sProcessName, 0, 0, 0, 0);
			m_lstEvD.SetItem(nIndex, 3, LVIF_TEXT, sPID, 0, 0, 0, 0);
			m_lstEvD.SetItem(nIndex, 4, LVIF_TEXT, sTotalFilePath, 0, 0, 0, 0);
			nIndex++;
		}
	}
}