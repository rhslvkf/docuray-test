
// FilterDriverLoader.h : PROJECT_NAME ���� ���α׷��� ���� �� ��� �����Դϴ�.
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH�� ���� �� ������ �����ϱ� ���� 'stdafx.h'�� �����մϴ�."
#endif

#include "resource.h"		// �� ��ȣ�Դϴ�.


// CFilterDriverLoaderApp:
// �� Ŭ������ ������ ���ؼ��� FilterDriverLoader.cpp�� �����Ͻʽÿ�.
//

class CFilterDriverLoaderApp : public CWinApp
{
public:
	CFilterDriverLoaderApp();

// �������Դϴ�.
public:
	virtual BOOL InitInstance();

// �����Դϴ�.

	DECLARE_MESSAGE_MAP()
};

extern CFilterDriverLoaderApp theApp;