
// TreeViewTestDlg.cpp : ���� ����
//

#include "stdafx.h"
#include "TreeViewTest.h"
#include "TreeViewTestDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTreeViewTestDlg ��ȭ ����



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


// CTreeViewTestDlg �޽��� ó����

BOOL CTreeViewTestDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// �� ��ȭ ������ �������� �����մϴ�. ���� ���α׷��� �� â�� ��ȭ ���ڰ� �ƴ� ��쿡��
	//  �����ӿ�ũ�� �� �۾��� �ڵ����� �����մϴ�.
	SetIcon(m_hIcon, TRUE);			// ū �������� �����մϴ�.
	SetIcon(m_hIcon, FALSE);		// ���� �������� �����մϴ�.

	//ChangeWindowMessageFilter(0x0049, MSGFLT_ADD);
	//ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);

	DragAcceptFiles(TRUE);

	// TODO: ���⿡ �߰� �ʱ�ȭ �۾��� �߰��մϴ�.
	InitTreeCtrlDirRootView();
	return TRUE;  // ��Ŀ���� ��Ʈ�ѿ� �������� ������ TRUE�� ��ȯ�մϴ�.
}

// ��ȭ ���ڿ� �ּ�ȭ ���߸� �߰��� ��� �������� �׸�����
//  �Ʒ� �ڵ尡 �ʿ��մϴ�. ����/�� ���� ����ϴ� MFC ���� ���α׷��� ��쿡��
//  �����ӿ�ũ���� �� �۾��� �ڵ����� �����մϴ�.

void CTreeViewTestDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // �׸��⸦ ���� ����̽� ���ؽ�Ʈ�Դϴ�.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Ŭ���̾�Ʈ �簢������ �������� ����� ����ϴ�.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// �������� �׸��ϴ�.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// ����ڰ� �ּ�ȭ�� â�� ���� ���ȿ� Ŀ���� ǥ�õǵ��� �ý��ۿ���
//  �� �Լ��� ȣ���մϴ�.
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

		tInsertItemBuff.item.pszText = _T("Computer");        // ��� �ؽ�Ʈ.
		tInsertItemBuff.item.iImage = 0;                    // ��� �⺻ �̹��� ����. 
		tInsertItemBuff.item.iSelectedImage = 3;         // ��� ���ý� �̹��� ����.

		 HTREEITEM hRoot = m_cTreeCtrl.InsertItem(&tInsertItemBuff); //����1 ��Ʈ�� �߰�
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
// // 		struct _finddata_t fd;//���丮 �� ���� �� ���� ���� ���� ��ü
// // 		intptr_t handle;
// // 		CAtlList<_finddata_t> fdlist;
// // 
// // 		if ((handle = _findfirst(dirPath, &fd)) == -1L) //fd ����ü �ʱ�ȭ.
// // 		{
// // 			//�����̳� ���丮�� ���� ���.
// // 			cout << "No file in directory!" << endl;
// // 			return;
// // 		}
// // 
// // 		do //���� Ž�� �ݺ� ����
// // 		{
// // 			checkDirFile = isFileOrDir(fd);//���� ��ü ���� Ȯ��(���� or ���丮)
// // 			if (checkDirFile == 0 && fd.name[0] != '.') {
// // 				//���丮�� ���� ����
// // 				cout << "Dir  : " << path << "\\" << fd.name << endl;
// // 				searchingDir(path + "\\" + fd.name);//����� ȣ��
// // 			}
// // 			else if (checkDirFile == 1 && fd.name[0] != '.') {
// // 				//������ ���� ����
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
	// TODO: ���⿡ Ư��ȭ�� �ڵ带 �߰� ��/�Ǵ� �⺻ Ŭ������ ȣ���մϴ�.

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
	// TODO: ���⿡ �޽��� ó���� �ڵ带 �߰� ��/�Ǵ� �⺻���� ȣ���մϴ�.

	MessageBox(_T("OnDropFiles"));
	int count = DragQueryFile(hDropInfo, -1, NULL, 0);

	CAtlString semp_path = _T("");
	for (int i = 0; i < count; i++) {
		TCHAR szTest[MAX_PATH + 1] = {0,};
		// ��ӵ� �׸��߿� i��° �׸��� ��θ� ��´�.
		DragQueryFile(hDropInfo, i, szTest, MAX_PATH);
		// ���� ��θ� ����Ʈ �ڽ��� �߰��Ѵ�.
		semp_path.Format(_T("%s"), szTest);
		MessageBox(semp_path);
	}

	CDialogEx::OnDropFiles(hDropInfo);
}
