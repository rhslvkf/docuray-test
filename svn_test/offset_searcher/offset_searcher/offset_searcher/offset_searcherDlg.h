
// offset_searcherDlg.h : ��� ����
//

#pragma once
#include "afxwin.h"


// Coffset_searcherDlg ��ȭ ����
class Coffset_searcherDlg : public CDialogEx
{
// �����Դϴ�.
public:
	Coffset_searcherDlg(CWnd* pParent = NULL);	// ǥ�� �������Դϴ�.

// ��ȭ ���� �������Դϴ�.
	enum { IDD = IDD_OFFSET_SEARCHER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV �����Դϴ�.


// �����Դϴ�.
protected:
	HICON m_hIcon;

	// ������ �޽��� �� �Լ�
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnDropFiles(HDROP hDropInfo);
	CEdit m_cResultEdit;
	CEdit m_cEditOffset;
};
