
// TreeViewTestDlg.cpp : 구현 파일
//

#include "stdafx.h"
#include "TreeViewTest.h"
#include "TreeViewTestDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTreeViewTestDlg 대화 상자



CTreeViewTestDlg::CTreeViewTestDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CTreeViewTestDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTreeViewTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE1, m_cTreeCtrl);
}

BEGIN_MESSAGE_MAP(CTreeViewTestDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DROPFILES()
END_MESSAGE_MAP()


// CTreeViewTestDlg 메시지 처리기

BOOL CTreeViewTestDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 이 대화 상자의 아이콘을 설정합니다. 응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	//ChangeWindowMessageFilter(0x0049, MSGFLT_ADD);
	//ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);

	DragAcceptFiles(TRUE);

	// TODO: 여기에 추가 초기화 작업을 추가합니다.
	InitTreeCtrlDirRootView();
	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다. 문서/뷰 모델을 사용하는 MFC 응용 프로그램의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CTreeViewTestDlg::OnPaint()
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
HCURSOR CTreeViewTestDlg::OnQueryDragIcon()
{
	MessageBox(_T("Test Drop OnQueryDragIcon"));

	return static_cast<HCURSOR>(m_hIcon);
}

void CTreeViewTestDlg::InitTreeCtrlDirView()
{
	try
	{


		 TVINSERTSTRUCT tInsertItemBuff;
		//m_cTreeCtrl.
	}
	
	catch (...)
	{
		CAtlString sErrOutStr = _T("");
		sErrOutStr.Format(_T("[CTreeViewTestDlg::InitTreeCtrlDirView] - Err (%d)"), GetLastError());
		MessageBox(sErrOutStr);
	}
}

void CTreeViewTestDlg::InitTreeCtrlDirRootView()
{
	try
	{
		TVINSERTSTRUCT tInsertItemBuff;

		tInsertItemBuff.hParent = NULL; 
		tInsertItemBuff.hInsertAfter = TVI_LAST;
		tInsertItemBuff.item.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT; 
		tInsertItemBuff.item.state = 0;
		tInsertItemBuff.item.stateMask = 0;
		tInsertItemBuff.item.cchTextMax = 60; 

		tInsertItemBuff.item.cChildren = 0; 

		tInsertItemBuff.item.pszText = _T("Computer");        // 노드 텍스트.
		tInsertItemBuff.item.iImage = 0;                    // 노드 기본 이미지 지정. 
		tInsertItemBuff.item.iSelectedImage = 3;         // 노드 선택시 이미지 지정.

		 HTREEITEM hRoot = m_cTreeCtrl.InsertItem(&tInsertItemBuff); //레벨1 루트에 추가
		//m_cTreeCtrl.
	}

	catch (...)
	{
		CAtlString sErrOutStr = _T("");
		sErrOutStr.Format(_T("[CTreeViewTestDlg::InitTreeCtrlDirRootView] - Err (%d)"), GetLastError());
		MessageBox(sErrOutStr);
	}

}
// 
// void CTreeViewTestDlg::searchingDir(CAtlString path, CAtlList<CAtlString> &slistOut)
// {
// 	try
// 	{
// // 		int checkDirFile = 0;
// // 		CAtlStringA dirPath = path + "\\*.*";
// // 		struct _finddata_t fd;//디렉토리 내 파일 및 폴더 정보 저장 객체
// // 		intptr_t handle;
// // 		CAtlList<_finddata_t> fdlist;
// // 
// // 		if ((handle = _findfirst(dirPath, &fd)) == -1L) //fd 구조체 초기화.
// // 		{
// // 			//파일이나 디렉토리가 없을 경우.
// // 			cout << "No file in directory!" << endl;
// // 			return;
// // 		}
// // 
// // 		do //폴더 탐색 반복 시작
// // 		{
// // 			checkDirFile = isFileOrDir(fd);//현재 객체 종류 확인(파일 or 디렉토리)
// // 			if (checkDirFile == 0 && fd.name[0] != '.') {
// // 				//디렉토리일 때의 로직
// // 				cout << "Dir  : " << path << "\\" << fd.name << endl;
// // 				searchingDir(path + "\\" + fd.name);//재귀적 호출
// // 			}
// // 			else if (checkDirFile == 1 && fd.name[0] != '.') {
// // 				//파일일 때의 로직
// // 				cout << "File : " <<path<<"\\"<< fd.name << endl;
// // 				fdlist.push_back(fd);
// // 			}
// // 
// // 		} while (_findnext(handle, &fd) == 0);
// // 		_findclose(handle);
// 	}
// 
// 	catch (...)
// 	{
// 		CAtlString sErrOutStr = _T("");
// 		sErrOutStr.Format(_T("[CTreeViewTestDlg::searchingDir] - Err (%d)"), GetLastError());
// 		MessageBox(sErrOutStr);
// 	}
// }
// int CTreeViewTestDlg::isFileOrDir(_finddata_t fd)
// {
// 	try
// 	{
// 
// 
// 		TVINSERTSTRUCT tInsertItemBuff;
// 		//m_cTreeCtrl.
// 	}
// 
// 	catch (...)
// 	{
// 		CAtlString sErrOutStr = _T("");
// 		sErrOutStr.Format(_T("[CTreeViewTestDlg::isFileOrDir] - Err (%d)"), GetLastError());
// 		MessageBox(sErrOutStr);
// 	}
// }



BOOL CTreeViewTestDlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO: 여기에 특수화된 코드를 추가 및/또는 기본 클래스를 호출합니다.

// 	if (pMsg->message != 275 && pMsg->message != 512 && pMsg->message != 32767 && pMsg->message != 799
// 		&& pMsg->message != 15 && pMsg->message != 96 && pMsg->message != 8448 && pMsg->message != 160 
// 		&& pMsg->message != 160 && pMsg->message != 674 && pMsg->message != 257 && pMsg->message != WM_NCMOUSELEAVE
// 		&& pMsg->message != 256 && pMsg->message != 647)
// 	{
// 
// 		CAtlString sOutStr = _T("");
// 		sOutStr.Format(_T("Test Drop Item (%d)"), pMsg->message);
// 		MessageBox(sOutStr);
// 	}

// 	if (pMsg->message == WM_DROPFILES)
// 	{
// 		CAtlString sOutStr = _T("");
// 		sOutStr.Format(_T("Test Drop Item !!!!!! (%d)"), pMsg->message);
// 		MessageBox(sOutStr);
// 	}

	return CDialogEx::PreTranslateMessage(pMsg);
}


void CTreeViewTestDlg::OnDropFiles(HDROP hDropInfo)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.

	MessageBox(_T("OnDropFiles"));
	int count = DragQueryFile(hDropInfo, -1, NULL, 0);

	CAtlString semp_path = _T("");
	for (int i = 0; i < count; i++) {
		TCHAR szTest[MAX_PATH + 1] = {0,};
		// 드롭된 항목중에 i번째 항목의 경로를 얻는다.
		DragQueryFile(hDropInfo, i, szTest, MAX_PATH);
		// 얻은 경로를 리스트 박스에 추가한다.
		semp_path.Format(_T("%s"), szTest);
		MessageBox(semp_path);
	}

	CDialogEx::OnDropFiles(hDropInfo);
}
