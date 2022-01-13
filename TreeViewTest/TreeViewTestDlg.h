
// TreeViewTestDlg.h : 헤더 파일
//

#pragma once
#include "afxcmn.h"
#include <iostream> //I/O스트림 헤더
#include <io.h> //파일 구조체 헤더
#include <string>//스트링 객체 사용 헤더
#include <list>//리스트 자료형 헤더

// CTreeViewTestDlg 대화 상자
class CTreeViewTestDlg : public CDialogEx
{
// 생성입니다.
public:
	CTreeViewTestDlg(CWnd* pParent = NULL);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
	enum { IDD = IDD_TREEVIEWTEST_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
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
