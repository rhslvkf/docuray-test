
// offset_searcher.h : PROJECT_NAME ���� ���α׷��� ���� �� ��� �����Դϴ�.
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH�� ���� �� ������ �����ϱ� ���� 'stdafx.h'�� �����մϴ�."
#endif

#include "resource.h"		// �� ��ȣ�Դϴ�.


// Coffset_searcherApp:
// �� Ŭ������ ������ ���ؼ��� offset_searcher.cpp�� �����Ͻʽÿ�.
//

class Coffset_searcherApp : public CWinApp
{
public:
	Coffset_searcherApp();

// �������Դϴ�.
public:
	virtual BOOL InitInstance();

// �����Դϴ�.

	DECLARE_MESSAGE_MAP()
};

extern Coffset_searcherApp theApp;