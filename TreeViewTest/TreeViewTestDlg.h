
// TreeViewTestDlg.h : ��� ����
//

#pragma once
#include "afxcmn.h"
#include <iostream> //I/O��Ʈ�� ���
#include <io.h> //���� ����ü ���
#include <string>//��Ʈ�� ��ü ��� ���
#include <list>//����Ʈ �ڷ��� ���

// CTreeViewTestDlg ��ȭ ����
class CTreeViewTestDlg : public CDialogEx
{
// �����Դϴ�.
public:
	CTreeViewTestDlg(CWnd* pParent = NULL);	// ǥ�� �������Դϴ�.

// ��ȭ ���� �������Դϴ�.
	enum { IDD = IDD_TREEVIEWTEST_DIALOG };

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
	CTreeCtrl m_cTreeCtrl;

	void InitTreeCtrlDirView();
	void InitTreeCtrlDirRootView();

// 	void searchingDir(CAtlString path);
// 	int isFileOrDir(_finddata_t fd);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnDropFiles(HDROP hDropInfo);
};
