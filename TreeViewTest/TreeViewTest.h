
// TreeViewTest.h : PROJECT_NAME ���� ���α׷��� ���� �� ��� �����Դϴ�.
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH�� ���� �� ������ �����ϱ� ���� 'stdafx.h'�� �����մϴ�."
#endif

#include "resource.h"		// �� ��ȣ�Դϴ�.


// CTreeViewTestApp:
// �� Ŭ������ ������ ���ؼ��� TreeViewTest.cpp�� �����Ͻʽÿ�.
//

class CTreeViewTestApp : public CWinApp
{
public:
	CTreeViewTestApp();

// �������Դϴ�.
public:
	virtual BOOL InitInstance();

// �����Դϴ�.

	DECLARE_MESSAGE_MAP()
};

extern CTreeViewTestApp theApp;