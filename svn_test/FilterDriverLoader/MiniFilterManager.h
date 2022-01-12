#pragma once
#pragma comment(lib, "FltLib.lib")
#include <atlcoll.h>
#include <WinSvc.h>
#include <fltUser.h>
#include "../DocurayEventFilter/DocurayEventFilter_msgstruct.h"

#define FILTER_DRIVER_NAME_X64	_T("DocurayEventFilterTester_x64.sys")
#define FILTER_DRIVER_NAME		_T("DocurayEventFilterTester.sys")
#define SERVICE_NAME_X64		_T("DocurayEventFilterTester_x64")
#define SERVICE_NAME			_T("DocurayEventFilterTester")
#define FILTER_ALTITUDE			_T("369993")
#define IOCP_PORT_NAME			L"\\BMSDOCTWMT"


#define SECUWORKS_DEFAULT_THREAD_COUNT		1
#define SECUWORKS_DEFAULT_REQUEST_COUNT		5


#define DELETE_MEMORY(p)		if(p) {delete p; p = NULL;}
#define DELETE_ARRAY_MEMORY(p)	if(p) {delete [] p; p = NULL;}

#define FREE_MEMORY(p)			if(p) {free(p); p = NULL;}

#define MINIFILTER_LOAD_SUCCESS							0
#define MINIFILTER_LOAD_ERROR_NO_DRIVER_FILE			1
#define MINIFILTER_LOAD_ERROR_REGISTER_SERVICE_FAIL		2
#define MINIFILTER_LOAD_ERROR_START_SERVICE_FAIL		3
#define MINIFILTER_LOAD_ERROR_ACCESS_DENIED				5

typedef struct
{
	FILTER_MESSAGE_HEADER MessageHeader;
	IOCP_MSG_FROM_DRIVER_LOG la;
	OVERLAPPED Ovlp;
} GRADIUS_LOG_MESSAGE, *PGRADIUS_LOG_MESSAGE;

typedef struct
{
	BOOLEAN SafeToOpen;
} GRADIUS_REPLY, *PGRADIUS_REPLY;

typedef struct
{
	FILTER_REPLY_HEADER ReplyHeader;
	GRADIUS_REPLY Reply;
} GRADIUS_REPLY_MESSAGE, *PGRADIUS_REPLY_MESSAGE;

enum 
{
	EXIT_THREAD = 0,
	ENTER_THREAD
};

class CMiniFilterManager
{
public:
	CMiniFilterManager();
	~CMiniFilterManager(void);


public:
	CRITICAL_SECTION m_hPortCS;
	CRITICAL_SECTION m_LogListCS;
	CAtlString m_sServiceName;
	HANDLE m_hThreads;

public:
	HANDLE m_hPort;
	HANDLE m_hCompletion;
	BOOL m_bIsWow64;
	CAtlString m_sDriverPath;
	DWORD m_dwExitThreadCode;
	BOOL m_bConnected;

protected:
	DWORD m_dwLastError;
	BOOL StartFilterDriver();

public:
	//void EnterCriticalSectionLogList() { EnterCriticalSection(&m_LogListCS); }
	//void LeaveCriticalSectionLogList() { LeaveCriticalSection(&m_LogListCS); }

	DWORD SafeStartFilterDriver(BOOL bConnectPort);
	BOOL StopFilterDriver();
	BOOL IsDriverRunning();

	BOOL RegisterFilterDriverServ(CString sDriverPath);
	BOOL UnRegisterFilterDriverServ();

	DWORD GetFilterDriverStatus();
	
	BOOL CreateServiceAndSetRegistry(CString sDriverPath);
	void SetRegistry();
	
	void CloseService();

	CString GetDriverFilePath();
	BOOL ExistsMiniFilterDriver();
	
	DWORD ConnectCommPort();
	void CleanupConnection();
	HRESULT SendTextToDriver(ULONG uCommand, TCHAR *buffer, ULONG dataLen);
	HRESULT EnableDriver();
	HRESULT DisableDriver();

	HRESULT SendFltMessage(LPVOID lpPolicyData, DWORD dwDataSize);

	HANDLE hRcvHandle;
	void StartReadFilterMsgThread();


	CAtlList<PIOCP_MSG_FROM_DRIVER_LOG> m_LogList;
	void AddTailLogList(PIOCP_MSG_FROM_DRIVER_LOG pLA) { m_LogList.AddTail(pLA); }
	DWORD GetLogListCount();
	void GetLogDataAtHead(PIOCP_MSG_FROM_DRIVER_LOG *pLA);
	void RemoveLogListHead(PIOCP_MSG_FROM_DRIVER_LOG *pLA);
	int GetFileEvents(PIOCP_MSG_FROM_DRIVER_LOG *ppList);

	HANDLE m_hEvent;
	void SetEventHandle(HANDLE h) { m_hEvent = h; }
	void RemoveNetworkHeaderInPath(PWCHAR p);

	//typedef void (*cbFunc)(PIOCP_MSG_FROM_DRIVER_LOG);
	//cbFunc CallbackToParent;
	//void SetCallback(void callback(PIOCP_MSG_FROM_DRIVER_LOG))
	//{
	//	CallbackToParent = callback;
	//}

};

