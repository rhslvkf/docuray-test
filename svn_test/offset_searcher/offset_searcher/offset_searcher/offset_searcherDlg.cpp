
// offset_searcherDlg.cpp : 구현 파일
//

#include "stdafx.h"
#include "offset_searcher.h"
#include "offset_searcherDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnDropFiles(HDROP hDropInfo);
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
	ON_WM_DROPFILES()
END_MESSAGE_MAP()


// Coffset_searcherDlg 대화 상자



Coffset_searcherDlg::Coffset_searcherDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(Coffset_searcherDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void Coffset_searcherDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT2, m_cResultEdit);
	DDX_Control(pDX, IDC_EDIT1, m_cEditOffset);
}

BEGIN_MESSAGE_MAP(Coffset_searcherDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DROPFILES()
END_MESSAGE_MAP()


// Coffset_searcherDlg 메시지 처리기

BOOL Coffset_searcherDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void Coffset_searcherDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 응용 프로그램의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void Coffset_searcherDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR Coffset_searcherDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

#define FUNCNAME2STRING(Func) _T(#Func)
#define FILESPY_COLUMN_NO_INDEX	0
#define FILESPY_COLUMN_REQUEST_INDEX	8
#define FILESPY_COLUMN_FLAG_INDEX	9
#define FILESPY_COLUMN_MOREINFO_INDEX	18

#define REQUEST_READ _T("IRP_MJ_READ")
#define REQUEST_WRITE _T("IRP_MJ_WRITE")

#define MOREINFO_OFFSET_START _T("Offset: ")
#define MOREINFO_TOREAD_START _T("ToRead: ")
#define MOREINFO_TOWRITE_START _T("ToWrite: ")
#define MOREINFO_SEPARATOR _T(" ")

CString PrintErrorCode(CString sFuncName, DWORD ErrorCode)
{
	CString sRet = _T("");
	sRet.Format(_T("%s error(0x%x)"), sFuncName, ErrorCode);
	return  sRet;
}

void CAboutDlg::OnDropFiles(HDROP hDropInfo)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
	CDialogEx::OnDropFiles(hDropInfo);
}

// Get space separated floating point number as CString object.
// Empty string if not a floating point value.
// Returns pointer to next space, trailing NULL char, or NULL
LPCTSTR GetNumberStr(CString& str, LPCTSTR s)
{
	while (*s <= _T(' '))
		++s;
	LPTSTR lpszEnd = NULL;
	_tcstod(s, &lpszEnd);
	// Floating point or int value if conversion stopped at space or end of string
	if (*lpszEnd <= _T(' '))
		str.SetString(s, static_cast<int>(lpszEnd - s));
	else
	{
		str = _T("");
		// This may return NULL!
		lpszEnd = (char*)_tcschr(s, _T(' '));
	}
	return lpszEnd;
}

void Parse(CString& strOut, const CString& strIn)
{
	strOut = _T("");
	LPCTSTR s = strIn.GetString();
	while (s && *s)
	{
		CString str;
		s = GetNumberStr(str, s);
		if (!str.IsEmpty())
		{
			if (!strOut.IsEmpty())
				strOut += _T(' ');
			strOut += str;
		}
	}
}

void Coffset_searcherDlg::OnDropFiles(HDROP hDropInfo)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
	int NumDropFiles = 0;
	TCHAR FilePath[MAX_PATH] = { 0, };
	NumDropFiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, FilePath, MAX_PATH);
	if (NumDropFiles > 1)
	{
		AfxMessageBox(_T("하나만"));
		return;
	}

	if (DragQueryFile(hDropInfo, 0, FilePath, MAX_PATH) == 0)
	{
		AfxMessageBox(PrintErrorCode(FUNCNAME2STRING(DragQueryFile), GetLastError()));
		return;
	}

	HANDLE hFile = 0;
	hFile = CreateFile(FilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		AfxMessageBox(PrintErrorCode(FUNCNAME2STRING(CreateFile), GetLastError()));
		return;
	}

	LARGE_INTEGER FileSize;
	ZeroMemory(&FileSize, sizeof(LARGE_INTEGER));
	if (GetFileSizeEx(hFile, &FileSize) == FALSE)
	{
		AfxMessageBox(PrintErrorCode(FUNCNAME2STRING(GetFileSizeEx), GetLastError()));
		return;
	}

	PBYTE pbRead = NULL;
	pbRead = new BYTE[FileSize.QuadPart + 1];
	if (pbRead == NULL)
	{
		AfxMessageBox(PrintErrorCode(FUNCNAME2STRING(new), GetLastError()));
		return;
	}
	ZeroMemory(pbRead, sizeof(BYTE) * (FileSize.QuadPart + 1));

	DWORD lpNumberOfBytesRead = 0;
	if (ReadFile(hFile, pbRead, sizeof(BYTE) * FileSize.QuadPart, &lpNumberOfBytesRead, NULL) == FALSE)
	{
		AfxMessageBox(PrintErrorCode(FUNCNAME2STRING(new), GetLastError()));
		return;
	}

	CloseHandle(hFile);

	int i = 0;
	CString sSubLine = _T("");
	CMap<CString, LPCSTR, CStringList*, CStringList*> cResultMap;

	CString sData = _T("");
	sData = (LPCTSTR)pbRead;	
	while (AfxExtractSubString(sSubLine, sData, i, _T('\n')))
	{
		i++;

		CString sSubRequest = _T("");
		if (AfxExtractSubString(sSubRequest, sSubLine, FILESPY_COLUMN_REQUEST_INDEX, _T(',')) == FALSE)
		{
			//AfxMessageBox(PrintErrorCode(FUNCNAME2STRING(AfxExtractSubString2), GetLastError()));
			continue;
		}

		if (sSubRequest.Compare(REQUEST_READ) != 0 && sSubRequest.Compare(REQUEST_WRITE) != 0)
		{
			continue;
		}

		CString sSubMoreInfo = _T("");
		if (AfxExtractSubString(sSubMoreInfo, sSubLine, FILESPY_COLUMN_MOREINFO_INDEX, _T(',')) == FALSE)
		{
			//AfxMessageBox(PrintErrorCode(FUNCNAME2STRING(AfxExtractSubString3), GetLastError()));
			continue;
		}
		


		DWORD dwOffsetStart = 0, dwOffsetEnd = 0;
		dwOffsetStart = sSubMoreInfo.Find(MOREINFO_OFFSET_START, 0) + _tcslen(MOREINFO_OFFSET_START);
		dwOffsetEnd = sSubMoreInfo.Find(MOREINFO_SEPARATOR, dwOffsetStart);
		CString sOffset = sSubMoreInfo.Mid(dwOffsetStart, dwOffsetEnd - dwOffsetStart);
		CString sHigh = sOffset.Left(sOffset.GetLength() - sOffset.Find(_T("-")) - 1);
		CString sLow = sOffset.Right(sOffset.GetLength() - sOffset.Find(_T("-")) - 1);
		
		CString sToRead = _T("");
		if (sSubRequest.Compare(REQUEST_READ) == 0)
		{
			DWORD dwToReadStart = 0, dwToReadEnd = 0;
			dwToReadStart = sSubMoreInfo.Find(MOREINFO_TOREAD_START, 0) + _tcslen(MOREINFO_TOREAD_START);
			dwToReadEnd = sSubMoreInfo.Find(MOREINFO_SEPARATOR, dwToReadStart);
			sToRead = sSubMoreInfo.Mid(dwToReadStart, dwToReadEnd - dwToReadStart);
		}
		else
		{
			DWORD dwToReadStart = 0, dwToReadEnd = 0;
			dwToReadStart = sSubMoreInfo.Find(MOREINFO_TOWRITE_START, 0) + _tcslen(MOREINFO_TOWRITE_START);
			dwToReadEnd = sSubMoreInfo.Find(MOREINFO_SEPARATOR, dwToReadStart);
			sToRead = sSubMoreInfo.Mid(dwToReadStart, dwToReadEnd - dwToReadStart);
		}

		CString sInputOffset = _T("");
		m_cEditOffset.GetWindowText(sInputOffset);
		if (sInputOffset.IsEmpty())
		{
			AfxMessageBox(PrintErrorCode(FUNCNAME2STRING(GetWindowText), GetLastError()));
			return;
		}

		if (strtoul(sLow, NULL, 16) == 0x133000)
		{
			GetLastError();
		}

		DWORD dwInputOffset = strtoul(sInputOffset, NULL, 16);
		if (strtoul(sLow, NULL, 16) <= dwInputOffset && strtoul(sLow, NULL, 16) + strtoul(sToRead, NULL, 16) >= dwInputOffset)
		{
			CString sSubNo = _T("");
			AfxExtractSubString(sSubNo, sSubLine, FILESPY_COLUMN_NO_INDEX, _T(','));

			CString sSubFlag = _T("");
			AfxExtractSubString(sSubFlag, sSubLine, FILESPY_COLUMN_FLAG_INDEX, _T(','));

			if (sSubFlag.IsEmpty())
			{
				continue;;
			}

			CString sResultRow = _T("");
			sResultRow += sSubNo + _T(" ");
			sResultRow += sSubRequest + _T(" ");
			sResultRow += sSubFlag + _T(" ");
			sResultRow += sSubMoreInfo + _T("\n");

		
			CStringList *cMoreInfoList = NULL;
			if (cResultMap.Lookup(sSubRequest, cMoreInfoList))
			{
				cMoreInfoList->AddTail(sResultRow);
				cResultMap.SetAt(sSubRequest, cMoreInfoList);
			}
			else
			{
				cMoreInfoList = new CStringList;
				cMoreInfoList->AddTail(sResultRow);
				cResultMap.SetAt(sSubRequest, cMoreInfoList);
			}
		}
	}

	CString sResult = _T("");
	CStringList *cMoreInfoList = NULL;
	if (cResultMap.Lookup(REQUEST_READ, cMoreInfoList))
	{
		POSITION pos = cMoreInfoList->GetHeadPosition();
		while (pos != NULL)
		{
			sResult += cMoreInfoList->GetAt(pos);
			cMoreInfoList->GetNext(pos);
		}
	}

	if (cResultMap.Lookup(REQUEST_WRITE, cMoreInfoList))
	{
		POSITION pos = cMoreInfoList->GetHeadPosition();
		while (pos != NULL)
		{
			sResult += cMoreInfoList->GetAt(pos);
			cMoreInfoList->GetNext(pos);
		}
	}

	m_cResultEdit.SetWindowText(sResult);

	CDialogEx::OnDropFiles(hDropInfo);
}
