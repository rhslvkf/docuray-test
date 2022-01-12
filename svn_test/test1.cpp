﻿#include "stdafx.h"
#include "ClientManager.h"
#include "BMSCommAgentDlg.h"
// #include "CydenzaUtils040316.h"
#include <Iptypes.h>
#include <Iphlpapi.h>
#include <Winbase.h>
#include <lm.h>
#include <atlbase.h>
#include <Lmshare.h>
#include <Lm.h>
#include <atlconv.h>
#include <psapi.h>
#include "GradiusUtils.h"
#include "encryptfile.h"
#include "GRegulationManager.h"
#include "GradiusDefines.h"
#include "Sn3.h"
#include "Utils.h"
#include "strutils.h"
#include "BMSConfiguration.h"
#include "CalculateDate.h"
#include "RebootPopupDlg.h"
#include <Sensapi.h>
CString g_sBMSRemoveParam[7] = {_T("RemoveFromGAM"), _T("RemoveFromLook"), _T("RemoveFromRemapping"), 
_T("RemoveFromMultiuserRegCancel"), _T("RemoveFromMultiuserRegFail"), _T("RemoveFromNoExistUser"),
_T("RemoveFromUserRegCancel")
};

#include <wininet.h>
#undef BOOLAPI
#undef SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
#undef SECURITY_FLAG_IGNORE_CERT_CN_INVALID
#define URL_COMPONENTS URL_COMPONENTS_ANOTHER
#define URL_COMPONENTSA URL_COMPONENTSA_ANOTHER
#define URL_COMPONENTSW URL_COMPONENTSW_ANOTHER
#define LPURL_COMPONENTS LPURL_COMPONENTS_ANOTHER
#define LPURL_COMPONENTSA LPURL_COMPONENTS_ANOTHER
#define LPURL_COMPONENTSW LPURL_COMPONENTS_ANOTHER
#define INTERNET_SCHEME INTERNET_SCHEME_ANOTHER
#define LPINTERNET_SCHEME LPINTERNET_SCHEME_ANOTHER
#define HTTP_VERSION_INFO HTTP_VERSION_INFO_ANOTHER
#define LPHTTP_VERSION_INFO LPHTTP_VERSION_INFO_ANOTHER
#include <winhttp.h>
#undef URL_COMPONENTS
#undef URL_COMPONENTSA
#undef URL_COMPONENTSW
#undef LPURL_COMPONENTS
#undef LPURL_COMPONENTSA
#undef LPURL_COMPONENTSW
#undef INTERNET_SCHEME
#undef LPINTERNET_SCHEME
#undef HTTP_VERSION_INFO
#undef LPHTTP_VERSION_INFO

#include "HtmlViewDlg.h"

#include "AESEncrypt.h"
#include "AtlBMSEncrypt.h"

#include "DiscoveryDefines.h"

#include <Msi.h>
#include <atlcoll.h>

#include "GradiusUtils.h"

#define MAX_INSTALLATION_CNT 1024
#define MAX_PRODUCT_NAME_LEN 256
#define MAX_PRODUCT_CODE_LEN 39
#define BMS_ERROR_BAD_PTR	-1
#define BMS_COMMINI_DECRYPT_FILE_NAME			_T("DecCommini")

int LoadFileFromRes(CString strPath, CString strFile);


// by realhan 20080805
#include "BMSSupport.h"
#include "BMSSendPolicy.h"

#define SLASH		_T('\\')

// to extern
CClientManager *g_pClientManager;
CClientManager *g_pClientManager_LookServer;
CClientManager *g_pClientManager_MonitorLog;

// from CBMSCommAgentDlg
extern CBMSCommAgentDlg *g_pBMSCommAgentDlg;

// CManageLog 글로벌 인스턴스를 생성한다.
CManageLog	g_Log;
CFileIndex *g_pFile;
TCHAR g_Separator = 0x11;

// 아모레퍼시픽 정책 메모리 릭으로 인해 정책을 담을 변수 생성
CString g_sEncryptPolicy;
tstring UriEncode(const std::string & sSrc);

#define TIME_ZONE_REG_PATH _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones")
#define TIME_ZONE_REG_VALUE_NAME	_T("Std")

CString BLOCK_LOG_OLD(CString filename)
{
	return filename + _T("B");
}


// by realhan
// 경로를 주면 경로안에 것들을 다삭제한다.
BOOL RemoveAllDir(TCHAR *strDirPathValue)
{
	//WIN32_FIND_DATA wfd;
	CFileFind finder;

	CString strDirPath;
	strDirPath = strDirPathValue;
	int strLength;
	if(strLength = strDirPath.GetLength(),strLength < 4)
	{
		return FALSE;
	}
	strDirPath = strDirPathValue;
	if(strDirPath[strLength] != _T('\\'))
	{
		strDirPath += _T("\\*.*");
	}
	else
	{
		strDirPath += _T("*.*");
	}

	BOOL bExistDir = finder.FindFile(strDirPath);
	while (bExistDir)
	{
		bExistDir = finder.FindNextFile();

		if (finder.IsDots())
		{
			continue;
		}
		else if (finder.IsDirectory())
		{
			CString tempStr = finder.GetFilePath();
			if(!RemoveAllDir(tempStr.GetBuffer()))
			{
				//g_logger.Write("CBMSInstallAgentWnd::RemoveAllDir 삭제 못함 %s",finder.GetFilePath());
				//return FALSE;
			}
		}
		else
		{
			if((finder.IsReadOnly()))
			{
				if(!SetFileAttributes(finder.GetFilePath(),FILE_ATTRIBUTE_NORMAL))
				{
					WRITELOG(_T("ClientManager::RemoveAllDir 속성 변경 못함 %s"),finder.GetFilePath());
					//return FALSE;
				}
			}
			if(!DeleteFile(finder.GetFilePath()))
			{
				WRITELOG(_T("ClientManager::RemoveAllDir 삭제 못함 %s"),finder.GetFilePath());
				//return FALSE;
			}
		}	
	}
	finder.Close();
	if(!RemoveDirectory(strDirPathValue))
	{
		//return FALSE;
	}
	return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////
//  CClientManager

CClientManager::CClientManager()
{
	m_bConnected = FALSE;
	m_bPolicyReceived = FALSE;
	m_bLookStart = FALSE;

	m_data_id		= 0;

	m_bNeedAgentID = FALSE;
	m_bTerminate = FALSE;
	m_dwRetryCount = 0;
	// 2010/01/26, avilon
	m_bUpgrade = TRUE;
	m_nRetryConnectFromLookTimer = 0;

	// 사본 저장 크기 제한
	m_SaveCopySizeLimit		= BM_DEFAULT_COPYSAVE_SIZE_LIMIT;

	// by grman, 2008/07/04
	m_bLocked = m_bPaused = FALSE;


	m_nIndex = 0;
	g_sEncryptPolicy = _T("");

	m_sInstallmode = _T("");
	m_sSiteName = _T("");

	m_stPageInfo.m_GSFILE = DEFAULT_GSFILE;
	m_stPageInfo.m_GSHS = DEFAULT_GSHS;
	m_stPageInfo.m_GSLOOK = DEFAULT_GSLOOK;
	m_stPageInfo.m_GSREG = DEFAULT_GSREG;
	m_stPageInfo.m_GSALLOW = DEFAULT_GSALLOW;
	m_stPageInfo.m_GSAPP = DEFAULT_GSAPP;
	m_stPageInfo.m_GSBLOCK = DEFAULT_GSBLOCK;
	// 2011/02/14, avilon
	m_stPageInfo.m_GSINTEGRITY = DEFAULT_GSINTEGRITY;
	// 2011/09/20, avilon
	m_stPageInfo.m_GDSFILE = DEFAULT_GDSFILE;
	m_stPageInfo.m_GDSALLOW = DEFAULT_GDSALLOW;
	m_stPageInfo.m_GDSENCKEY = DEFAULT_GDSENCKEY;
	m_stPageInfo.m_GDSFILEREQ = DEFAULT_GDSFILEREQ;
	m_stPageInfo.m_GSMULTI = DEFAULT_GSMULTI;
	m_stPageInfo.m_GSUSER = DEFAULT_GSUSER;
	m_stPageInfo.m_GSHW = DEFAULT_GSHW;
	m_stPageInfo.m_GSALLOWUPLOAD = DEFAULT_GDSALLOWUPLOAD;

	m_sSaveCopyKeyDef = (CString)BM_SAVECOPY_KEY_SHARED + _T(",") + BM_SAVECOPY_KEY_DEVICE + _T(",") + BM_SAVECOPY_KEY_SMTPFILE + _T(",") + BM_SAVECOPY_KEY_WEBFILE + _T(",") + BM_SAVECOPY_KEY_FTP + _T(",") + BM_SAVECOPY_KEY_MSGFILE + _T(",") + BM_SAVECOPY_KEY_CDRW + _T(",") + BM_SAVECOPY_KEY_ACCESS + _T(",") + BM_DISCOVERY_SAVECOPY_KEY_ISOLATION + _T(",") + BM_DISCOVERY_SAVECOPY_KEY_DELETE + _T(",") + BM_SAVECOPY_KEY_SCREENSHOT + _T(",") + BM_SAVECOPY_KEY_MTP + _T(",");

	//2015.03.16 keede122
	m_stPageInfo.m_GSHS61 = DEFAULT_GSHSASSET;
	m_stPageInfo.m_GSASSET = DEFAULT_GSASSET61;
	m_stPageInfo.m_GSASSET64 = DEFAULT_GSASSET64;

	m_stPageInfo.m_GSDEV = DEFAULT_GSDEV61;
	m_stPageInfo.m_GSDEV64 = DEFAULT_GSDEV64;
	// PC보안감사 by yoon.2019.01.08.
	m_stPageInfo.m_GSPCSECU = DEFAULT_GSPCSECU;
	m_stPageInfo.m_sJsonFile = DEFAULT_GSJSONFILE;
	m_stPageInfo.m_sJsonBlockFile = DEFAULT_GSJSONBLOCKFILE;
	m_stPageInfo.m_GSHWDEVICE = DEFAULT_GSHWDEVICE;
	m_stPageInfo.m_sEXDS = DEFAULT_EXDS;
	m_stPageInfo.m_sReciveMail = DEFAULT_RECIVEMAIL;

	m_bSendIPL = FALSE;

	m_bTraffic = FALSE;

	// 2011/03/14, avilon
	m_bIsLogEncoded = FALSE;
	m_bTLS1_2 = TRUE;
	// 2011/05/24, avilon
	m_bDupPopUp = FALSE;

	// 2011/06/02, avilon
	m_bIsCurrentUsingInstantLevel = FALSE;

	m_bUseMultiLan = FALSE;

	m_LangID = GetSystemDefaultLangID();
	m_bConnectUTF8 = TRUE;

	// 2011/10/25, avilon
	m_bUseDiscovery = FALSE;

	//2015.03.16, keede122
	m_bUseAsset = FALSE;

	m_dwUserBaseLogoutTime = 0;

	m_bGSALLOW_62 = FALSE;

	m_bHardwareSpec = FALSE;
	m_bFirstHardwareSpec = FALSE;

	m_httpPort = 80;
	m_bIsHttps = FALSE;

	m_bADUnLock = FALSE;
	m_nOfflineLevelId = 0;

	m_bNeedRebootAfterMapping = FALSE;
	m_bUsePublicEncryptKeyForDiscovery= FALSE;
	m_bReceivedDiscoveryKey = FALSE;
	m_dwLookConnectionLimitCount = 0;
	m_bAppliedOfflinePolicy = FALSE;
	m_bUpgradePass = FALSE;

	m_bRequestRunningProcess = FALSE;
	m_bIsSetCommini = FALSE;
	// PC보안감사 by yoon.2019.01.08.
	m_bUsePCSecu = FALSE;
	m_sPCSecuPolicyTime = _T("");

	m_bTimeSync = FALSE;
	m_nTimeSync_sec = 0;

	m_bMultiURL = FALSE;

	m_bHardwareDvice = FALSE;
	m_bFirstInstall = FALSE;

	// 2012/09/11, avilon
	InitializeCriticalSection(&m_RequestPoliciesCS);
	InitializeCriticalSection(&m_AllowDataMapCS);
}

CClientManager::~CClientManager()
{
}

// SendLog Thread
DWORD WINAPI SendELogThread(LPVOID arg)
{
	CClientManager *pClentManager = (CClientManager*)arg;
	if(pClentManager)
	{
		pClentManager->SendExecutionLog();
	}
	return  0;
}

void CClientManager::Begin()
{
	WRITELOG(_T("[CClientManager::Begin] this : 0x%x"), this);
	InitManageList();
	
	// by realhan 
	GetOSVersion(&m_OSVerMajor, &m_OSVerMinor);
	
	// Agent 설치 디렉토리에서 BMSIASCOMM.INI 파일을 읽어온다.
	m_bIsSetCommini = InitComm();
	WRITELOG(_T("[CClientManager::Begin] - InitComm 호출 후 m_bIsSetCommini : %d"), m_bIsSetCommini);

	// SYSTEM 디렉토리에서 BMSIASVACC.INI 파일을 읽어온다.
	CString sIniFile = _T("");
	sIniFile = g_pBMSCommAgentDlg->m_sMainAgentPath;
	sIniFile += _T("\\BMSIASVACC.INI");
	WRITELOG(_T("<CClientManager::Begin> 백신.INI : %s"), (LPCTSTR)sIniFile);
	InitVaccineList(sIniFile);

	SetMonitoringFileName();
	UpgradeFailProc();
}

VOID CClientManager::End()
{
	while (m_RecItemList.IsEmpty() == FALSE)
	{
		PRECITEM pItem = (PRECITEM)m_RecItemList.RemoveHead();
		delete pItem;
	}

	while (m_prcList.IsEmpty() == FALSE)
	{
		PPRCLIST prc = (PPRCLIST)m_prcList.RemoveHead();
		delete prc;
	}
}

void CClientManager::SetupProcessList()
{
	if (m_prcList.GetCount() > 0)
	{
		return;
	}

	CString sTemp = _T("");
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, _T(""), BMS_MNG_PROCESSLIST);
	if (SetProcessList(sTemp) == FALSE)
	{
		WRITELOG(_T("## ERROR!! BMS_MNG_PROCESSLIST SetProcessList"));
	}

	BOOL bWOW64 = FALSE;
	IsWow64Process(GetCurrentProcess(), &bWOW64);
	if (bWOW64 == TRUE)
	{
		sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, _T(""), BMS_MNG_PROCESSLIST_X64);
		if (SetProcessList(sTemp) == FALSE)
		{
			WRITELOG(_T("## ERROR!! BMS_MNG_PROCESSLIST_X64 SetProcessList"));
		}
	}
}


int CClientManager::ConnectToServer()
{
	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::ConnectToServer] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);

	// by grman, 2007/10/02 첫 시도인 경우, 정책이 존재하면 백업한다.
	if (m_dwRetryCount == 0)
	{
		BackupPolicyFile();
	}

	CString sSendNICList = GetMACAndIPAddr();
	m_sMACIPList = sSendNICList;

	CString sReq = GetConnectReqString();
	WRITELOG(_T("[CClientManager::ConnectToServer] - User String:%s"), sReq);

	CHAR *pszUTF8 = NULL;
	if (m_bConnectUTF8 == TRUE)
	{
		int nLen = WideCharToMultiByte(CP_UTF8, 0, sReq, sReq.GetLength(), NULL, 0, NULL, NULL);
		pszUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_UTF8, 0, sReq, sReq.GetLength(), pszUTF8, nLen, NULL, NULL);
		pszUTF8[nLen] = NULL;
	}
	else
	{
		int nLen = WideCharToMultiByte(CP_ACP, 0, sReq, sReq.GetLength(), NULL, 0, NULL, NULL);
		pszUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_ACP, 0, sReq, sReq.GetLength(), pszUTF8, nLen, NULL, NULL);
		pszUTF8[nLen] = NULL;
	}

	CStringA sTempReq = pszUTF8;
	if (pszUTF8)
		delete [] pszUTF8; pszUTF8 = NULL;

	sReq = UriEncode(sTempReq.GetBuffer()).c_str();

	CString sCmd = _T("");
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18000"));

	CString sResp = _T("");

	int nRet = 0, nCnt = 0;
	while (nCnt < 2)
	{
		nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sResp, m_bIsHttps);
		if ((m_bFileAgent_boot_run == TRUE && nRet == ERROR_WINHTTP_TIMEOUT) || sResp.GetLength() > 0)
		{
			break;
		}
		else
		{
			nCnt++;
			Sleep(500);
		}
	}

	if (nRet == BM_SERVER_ERR_NO_EXIST_USER)
	{
		// 삭제
		WRITELOG(_T("[CClientManager::ConnectToServer] 미확인 에이전트다. 삭제."));
		AgentRemove(g_sBMSRemoveParam[BMS_REMOVE_FROM_NO_EXIST_USER]);
		return 0;
	}

	// 2009/08/12 by avilon
	// 10번째 Look이 1이면 다른 작업을 하지 않는다.
	CString sLookField = _T("");
	AfxExtractSubString(sLookField, sResp, 11, FIELD_SEPARATOR);

	// 2011/09/20, avilon
	// Discovery : 32, GRADIUS : 16
	if ((sLookField.GetLength() == SERVER_LOOK_RESP_LENGTH) || (sLookField.GetLength() == DISCOVERY_SERVER_LOOK_RESP_LENGTH))
	{
		if (sLookField.GetAt(9) != _T('0'))
		{
			WRITELOG(_T("[CClientManager::ConnectToServer] - 서버 접속 제한!!"));
			return BMS_TRAFFICCONTROL;
		}
		else
		{
			WRITELOG(_T("[CClientManager::ConnectToServer] - 서버 접속 제한이 아니다."));
		}
	}
	else
	{
		WRITELOG(_T("[CClientManager::ConnectToServer] - Look 길이가 맞지 않음!! Look : %s, Length : %d"), sLookField, sLookField.GetLength());
	}
	
	// by grman, 2009/07/17
	// 역시 응답의 유효성을 보고 connected 완료 여부를 판단해야 한다.
	// FIELD_SEPARATOR가 9개 있어야 하지만, 5개 이상인 경우 정상으로 본다.
	// 이 외의 경우 역시 오프라인 정책으로 넘어갈 수 있도록 한다.
	CString sNull = _T("");
	if (AfxExtractSubString(sNull, sResp, 5, FIELD_SEPARATOR) == FALSE)
	{
		WRITELOG(_T("응답이 유효하지 않음"));
		nRet = 7777; // 임시정의
	}
	// end by grman

	// 2011/08/03, avilon
	CString sManager;
	if (AfxExtractSubString(sManager, sResp, 12, FIELD_SEPARATOR) == FALSE)
	{
		WRITELOG(_T("18000 응답의 12번째 값이 이상하다.(%s)"), sResp);
	}

	CString sManagerFileName = _T("");
	sManagerFileName.Format(_T("%srdata\\Manager"), g_pBMSCommAgentDlg->m_sBMSDATLogPath);
	if (sManager.GetLength() > 0)
	{
		if (sManager == _T("I"))
		{
			HANDLE hFind = INVALID_HANDLE_VALUE;
			WIN32_FIND_DATA FindFileData;
			hFind = FindFirstFile(sManagerFileName, &FindFileData);
			if (hFind == INVALID_HANDLE_VALUE)
			{
				WRITELOG(_T("[CClientManager] Manager다."));
				HANDLE hAID = INVALID_HANDLE_VALUE;
				hAID = CreateFile(sManagerFileName, FILE_ALL_ACCESS, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hAID != INVALID_HANDLE_VALUE)
				{
					CloseHandle(hAID);
				}
			}
		}
		else
		{
			if (FileExists(sManagerFileName) == TRUE)
			{
				::DeleteFile(sManagerFileName);
			}
		}
	}
	else
	{
		if (FileExists(sManagerFileName) == TRUE)
		{
			::DeleteFile(sManagerFileName);
		}
	}

	// 접속 Flag 처리
	m_bConnected = (nRet == 0);

	if (m_bConnected == FALSE)
	{
		WRITELOG(_T("서버 접속 실패...%d"), nRet);
	}
	else
	{
		CString sSubString = _T("");
		AfxExtractSubString(sSubString, sResp, 16, g_Separator);
		g_pBMSCommAgentDlg->m_bUserBase = _ttoi(sSubString);
		AfxExtractSubString(sSubString, sResp, 17, g_Separator);
		m_dwUserBaseLogoutTime = _ttoi(sSubString) * (60 * 1000);
		
		if (g_pBMSCommAgentDlg->m_bUserBase == TRUE)
		{
			WRITELOG(_T("[ConnectToServer] 사용자 기반 ON!!! %d, %d"), g_pBMSCommAgentDlg->m_bUserBase, m_dwUserBaseLogoutTime);
			g_pBMSCommAgentDlg->SetTimer(USERBASE_MONITORING_TIMER_ID, 10 * 1000, NULL);

			CreateUserBaseFlagFile();

			if (g_pBMSCommAgentDlg->m_bUpdateComplete == TRUE && ReadUserInfoFile(USERINFO_LOGINID) != _T(""))
			{
				WRITELOG(_T("[ConnectToServer] 업데이트 완료 후에는 Login을 실행하지 않는다!!"));
				g_pBMSCommAgentDlg->m_bUpdateComplete = FALSE;
			}
			else
			{
				TCHAR agent_path[MAX_PATH] = {0, };
				GetAgentPath(agent_path, MAX_PATH);

				CAtlString sAgentPath = _T("");
				sAgentPath = agent_path;
				sAgentPath += BM_LOGIN_PROCESS_NAME;

				HWND hWnd = ::FindWindow(_T("#32770"), BM_LOGIN_WND_TITLE);
				if ((FileExists2(sAgentPath) == TRUE) && !hWnd && g_pBMSCommAgentDlg->m_bUserBaseForSSO == FALSE)
				{
					DeleteUserIDFile();
					WRITELOG(_T("[ConnectToServer] BMSLogin 실행!!!"));
					
					if (!ShellExecute(NULL, _T("open"), sAgentPath, _T(""), NULL, SW_SHOW))
					{
						WRITELOG(_T("[ConnectToServer] ShellExecute 실패 err(%d)"), GetLastError());
					}

					/*
					if (RunIExplorer(sAgentPath) == FALSE)
					{
						WRITELOG(_T("[ConnectToServer] ExecuteProcess 실패 err(%d)"), GetLastError());
					}
					*/
				}
			}		
		}
		else
		{
			WRITELOG(_T("[ConnectToServer] 사용자 기반 OFF!!! %d, %d"), g_pBMSCommAgentDlg->m_bUserBase, m_dwUserBaseLogoutTime);
			g_pBMSCommAgentDlg->KillTimer(USERBASE_MONITORING_TIMER_ID);

			DeleteUserBaseFlagFile();
			DeleteUserIDFile();

			HWND hWnd = ::FindWindow(_T("#32770"), BM_LOGIN_WND_TITLE);
			if (hWnd != NULL)
			{
				::PostMessage(hWnd, BM_SHUTDOWN_AGENT, (WPARAM)NULL, (LPARAM)NULL);
			}
		}

		// 접속 후 처리 (버전 처리, 트래픽, 결과 저장 및 정책 요청)
		if(ProcessConnect(sResp) == BMS_AGENTUPGRADE)
		{
			// 업그레이드가 필요한 경우이다. 더 이상 진행하지 않는다.
			return BMS_AGENTUPGRADE;
		}
	}

	// by grman, 2007/10/02
	// 서버 접속 실패시 정책 생성 (첫 시도에만)
	if (m_bConnected == FALSE)
	{
		if (m_dwRetryCount == 0)
		{
			WRITELOG(_T("BeginTcp - 첫 접속 실패. offline정책(%d)에 따라 정책을 생성한다"), m_OfflinePolicy);
			CreateOfflinePolicy(m_OfflinePolicy);
			RequestUpdatePolicies(TRUE);
			ApplyPolicies();	
		}
		m_dwRetryCount++;
	}
	else
	{
		WRITELOG(_T("##############  test 2"));
		
		// by grman, 2010/02/11
		// 접속 성공 날짜를 레지스트리에 기록한다.
	}
	return 0;
}

// by grman, 2009/01/14
// 주기적으로 서버에 접속하여 변경 여부를 얻어온다.
// Look about : 정책, 제어명령 등
VOID CClientManager::LookServer()
{
	CString sTempAgentID = NumFilter(m_sAgentID);
	if (sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::LookServer] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::LookServer] AgentId : %s"), m_sAgentID);
	}

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSLOOK, sServerURL);
	WRITELOG(_T("[CClientManager::LookServer] - m_stPageInfo.m_GSLOOK - %s"), m_stPageInfo.m_GSLOOK);

	CString sResp = _T(""), sCmd = _T("");
	sCmd.Format(_T("%s%s?"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSLOOK : sPageName);

	// 2010/01/20, avilon
	CString sMACAndIPList = GetMACAndIPAddr();
	CString sChangedNIC = _T("0");
	WRITELOG(_T("[CClientManager::LookServer]   MAC_IP_LIST from API  : %s"), sMACAndIPList);
	WRITELOG(_T("[CClientManager::LookServer]   MAC_IP_LIST from LIST : %s"), m_sMACIPList);
	if (sMACAndIPList.GetLength() > 0)
	{
		if (CheckNICInfoChanged(sMACAndIPList) == TRUE)
		{
			sChangedNIC = _T("1");
			m_bConnected = FALSE;	//18000을 다시 요청한다.
		}
	}
	else
	{
		WRITELOG(_T("[LookServer]   장치로부터 읽어 온 NIC 정보가 비어 있다. 변경 된 경우가 아니다."));
	}

	CString wsCmd = sCmd, wsAgentid = m_sAgentID, wsAgentIP = sMACAndIPList, wsChangeFlage = sChangedNIC;
	CString wsProcessRun = _T("");
	// 2015.03.17, keede122 자산관리 정책이 false일때, 프로세스 실행 목록 검사
	if (m_bUseAsset == FALSE)
	{
		WRITELOG(_T("[CClientManager::LookServer] m_bUseAsset = FALSE, CheckRunningProcessList() 호출"));
		wsProcessRun = CheckRunningProcessList();
	}
	else
	{
		WRITELOG(_T("[CClientManager::LookServer] m_bUseAsset = TRUE, 프로세스 실행 목록을 검사하지않는다."));
	}
	// 2010.03.09 HANGEBS	프로세스 실행 목록 검사 추가
	wsAgentid += g_Separator;
	wsAgentid += wsAgentIP;
	wsAgentid += g_Separator;
	wsAgentid += wsChangeFlage;
	wsAgentid += g_Separator;
	wsAgentid += wsProcessRun;
	// 2012.08.22 by realhan, 에이전트 시간대 전달.
	wsAgentid += g_Separator;
	wsAgentid += getTimeZoneBiasString();
	wsAgentid += g_Separator;
	wsAgentid += m_sDLPPolicyTime;
	wsAgentid += g_Separator;
	wsAgentid += m_sDISPolicyTime;

	int nRet = g_pClientManager_LookServer->SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsAgentid, sResp, m_bIsHttps);
	WRITELOG(_T("[CClientManager::LookServer] - Result(%d):%s"), nRet, sResp);

	// 2010/12/22, avilon
	if (!_tcsicmp(sChangedNIC, _T("1")) && !nRet)
	{
		m_sMACIPList = sMACAndIPList;
		WRITELOG(_T("[CClientManager::LookServer] - IP가 변경 되어 업데이트 한다. m_sMACIPList = %s"), m_sMACIPList);
	}

	// 2010/01/26, avilon
	// 접속 실패시 오프라인 정책 적용..

	// by realhan
	// 접속 실패시 bmsdat 폴더를 감시할 필요가 없다. Monitor Timer 가 죽는 부분 보완후 넣어주자.

	if (nRet)
	{
		m_nRetryConnectFromLookTimer++;
		TCHAR szName[MAX_PATH] = {0, };
		BOOL bOffline = FALSE;
		DWORD dwState = 0;
		BOOL bInterNetConnect = InternetGetConnectedStateEx(&dwState, szName, MAX_PATH, 0);
		if (bInterNetConnect == TRUE)
		{
			WRITELOG(_T("[CClientManager::LookServer] NetWork Is Alive!! : %d"), dwState);
			g_pClientManager_LookServer->CheckNChangeServerInfo();
			if (m_dwLookConnectionLimitCount >= m_nRetryConnectFromLookTimer)
			{
				WRITELOG(_T("[CClientManager::LookServer] LimitCount에 도달하지 않음!! 카운트 : %d "), m_nRetryConnectFromLookTimer);
			}
			else
			{
				WRITELOG(_T("[CClientManager::LookServer] LimitCount에 도달!! 접속 실패 카운트 : %d, LimitCount : %d"), m_nRetryConnectFromLookTimer, m_dwLookConnectionLimitCount);
				bOffline = TRUE;
			}
		}
		else
		{
			WRITELOG(_T("[CClientManager::LookServer] NetWork에 연결되어 있지 않음!! : %d"), dwState);
			bOffline = TRUE;
		}

		if (bOffline == TRUE && m_bAppliedOfflinePolicy == FALSE)
		{
			m_bAppliedOfflinePolicy = TRUE;
			WRITELOG(_T("[CClientManager::LookServer] - 접속이 끊어 졌다. Error Code = %d"), nRet);
			BackupPolicyFile();
			CreateOfflinePolicy(m_OfflinePolicy);
			RequestUpdatePolicies(TRUE);
			ApplyPolicies();
		}
	}
	else if (!nRet && m_nRetryConnectFromLookTimer>0)
	{
		WRITELOG(_T("[LookServer] 다시 연결에 성공했다. Error Code = %d"), nRet);
		m_bAppliedOfflinePolicy = FALSE;
		m_nRetryConnectFromLookTimer = 0;

		if (!RequestAllowList(m_sAgentID, TRUE))
		{
			WRITELOG(_T("[LookServer] 임시정책 사용중. 임시정책 요청 한다."));
			if (m_bGSALLOW_62 == TRUE)
			{
				UpdateInstantLevelEndDate();
				CreateInstantLevel();
			}
			else
			{
				StartInstantPolicies();
			}

			g_pBMSCommAgentDlg->SetTimer(ALLOW_INSTANTLEVEL_TIMER_ID, 1000*60, NULL);
		}
		else
		{
			WRITELOG(_T("[LookServer] 일반 정책 요청."));
			RequestPolicies();
			RequestUpdatePolicies();
		}

		// 2011/09/27, avilon
		if (m_bUseDiscovery == TRUE)
		{
			RequestDiscoveryPolicy();
			//RequestEncryptKeyValue();
		}

		// 2015.03.26, keede122
		if (m_bUseAsset == TRUE)
		{
			RequestAssetPolicies();
		}

		// PC보안감사 by yoon.2019.01.08.
		if (m_bUsePCSecu == TRUE)
		{
			RequestPoliciesPCSecu();
		}
	}

	// 2009/08/24 avilon, 서버 시간과 동기화
	CString sDT = _T(""), sDatetime = _T(""), sTimeZone = _T(""), sTimeSyncSec = _T("");
	AfxExtractSubString(sDT, sResp, 1, g_Separator);
	AfxExtractSubString(sDatetime, sResp, 2, g_Separator);
	AfxExtractSubString(sTimeZone, sResp, 4, g_Separator);
	AfxExtractSubString(sTimeSyncSec, sResp, 5, g_Separator);

	if (m_bTimeSync)
	{
		sTimeSyncSec.Format(_T("%d"), m_nTimeSync_sec);

		if (sDatetime.GetLength() == 14 && (!_tcsicmp(sDatetime.Mid(0, 2), _T("20"))))
		{
			if (SetClientDateTime(sDatetime, sTimeSyncSec) == FALSE)
			{
				WRITELOG(_T("[ClientManager::LookServer]시간 동기화 실패!!"));
			}
		}
		else
		{
			WRITELOG(_T("[ClientManager::LookServer]시간 형식이나 해당 일자가 잘못 되었습니다. DateTime : %s, Length : %d"), sDatetime, sDatetime.GetLength());
		}

// 서버의 타임존과 PC의 타임존을 비교하여 시간 계산
// 		if (sDatetime.GetLength() == 14 && (!_tcsicmp(sDatetime.Mid(0, 2), _T("20"))))
// 		{
// 			SetClientDateTime_new(sDatetime, sTimeZone, m_nTimeSync_sec);
// 		}
// 		else
// 		{
// 			WRITELOG(_T("[ClientManager::LookServer]시간 형식이나 해당 일자가 잘못 되었습니다. DateTime : %s, Length : %d"), sDatetime, sDatetime.GetLength());
// 		}
	}
	else // 시간 동기화 정책으로 빼서 메이저 버전 올릴때 삭제가 필요한 부분
	{

		if (sDatetime.GetLength() == 14 && (!_tcsicmp(sDatetime.Mid(0, 2), _T("20"))))
		{
			if (SetClientDateTime(sDatetime, sTimeSyncSec) == FALSE)
			{
				WRITELOG(_T("[ClientManager::LookServer]시간 동기화 실패!!"));
			}
		}
		else
		{
			WRITELOG(_T("[ClientManager::LookServer]시간 형식이나 해당 일자가 잘못 되었습니다. DateTime : %s, Length : %d"), sDatetime, sDatetime.GetLength());
		}
	}

	if (nRet == 0)
	{
		WriteLastLookServerTime();
	}

	// 2017/08/31 kjh4789 Thread alive 여부 확인
	SaveLookThreadExitTime();

	// 2011/10/17, avilon
	CString sCheck = _T("");
	// by grman, 2009/07/17
	// Look 결과가 비정상 페이지의 응답일 수 있다. 유효성을 검사한다.
	BOOL bIsVaildLookResp = TRUE;
	if (sResp.GetLength() < SERVER_LOOK_RESP_LENGTH)
	{
		bIsVaildLookResp = FALSE;
	}
	else
	{
		AfxExtractSubString(sCheck, sResp, 0, g_Separator);
		sCheck = NumFilter(sCheck);
		// 2011/09/22, avilon - Gradius look = 16, Discovery look = 32
		if((sCheck.GetLength() != SERVER_LOOK_RESP_LENGTH) && (sCheck.GetLength() != DISCOVERY_SERVER_LOOK_RESP_LENGTH) && (sCheck.GetLength() != NEW_SERVER_LOOK_RESP_LENGTH))
		{
			bIsVaildLookResp = FALSE;
		}
	}
	if (bIsVaildLookResp == FALSE)
	{
		return;
	}

	if (nRet == 0)
	{
		// Default
		// 0 정책 변경 여부
		// 1 중지 명령 여부 -> 
		// 2 시작 명령 여부 -> 
		// 3 제거 명령 여부
		// 4 업그레이드 명령 여부
		// 5 잠금 풀기 명령 여부 -> 
		// 6 임시 정책 설정 여부 -> gsallow6.1
		// 7 예외 요청 에이전트
		// 8 예외 요청 매니저
		// 9 서버 트래픽 관리 (Debug)
		// 10 ProcessList
		// 11 설치 프로그램 목록
		// 12 임시정책해지
		// 13 사용자 기반 정책 변경
		// 14 윈도우 업데이트 시작
		// 15 사용하는 정책 전송 (Debug)

		// Discovery
		// 16 디스커버리 정책변경
		// 17 디스커버리 중지
		// 18 디스커버리 시작
		// 19 디스커버리 제거
		// 20 디스커버리 임시정책 요청자
		// 21 디스커버리 임시정책 승인자
		// 22 디스커버리 임시정책 요청
		// 23 디스커버리 임시정책 중지
		// 24 디스커버리 동기화
		// 25 디스커버리 관리자 문서 삭제
		// 26 디스커버리 관리자 강제 재스캔
		// 27
		// 28
		// 29
		// 30
		// 31

		// PC반출
		// 32 PC반출 관리자
		// 33 PC반출 에이전트
		// 34
		// 35

		// New Look
		// 36 하드웨어 스펙
		// 37 정책, 에이전트 파일 무결성 검사
		// 38 오프라인 level 정책 변경
		// 40 PC보안감사 정책 변경
		// 41 PC 정보 요청
		// 42 사용자 기반 공용PC 강제 로그아웃
		// 43 내부 IP 조건 변경
		// 44 하드웨어 장치
		// 45 외부유통 승인권자
		// 46 외부유통 요청자
		// 47

		ProcessDefaultLook(sCheck, sDT);	// 0 ~ 15
		
		// 2012/05/15, avilon - 새로 추가된 Look을 처리한다. Discovery, Takeout
		if (sCheck.GetLength() >= DISCOVERY_SERVER_LOOK_RESP_LENGTH)
		{
			ProcessDiscoverLook(sCheck);	//16 ~ 26
		}

		if (sCheck.GetLength() >= NEW_SERVER_LOOK_RESP_LENGTH)
		{
			ProcessTakoutLook(sCheck);		//32 ~ 35 (34, 35는 사용안하고 있음)
			ProcessNewLook(sCheck);			//36 ~ 48 (46까지 사용중)
		}
	}
}

VOID CClientManager::UpdateLook(int pos, int val)
{
	// 2010/10/12, avilon - agentID를 얻어 오지 못 한 경우 agentinfo에서 다시 읽어 온다.
	WRITELOG(_T("UpdateLook 시작 %d번째 %d"), pos, val);
	CString sTempAgentID = NumFilter(m_sAgentID);
	if (sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::UpdateLook()] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::UpdateLook()] - AgentID = %s"), m_sAgentID);
	}

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::UpdateLook()] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);

 	CString sReq = _T(""), sResp = _T(""), sCmd = _T("");
 	sReq.Format(_T("%d%c%d%c%s"), pos, FIELD_SEPARATOR, val, FIELD_SEPARATOR, m_sAgentID);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18003"));
	WRITELOG(_T("[CClientManager::UpdateLook] - sReq = %s , sCmd = %s"), sReq, sCmd);
	int ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sResp, m_bIsHttps);
	WRITELOG(_T("[CClientManager::UpdateLook] -  Result:%d"), ret);
}
////////////////////////////////////////////////////////////////////////////////////////////////
// Util Functions

BOOL CClientManager::InitComm()
{
	WRITELOG(_T("[InitComm()] Start!!"));
	// fileexist 필요 cfg, ini 실패 시 return false
	if (m_sProjectName == BMS_PROJECT_SAVETONER)	// by grman, 2009/04/25
	{
		SetSaveTonerServerPage();
		return TRUE;
	}
	// 2011/03/03 by realhan CC 모듈 수정
	if (FileExistRegInfo() == TRUE)
	{
		WRITELOG(_T("[InitComm] RegInfo 파일 발견. REGINFO_STRING_SITENAME"));
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_REGINFO_FILENAME, BMS_REGINFO_SITE_NAME, m_sSiteName, _T(""));
	}
	else
	{
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REGINFO_STRING_SITENAME, m_sSiteName);
	}

	// Sections를 얻어온다.
	int nSectionNum = 0;
	TCHAR buff[HC_INI_BUFSIZE] = {0x20,};
	TCHAR sections[HC_INI_BUFSIZE] = {0, };
	TCHAR dec_path[MAX_PATH] = {0, };
	CheckEncryptCommini(dec_path);

	DWORD dwSectionLength = ::GetPrivateProfileSectionNames(buff, HC_INI_BUFSIZE, dec_path);
	int pos = 0;
	BOOL bMakedSect = FALSE;
	for (int i = 0; i < dwSectionLength; i++, pos++)	// section name
	{
		if (buff[i] != '\0')
		{
			if(bMakedSect) pos = 0;
			memcpy(sections + pos, buff+i, 1);
			bMakedSect = FALSE;
			continue;
		}
		else
		{
			nSectionNum++;
			sections[i] = '\0';
			TCHAR *pPosSec = sections;
			while (*pPosSec != NULL)
			{
				CString sSectionName = _T("");
				while (*pPosSec != NULL)
				{
					sSectionName += *pPosSec++;
				}
				//sSectionName.MakeUpper();

				// 각 Section을 분석한다.
				TCHAR pchBuf[BUFSIZ] = {0, };
				CString sTemp = _T("");

				if (sSectionName.Find(_T("Server")) > -1)
				{
					CServerInfo *pNewServerInfo = new CServerInfo;
					pNewServerInfo->m_sServerSection = sSectionName;
					int nURlCnt = 0;
					for (nURlCnt = 0 ; nURlCnt < SERVER_URL_COUNT; nURlCnt++)
					{
						CString sURL = _T("URL");
						if (nURlCnt != 0)
						{
							sURL.AppendFormat(_T("%d"), nURlCnt + 1);
						}
						GetPrivateProfileString(sSectionName, sURL, _T(""), pchBuf, BUFSIZ, dec_path);
						if (_tcsicmp(pchBuf, _T("")) == 0)
						{
							break;
						}
						if (nURlCnt > 0)
						{
							m_bMultiURL = TRUE;
						}

						pNewServerInfo->m_sURL[nURlCnt] = pchBuf;
						ZeroMemory(pchBuf, BUFSIZ);
					}
					SetServerPage(pNewServerInfo, dec_path, 0);
					m_ServerInfoList.AddTail(pNewServerInfo);
				}
				else if (sSectionName.CompareNoCase(_T("Prev Server")) == 0)
				{
					SetPrevServerPage(sSectionName, dec_path);
				}

				pPosSec++;
			}
			memset(sections, 0x00, sizeof(sections));
			bMakedSect = TRUE;
		}
	}

	if (_tcslen(dec_path) > 0)
	{
		::DeleteFile(dec_path);
	}

	CServerInfo *pServerInfoTemp = (CServerInfo *)m_ServerInfoList.GetHead();
	if (pServerInfoTemp->m_sURL[0].Find(_T("http://")) != 0 
		&& pServerInfoTemp->m_sURL[0].Find(_T("https://")) != 0)
	{
		WRITELOG(_T("[InitComm] Comm 주소가 정상적이지 않은것 같다!! 다시 셋팅!!!"));
		return FALSE;
	}
	

	return TRUE;
}

// 정책 적용
// 임시: 땜빵식이다.
void CClientManager::ApplyPolicies()
{
	WRITELOG(_T("[CClientManager::ApplyPolicies] Start"));
	DeletePrevPolicyFile();

	// 기본 설정 적용
	m_SaveCopySizeLimit	= BM_DEFAULT_COPYSAVE_SIZE_LIMIT;

	CGRegulationManager reg_man;
	if(reg_man.Read() < 0)
	{
		return;
	}

	// 사본 크기 제한
	int val = reg_man.getGlobalValue(_T("copysave"));
	WRITELOG(_T("[CClientManager::ApplyPolicies] copysave=%d"), val);
	if(val != 0)
	{
		m_SaveCopySizeLimit = val * 1000000; // MB 단위
	}
	else
	{
		m_SaveCopySizeLimit = 0;
	}

// 	m_OfflinePolicy = reg_man.getGlobalValue(_T("offlinepolicy"));
// 	CString sOfflinePolicy = _T("");
// 	sOfflinePolicy.Format(_T("%d"), m_OfflinePolicy);
// 	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_OFFLINEPOLICY, sOfflinePolicy, _T(""));

	DWORD dwLookInterval = reg_man.getGlobalValue(_T("lookinterval"));
	WRITELOG(_T("[ApplyPolicies] dwLookInterval : %d"), dwLookInterval);
	g_pBMSCommAgentDlg->m_dwLookInterval = (dwLookInterval > 30 && dwLookInterval <= 36000) ? dwLookInterval : LOOKING_TIME_INTERVAL;

	m_sExcepExpan = reg_man.getGlobalString(_T("copysave_ex_ext_list"));
	m_sExcepExpan = Make_lower(m_sExcepExpan);
	WRITELOG(_T("[CClientManager::ApplyPolicies] End"));

	// 2020/09/11 kjh4789 moniotrlog 쓰레드에 필요한 값을 전달한다.
	g_pClientManager_MonitorLog->m_SaveCopySizeLimit = m_SaveCopySizeLimit;
	g_pClientManager_MonitorLog->m_sExcepExpan = m_sExcepExpan;
	
}

void CClientManager::MonitorLogFiles(CString sLogPath)
{
	WRITELOG(_T("[CClientManager::MonitorLogFiles] Start"));
	g_pBMSCommAgentDlg->m_bDelayLog = FALSE;
	CAtlString sDelayLogTime = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_DELAYLOG, BMS_MNG_CUSTOMLIST);

	BOOL bFileLog = TRUE, bAppLog = TRUE, bPrintLog = TRUE, bPinrtJpgLog = TRUE, bAFileLog = TRUE, bNetwork = TRUE, bMMLog = TRUE, bNdisLog = TRUE;
	BOOL bSaveCopyLog = TRUE, bEXLog =TRUE, bGreenLog = TRUE, bDescLog = TRUE, bReqLog =TRUE, bAbnormalLog = TRUE, bFileAccessLog = TRUE, bScreenRecJpg = TRUE, bContentsLog = TRUE;
	BOOL bStatusLog = TRUE, bFILEBlockLog = TRUE, bAPPBlockLog = TRUE, bPRINTBlockLog = TRUE, bAFILEBlockLog = TRUE, bMMBlockLog = TRUE, bFileAccessBlockLog = TRUE, bWinUpdateLog = TRUE;
	BOOL bHashLog = TRUE, bDiscoveryFileLog = TRUE, bDsInstantEndLog = TRUE, bDsStateLog = TRUE, bDsScanStateLog = TRUE, bReqFileLog = TRUE, bRelayFile = TRUE, bPwdMgrLog = TRUE;
	BOOL bDisFileReq = TRUE, bAppFileBlockLog = TRUE, bCancelReqLog = TRUE, bScreenShotLog = TRUE, bScreenShotBlockLog = TRUE, bScreenShotBlockSaveCopyLog = TRUE, bScreenShotSaveCopyLog = TRUE, bChangeExtBlockLog = TRUE, bReqDevLog = TRUE;
	BOOL bAssetLog = TRUE, bDevLog = TRUE;  // 2015.03.16, keede122
	BOOL bDsSeqLog = TRUE;
	// PC보안감사 by yoon.2019.01.08.
	BOOL bPcsecuLog = TRUE;
	BOOL bJsonFileLog = TRUE, bJsonFileBlockLog = TRUE;
	BOOL bHardwareDeviceLog = TRUE;
	BOOL bExDsLog = TRUE, bExDSSCLog = TRUE, bExDSApproveLog = TRUE, bExDSCompleteLog = TRUE;
	BOOL bReciveMailLog =TRUE;
	BOOL bReqAppLog = TRUE, bReqWebLog = TRUE;

	do {

		if(m_bTerminate)
		{
			WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
			break;
		} 

		CFileIndex *pFile = NULL;
		if(bFileLog)
		{
			pFile = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_FileLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_FileLog);
			if(pFile != NULL)
			{
				WRITELOG(_T("'[CClientManager::MonitorLogFiles] FILE 로그 %s 발견"), pFile->m_sFileName);

				if((pFile->m_sFileName.Find(BLOCK_LOG_OLD(FILE_LOG_FILENAME)) != -1))
				{
					CString sOld = pFile->m_sFileName, sNew = pFile->m_sFileName;
					sNew.Replace(BLOCK_LOG_OLD(FILE_LOG_FILENAME), BLOCK_LOG_NEW(FILE_LOG_FILENAME));
					g_Log.CloseFile(pFile);
					int error = _trename(sOld, sNew);
				}
				else
				{
					if(ProcessActionLog(ACTION_COMMAND::COMMAND_FILEACTIONLOG, pFile) == FALSE) 
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_FILEACTIONLOG 전송 실패."));
						bFileLog = FALSE;
						if (_ttoi(sDelayLogTime) > 0)
						{
							WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
							g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						}
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile2 = NULL;
		if(bAppLog)
		{
			pFile2 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_AppLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_AppLog);
			if(pFile2 != NULL)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] APP 로그 %s 발견"), pFile2->m_sFileName);

				if((pFile2->m_sFileName.Find(BLOCK_LOG_OLD(APP_LOG_FILENAME)) != -1))
				{
					CString sOld = pFile2->m_sFileName, sNew = pFile2->m_sFileName;
					sNew.Replace(BLOCK_LOG_OLD(APP_LOG_FILENAME), BLOCK_LOG_NEW(APP_LOG_FILENAME));
					g_Log.CloseFile(pFile2);
					int error = _trename(sOld, sNew);
				}
				else
				{
					if(ProcessActionLog(ACTION_COMMAND::COMMAND_APPACTIONLOG, pFile2) == FALSE) 
					{ 
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_APPACTIONLOG 전송 실패."));
						bAppLog = FALSE;
						if (_ttoi(sDelayLogTime) > 0)
						{
							WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
							g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						}
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile3 = NULL;
		if(bPrintLog)
		{
			pFile3 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_PrintLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_PrintLog);
			if(pFile3 != NULL)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] PRINT 로그 %s 발견"), pFile3->m_sFileName);

				if((pFile3->m_sFileName.Find(BLOCK_LOG_OLD(PRINT_LOG_FILENAME)) != -1))
				{
					CString sOld = pFile3->m_sFileName, sNew = pFile3->m_sFileName;
					sNew.Replace(BLOCK_LOG_OLD(PRINT_LOG_FILENAME), BLOCK_LOG_NEW(PRINT_LOG_FILENAME));
					g_Log.CloseFile(pFile3);
					int error = _trename(sOld, sNew);
				}
				else
				{
					if(ProcessActionLog(ACTION_COMMAND::COMMAND_PRINTACTIONLOG, pFile3) == FALSE) 
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_PRINTACTIONLOG 전송 실패."));
						bPrintLog = FALSE;
						if (_ttoi(sDelayLogTime) > 0)
						{
							WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
							g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						}
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile4 = NULL;
		if(bPinrtJpgLog)
		{
			pFile4 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_PrintJpgLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_PrintJpgLog);
			if(pFile4 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> JPG 로그 %s 발견"), pFile4->m_sFileName);

				if(ProcessActionLog(COMMAND_PRINT_JPG_ACTIONLOG, pFile4) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_PRINT_JPG_ACTIONLOG 전송 실패."));
					bPinrtJpgLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile7 = NULL;
		if(bSaveCopyLog)
		{
			pFile7 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_SaveCopy, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_SaveCopy);
			if(pFile7 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> SaveCopy 로그 %s 발견"), pFile7->m_sFileName);

				DWORD dwDataLength = (DWORD)pFile7->m_File.GetLength();
				if(dwDataLength == 0)
				{
					DeleteFile(pFile7);	// 파일 크기가 0이면 해당 파일을 제거한다.
				}
				else
				{
					// fileupload 기능 사용하여 파일별 임시정책 요청 시 relayfile 옵션 무시
					CString sRelayFile = _T("");
					if (pFile7->m_sFileName.Find(SAVECOPY_RQF_LOG_FILENAME) == -1)
					{
						sRelayFile = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_RELAYFILE, BMS_MNG_CUSTOMLIST);
					}

					if(((sRelayFile == _T("true")) ? ProcessActionLog(COMMAND_MULTIFILE_GET, pFile7) : ProcessActionLog(COMMAND_SAVECOPY_ACTIONLOG, pFile7)) == FALSE)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_MULTIFILE_GET or COMMAND_SAVECOPY_ACTIONLOG 전송 실패."));
						bSaveCopyLog = FALSE;
						if (_ttoi(sDelayLogTime) > 0)
						{
							WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
							g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						}
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile8 = NULL;
		if(bAFileLog)
		{
			pFile8 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_AFileLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_AFileLog);
			if(pFile8 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> AFILE 로그 %s 발견"), pFile8->m_sFileName);

				if((pFile8->m_sFileName.Find(BLOCK_LOG_OLD(AFILE_LOG_FILENAME)) != -1))
				{
					CString sOld = pFile8->m_sFileName, sNew = pFile8->m_sFileName;
					sNew.Replace(BLOCK_LOG_OLD(AFILE_LOG_FILENAME), BLOCK_LOG_NEW(AFILE_LOG_FILENAME));
					g_Log.CloseFile(pFile8);
					int error = _trename(sOld, sNew);
				}
				else
				{
					if(ProcessActionLog(ACTION_COMMAND::COMMAND_FILEACTIONLOG, pFile8) == FALSE) 
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_FILEACTIONLOG 전송 실패."));
						bAFileLog = FALSE;
						if (_ttoi(sDelayLogTime) > 0)
						{
							WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
							g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						}
					}
				}
			}
		}

		CFileIndex *pFile9 = NULL;
		if(bNetwork)
		{
			pFile9 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_NetworkLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_NetworkLog);
			if(pFile9 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> NETWORK 로그 %s 발견"), pFile9->m_sFileName);

				if(ProcessActionLog(COMMAND_APPACTIONLOG, pFile9) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_APPACTIONLOG 전송 실패."));
					bNetwork = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}
		}

		CFileIndex *pFile10 = NULL;
		if(bMMLog)
		{
			pFile10 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_MMLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_MMLog);
			if(pFile10 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> MM 로그 %s 발견"), pFile10->m_sFileName);

				if((pFile10->m_sFileName.Find(BLOCK_LOG_OLD(MM_LOG_FILENAME)) != -1))
				{
					CString sOld = pFile10->m_sFileName, sNew = pFile10->m_sFileName;
					sNew.Replace(BLOCK_LOG_OLD(MM_LOG_FILENAME), BLOCK_LOG_NEW(MM_LOG_FILENAME));
					g_Log.CloseFile(pFile10);
					int error = _trename(sOld, sNew);
				}
				else
				{
					if(ProcessActionLog(ACTION_COMMAND::COMMAND_FILEACTIONLOG, pFile10) == FALSE) 
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_FILEACTIONLOG 전송 실패."));
						bMMLog = FALSE;
						if (_ttoi(sDelayLogTime) > 0)
						{
							WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
							g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						}
					}
				}
			}
		}

		CFileIndex *pFile11 = NULL;
		if(bEXLog)
		{
			pFile11 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_EXLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_EXLog);
			if(pFile11 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Exchange server 로그 %s 발견"), pFile11->m_sFileName);

				if(ProcessActionLog(COMMAND_FILEACTIONLOG, pFile11) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_FILEACTIONLOG 전송 실패."));
					bEXLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}
			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile12 = NULL;
		if(bGreenLog)
		{
			pFile12 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_GreenLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_GreenLog);
			if(pFile12 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Green 로그 %s 발견"), pFile12->m_sFileName);

				if(ProcessActionLog(COMMAND_GREENACTIONLOG, pFile12) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_GREENACTIONLOG 전송 실패."));
					bGreenLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}
			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile13 = NULL;
		if(bDescLog)
		{
			pFile13 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_DesLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_DesLog);
			if(pFile13 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Descriptor 로그 %s 발견"), pFile13->m_sFileName);

				if(ProcessActionLog(COMMAND_DESCRIPTOR_LOG, pFile13) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_DESCRIPTOR_LOG 전송 실패."));
					bDescLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}
			/////////////////////////////////////////////////////////
			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile14 = NULL;
		if(bReqLog)
		{
			pFile14 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ReqLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ReqLog);
			if(pFile14 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> ReqLog 로그 %s 발견"), pFile14->m_sFileName);

				if(ProcessActionLog(COMMAND_REQUSET_LOG, pFile14) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_REQUSET_LOG 전송 실패."));
					bReqLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}
			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile15 = NULL;
		if(bAbnormalLog)
		{
			pFile15 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_AbnormalLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_AbnormalLog);
			if(pFile15 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> AbnormalLog 로그 %s 발견"), pFile15->m_sFileName);

				if(ProcessActionLog(COMMAND_ABNORMAL_LOG, pFile15) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_ABNORMAL_LOG 전송 실패."));
					bAbnormalLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}
			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile16 = NULL;
		if(bFileAccessLog)
		{
			pFile16 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_FileReadLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_FileReadLog);
			if(pFile16 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> FileReadLog 로그 %s 발견"), pFile16->m_sFileName);

				if((pFile16->m_sFileName.Find(BLOCK_LOG_OLD(FILEREADLOG_NAME)) != -1))
				{
					CString sOld = pFile16->m_sFileName, sNew = pFile16->m_sFileName;
					sNew.Replace(BLOCK_LOG_OLD(FILEREADLOG_NAME), BLOCK_LOG_NEW(FILEREADLOG_NAME));
					g_Log.CloseFile(pFile16);
					int error = _trename(sOld, sNew);
				}
				else
				{
					if(ProcessActionLog(ACTION_COMMAND::COMMAND_FILEREAD_LOG, pFile16) == FALSE) 
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_FILEREAD_LOG 전송 실패."));
						bFileAccessLog = FALSE;
						if (_ttoi(sDelayLogTime) > 0)
						{
							WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
							g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						}
					}
				}
			}
			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile17 = NULL;
		if(bContentsLog)
		{
			pFile17 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ContentsLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ContentsLog);
			if(pFile17 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> FileReadLog 로그 %s 발견"), pFile17->m_sFileName);

				if(ProcessActionLog(COMMAND_CONTENTS_LOG, pFile17) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_CONTENTS_LOG 전송 실패."));
					bContentsLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}
			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile18 = NULL;
		if(bStatusLog)
		{
			pFile18 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_StatusLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_StatusLog);
			if(pFile18 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Status 로그 %s 발견"), pFile18->m_sFileName);

				if(ProcessActionLog(COMMAND_STATUS_LOG, pFile18) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_STATUS_LOG 전송 실패."));
					bStatusLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}
			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile19 = NULL;
		if(bFILEBlockLog)
		{
			pFile19 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_FileBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_FileBlockLog);
			if(pFile19 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> FILE BLOCK 로그 %s 발견"), pFile19->m_sFileName);
				if(ProcessBlockLog(BLOCKLOG_COMMAND::COMMAND_FILE_BLOCK_LOG, pFile19) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND_FILE_BLOCK_LOG 전송 실패."));
					bFILEBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile20 = NULL;
		if(bAPPBlockLog)
		{
			pFile20 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_AppBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_AppBlockLog);
			if(pFile20 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> APP BLOCK 로그 %s 발견"), pFile20->m_sFileName);
				if(ProcessBlockLog(BLOCKLOG_COMMAND::COMMAND_APP_BLOCK_LOG, pFile20) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND_APP_BLOCK_LOG 전송 실패."));
					bAPPBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile21 = NULL;
		if(bPRINTBlockLog)
		{
			pFile21 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_PrintBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_PrintBlockLog);
			if(pFile21 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> PRINT BLOCK 로그 %s 발견"), pFile21->m_sFileName);
				if(ProcessBlockLog(BLOCKLOG_COMMAND::COMMAND_FILE_BLOCK_LOG, pFile21) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND_FILE_BLOCK_LOG 전송 실패."));
					bPRINTBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile22 = NULL;
		if(bAFILEBlockLog)
		{
			pFile22 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_AFileBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_AFileBlockLog);
			if(pFile22 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> AFILE BLOCK 로그 %s 발견"), pFile22->m_sFileName);
				if(ProcessBlockLog(BLOCKLOG_COMMAND::COMMAND_FILE_BLOCK_LOG, pFile22) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND_FILE_BLOCK_LOG 전송 실패."));
					bAFILEBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile23 = NULL;
		if(bMMBlockLog)
		{
			pFile23 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_MMBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_MMBlockLog);
			if(pFile23 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> MM BLOCK 로그 %s 발견"), pFile23->m_sFileName);
				if(ProcessBlockLog(BLOCKLOG_COMMAND::COMMAND_FILE_BLOCK_LOG, pFile23) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND_FILE_BLOCK_LOG 전송 실패."));
					bMMBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile24 = NULL;
		if(bHashLog)
		{
			pFile24 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_AgentHashLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_AgentHashLog);
			if(pFile24 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> IntegrityLog 로그 %s 발견"), pFile24->m_sFileName);

				if(ProcessActionLog(COMMAND_AGENT_HASHLOG, pFile24) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_AGENT_HASHLOG 전송 실패."));
					bHashLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile25 = NULL;
		if(bFileAccessBlockLog)
		{
			pFile25 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_FileAccessBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_FileAccessBlockLog);
			if(pFile25 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> FileAccess BLOCK 로그 %s 발견"), pFile25->m_sFileName);
				if(ProcessBlockLog(BLOCKLOG_COMMAND::COMMAND_FILE_BLOCK_LOG, pFile25) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND_FILE_BLOCK_LOG 전송 실패."));
					bFileAccessBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile26 = NULL;
		if(bDiscoveryFileLog)
		{
			pFile26 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_DiscoveryLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_DiscoveryLog);
			if(pFile26 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Discovery 로그 %s 발견"), pFile26->m_sFileName);

				if(ProcessActionLog(COMMAND_AGENT_DISCOVERYLOG, pFile26) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_AGENT_DISCOVERYLOG 전송 실패."));
					bDiscoveryFileLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile27 = NULL;
		if(bDsInstantEndLog)
		{
			pFile27 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_DsEndInstantLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_DsEndInstantLog);
			if(pFile27 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Discovery 임시정책 종료 로그 %s 발견"), pFile27->m_sFileName);

				if(ProcessActionLog(COMMAND_AGENT_END_DSINSTANTLOG, pFile27) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_AGENT_END_DSINSTANTLOG 전송 실패."));
					bDsInstantEndLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile28 = NULL;
		if(bDsStateLog)
		{
			pFile28 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_DsStateLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_DsStateLog);
			if(pFile28 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Discovery 상태 변경 로그 %s 발견"), pFile28->m_sFileName);

				if(ProcessActionLog(COMMAND_AGENT_DISCOVERY_STATELOG, pFile28) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_AGENT_DISCOVERY_STATELOG 전송 실패."));
					bDsStateLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile29 = NULL;
		if(bDsScanStateLog)
		{
			pFile29 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_DsScanStateLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_DsScanStateLog);
			if(pFile29 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Discovery 스캔 상태 로그 %s 발견"), pFile29->m_sFileName);

				if(ProcessActionLog(COMMAND_AGENT_DISCOVERY_SCANSTATELOG, pFile29) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_AGENT_DISCOVERY_SCANSTATELOG 전송 실패."));
					bDsScanStateLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile30 = NULL;
		if(bDsScanStateLog)
		{
			pFile30 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ReqFileLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ReqFileLog);
			if(pFile30 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> 파일별 임시정책 로그 %s 발견"), pFile30->m_sFileName);

				if(ProcessActionLog(COMMAND_REQUEST_FILE_LOG, pFile30) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_REQUEST_FILE_LOG 전송 실패."));
					bReqFileLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile31 = NULL;
		if(bRelayFile)
		{
			pFile31 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_SendReceiveFile, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_SendReceiveFile);
			if(pFile31 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> 파일 이어받기 로그 %s 발견"), pFile31->m_sFileName);
				CString sRelayFile = _T("");
				sRelayFile = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_RELAYFILE, BMS_MNG_CUSTOMLIST);

				if(sRelayFile == _T("true") && ProcessActionLog(COMMAND_MULTIFILE_SENDFILE_GET, pFile31) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_MULTIFILE_SENDFILE_GET 전송 실패."));
					bRelayFile = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile32 = NULL;
		if(bPwdMgrLog)
		{
			pFile32 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_PasswordMgrLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_PasswordMgrLog);
			if(pFile32 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> PasswordManager 로그 %s 발견"), pFile32->m_sFileName);

				if(ProcessActionLog(COMMAND_PASSWORDMGR_LOG, pFile32) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_PASSWORDMGR_LOG 전송 실패."));
					bPwdMgrLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile33 = NULL;
		if(bWinUpdateLog)
		{
			pFile33 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_WinUpdateLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_WinUpdateLog);
			if(pFile33 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> WindowsUpdate 로그 %s 발견"), pFile33->m_sFileName);

				if(ProcessActionLog(COMMAND_WINUPDATE_LOG, pFile33) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_WINUPDATE_LOG 전송 실패."));
					bWinUpdateLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile34 = NULL;
		if(bNdisLog)
		{
			pFile34 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_NdisLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_NdisLog);
			if(pFile34 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Ndis 로그 %s 발견"), pFile34->m_sFileName);

				if(ProcessActionLog(COMMAND_NDIS_LOG, pFile34) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_NDIS_LOG 전송 실패."));
					bNdisLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}
		CFileIndex *pFile35 = NULL;
		if (bDisFileReq == TRUE)
		{
			pFile35 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_DisFileReqLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_DisFileReqLog);
			if(pFile35 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] DisFileReqLog 발견 filename(%s)"), pFile35->m_sFileName);

				if(ProcessActionLog(COMMAND_DISCOVERY_FILE_REQUEST_LOG, pFile35) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_DISCOVERY_FILE_REQUEST_LOG 전송 실패."));
					bDisFileReq = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}
		CFileIndex *pFile36 = NULL;
		if (bAppFileBlockLog == TRUE)
		{
			pFile36 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_AppFileBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_AppFileBlockLog);
			if(pFile36 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] APPFile BLOCK 로그 filename(%s)"), pFile36->m_sFileName);
				if(ProcessBlockLog(BLOCKLOG_COMMAND::COMMAND_FILE_BLOCK_LOG, pFile36) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND_FILE_BLOCK_LOG 전송 실패."));
					bAppFileBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}
		CFileIndex *pFile37 = NULL;
		if (bCancelReqLog == TRUE)
		{
			pFile37 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_CancelReqLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_CancelReqLog);
			if(pFile37 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] Cancel Request 로그 filename(%s)"), pFile37->m_sFileName);
				if(ProcessActionLog(COMMAND_CANCEL_REQUEST_LOG, pFile37) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_CANCEL_REQUEST_LOG 전송 실패."));
					bCancelReqLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}
		CFileIndex *pFile38 = NULL;
		if (bScreenShotBlockLog == TRUE)
		{
			pFile38 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ScreenShotBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ScreenShotBlockLog);
			if (pFile38 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] ScreenShot 로그 filename(%s)"), pFile38->m_sFileName);
				if (ProcessBlockLog(COMMAND_SCREENSHOT_BLOCK_LOG, pFile38) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND 전송 실패."));
					bScreenShotBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if (m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}
		CFileIndex *pFile39 = NULL;
		if (bScreenShotBlockSaveCopyLog == TRUE)
		{
			pFile39 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ScreenShotBlockSaveCopyLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ScreenShotBlockSaveCopyLog);
			if (pFile39 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] ScreenShot Block SaveCopy 로그 filename(%s)"), pFile39->m_sFileName);
				if (ProcessBlockLog(COMMAND_SCREENSHOT_SAVECOPY_BLOCK_LOG, pFile39) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND 전송 실패."));
					bScreenShotBlockSaveCopyLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if (m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}
		//2015.03.16, keede122
		CFileIndex *pFile40 = NULL;
		if (bAssetLog == TRUE)
		{
			pFile40 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_AssetLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_AssetLog);
			if(pFile40 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> Asset 로그 %s 발견"), pFile40->m_sFileName);
				
				if(ProcessActionLog(COMMAND_ASSET_LOG, pFile40) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::AssetLog> COMMAND_ASSET_LOG 전송 실패."));
					bAssetLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::AssetLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile41 = NULL;
		if (bDevLog == TRUE)
		{
			pFile41 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_DevLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_DevLog);
			if(pFile41 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> DevLog 로그 %s 발견"), pFile41->m_sFileName);

				if(ProcessActionLog(COMMAND_DEV_LOG, pFile41) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::DevLog> COMMAND_DEV_LOG 전송 실패."));
					bDevLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::DevLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile42 = NULL;
		if (bDsSeqLog == TRUE)
		{
			pFile42 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_DsSeqLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_DsSeqLog);
			if(pFile42 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> DSSeq 로그 %s 발견"), pFile42->m_sFileName);

				if(ProcessActionLog(COMMAND_AGENT_DISCOVERY_SEQLOG, pFile42) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::DevLog> COMMAND_AGENT_DISCOVERY_SEQLOG 전송 실패."));
					bDsSeqLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::DevLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		// by grman, 2017/12/13
		CFileIndex *pFile43 = NULL;
		if (bChangeExtBlockLog == TRUE)
		{
			pFile43 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ExtChangeBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ExtChangeBlockLog);
			if (pFile43 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> 확장자 변경 차단 로그 %s 발견"), pFile43->m_sFileName);

				if (ProcessBlockLog(COMMAND_FILE_BLOCK_LOG, pFile43) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND 전송 실패."));
					bChangeExtBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if (m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		// by grman, 2018/01/26
		CFileIndex *pFile44 = NULL;
		if (bScreenShotLog == TRUE)
		{
			pFile44 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ScreenShotLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ScreenShotLog);
			if (pFile44 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] ScreenShot 로그 filename(%s)"), pFile44->m_sFileName);
				if (ProcessActionLog(COMMAND_FILEACTIONLOG, pFile44) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ScreenShotLog> COMMAND 전송 실패."));
					bScreenShotLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ScreenShotLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if (m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile45 = NULL;
		if (bScreenShotSaveCopyLog == TRUE)
		{
			pFile45 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ScreenShotSaveCopyLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ScreenShotSaveCopyLog);
			if (pFile45 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] ScreenShot SaveCopy 로그 filename(%s)"), pFile45->m_sFileName);
				if (ProcessActionLog(COMMAND_SCREENSHOT_SAVECOPY_LOG, pFile45) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> COMMAND 전송 실패."));
					bScreenShotSaveCopyLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessBlockLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if (m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile46 = NULL;
		if (bReqDevLog == TRUE)
		{
			pFile46 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ReqDevLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ReqDevLog);
			if (pFile46 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] ReqDev 로그 filename(%s)"), pFile46->m_sFileName);
				if (ProcessActionLog(COMMAND_REQUEST_DEVICE_LOG, pFile46) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ReqDevLog> COMMAND 전송 실패"));
					bReqDevLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ReqDevLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if (m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}


		// PC보안감사 by yoon.2019.01.08.
		CFileIndex *pFile47 = NULL;
		if (bPcsecuLog == TRUE)
		{
			pFile47 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_PCSecuLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_PCSecuLog);
			if (pFile47 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] PCSecuLog 로그 filename(%s)"), pFile47->m_sFileName);
				if (ProcessActionLog(COMMAND_PCSECU_LOG, pFile47) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::PCSecuLog> COMMAND 전송 실패"));
					bPcsecuLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::PCSecuLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if (m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile48 = NULL;
		if (bJsonFileLog)
		{
			pFile48 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_JsonFileLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_JsonFileLog);
			if (pFile48 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] JsonFileLog 로그 filename(%s)"), pFile48->m_sFileName);
				if (ProcessActionLog(COMMAND_JSON_FILE_LOG, pFile48) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::JsonFileLog> COMMAND 전송 실패"));
					bJsonFileLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::JsonFileLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile49 = NULL;
		if (bJsonFileBlockLog)
		{
			pFile49 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_JsonFileBlockLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_JsonFileBlockLog);
			if (pFile49 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] JsonFileBlockLog 로그 filename(%s)"), pFile49->m_sFileName);
				if (ProcessActionLog(COMMAND_JSON_FILE_BLOCK_LOG, pFile49) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::JsonFileLog> COMMAND 전송 실패"));
					bJsonFileBlockLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::JsonFileLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile50 = NULL;
		if (bHardwareDeviceLog)
		{
			pFile50 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_HardwareDeviceLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_HardwareDeviceLog);
			if (pFile50 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] HardwareDeviceLog 로그 filename(%s)"), pFile50->m_sFileName);
				if (ProcessActionLog(COMMAND_HARDWAREDEVICE_FILE_LOG, pFile50) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::HardwareDeviceLog> COMMAND 전송 실패"));
					bHardwareDeviceLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::HardwareDeviceLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile51 = NULL;
		if (bExDsLog)
		{
			pFile51 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_EXDSLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_EXDSLog);
			if (pFile51 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] External Distribution Req 로그 filename(%s)"), pFile51->m_sFileName);
				if (ProcessActionLog(COMMAND_EXDS_REQ_LOG, pFile51) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::EXDSReqLog> COMMAND 전송 실패"));
					bExDsLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::EXDSReqLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile52 = NULL;
		if (bExDSSCLog)
		{
			pFile52 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_EXDSSCLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_EXDSSCLog);
			if (pFile52 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] External Distribution SaveCopy 로그 filename(%s)"), pFile52->m_sFileName);
				if (ProcessActionLog(COMMAND_EXDS_SAVECOPY_LOG, pFile52) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::EXDSSCLog> COMMAND 전송 실패"));
					bExDSSCLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::EXDSSCLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile53 = NULL;
		if (bExDSApproveLog)
		{
			pFile53 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_EXDSApproveLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_EXDSApproveLog);
			if (pFile53 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] External Distribution Approve 로그 filename(%s)"), pFile53->m_sFileName);
				if (ProcessActionLog(COMMAND_EXDS_APPROVE_LOG, pFile53) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::EXDSApproveLog> COMMAND 전송 실패"));
					bExDSApproveLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::EXDSApproveLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile54 = NULL;
		if (bExDSCompleteLog)
		{
			pFile54 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_EXDSCompleteLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_EXDSCompleteLog);
			if (pFile54 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] External Distribution Complete 로그 filename(%s)"), pFile54->m_sFileName);
				if (ProcessActionLog(COMMAND_EXDS_COMPLETE_LOG, pFile54) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::EXDSCompleteLog> COMMAND 전송 실패"));
					bExDSCompleteLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::EXDSCompleteLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile55 = NULL;
		if (bReciveMailLog)
		{
			pFile55 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ReciveMailLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ReciveMailLog);
			if (pFile55 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] Recive Mail 로그 filename(%s)"), pFile55->m_sFileName);
				if (ProcessActionLog(COMMAND_RECIVE_MAIL_LOG, pFile55) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ReciveMailLog> COMMAND 전송 실패"));
					bReciveMailLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ReciveMailLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("[CClientManager::MonitorLogFiles] terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile56 = NULL;
		if (bReciveMailLog)
		{
			pFile56 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ReciveMailSaveCopy, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ReciveMailSaveCopy);
			if (pFile56 != NULL)
			{
				WRITELOG(_T("[MonitorLogFiles] Recive Mail SaveCopy 로그 filename(%s)"), pFile56->m_sFileName);
				if (ProcessActionLog(COMMAND_RECIVE_MAIL_SAVECOPY, pFile56) == FALSE)
				{
					WRITELOG(_T("<MonitorLogFiles::ReciveMailSaveCopy> COMMAND 전송 실패"));
					bReciveMailLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ReciveMailSaveCopy> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						break;
					}
				}
			}
		}

		CFileIndex *pFile57 = NULL;
		if (bReqAppLog)
		{
			pFile57 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ReqAppLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ReqAppLog);
			if (pFile57 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> 어플리케이션 임시정책 로그 %s 발견"), pFile57->m_sFileName);

				if (ProcessActionLog(COMMAND_REQUEST_APP_LOG, pFile57) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_REQUEST_APP_LOG 전송 실패."));
					bReqAppLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}

		CFileIndex *pFile58 = NULL;
		if (bReqWebLog)
		{
			pFile58 = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ReqWebLog, TRUE);
			WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ReqWebLog);
			if (pFile58 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> 웹사이트 임시정책 로그 %s 발견"), pFile58->m_sFileName);

				if (ProcessActionLog(COMMAND_REQUEST_WEB_LOG, pFile58) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_REQUEST_WEB_LOG 전송 실패."));
					bReqWebLog = FALSE;
					if (_ttoi(sDelayLogTime) > 0)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
						g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
					}
				}
			}

			if(m_bTerminate)
			{
				WRITELOG(_T("<MonitorLogFiles> terminated. while break"));
				break;
			}
		}


		///////////////////////////////////////////////////////////////////////
		// FileLog가 더이상 없는 경우 유출에 대한 화면 저장을 서버로 전송한다.
		CFileIndex *pFileR = NULL;
		if(bScreenRecJpg == TRUE)
		{
			if(pFile==NULL && pFile8==NULL && pFile10==NULL)
			{
				pFileR = g_Log.OpenFile(sLogPath + m_sMonitoringFileName_ScreenRecJpg, TRUE);
				WRITELOG(_T("%s%s"), sLogPath, m_sMonitoringFileName_ScreenRecJpg);
				if(pFileR != NULL)
				{
					WRITELOG(_T("<MonitorLogFiles> SCREEN 로그 %s 발견"), pFileR->m_sFileName);

					if(ProcessActionLog(COMMAND_SCREENREC_JPG_ACTIONLOG, pFileR) == FALSE)
					{
						WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> COMMAND_SCREENREC_JPG_ACTIONLOG 전송 실패."));
						bScreenRecJpg = FALSE;
						if (_ttoi(sDelayLogTime) > 0)
						{
							WRITELOG(_T("<MonitorLogFiles::ProcessActionLog> DelayLog ON!! while break"));
							g_pBMSCommAgentDlg->m_bDelayLog = TRUE;
						}
					}
				}	
			}
		}

		// 아무 로그도 없으면 루프 종료
		if(pFile==NULL && pFile2==NULL && pFile3==NULL && pFile4==NULL && pFile7==NULL && pFile8==NULL && pFile9==NULL && pFile10==NULL&& pFile11==NULL 
			&&pFileR==NULL && pFile12 == NULL && pFile13 == NULL && pFile14 == NULL && pFile15 == NULL && pFile16 == NULL && pFile17 == NULL && pFile18 == NULL
			&& pFile19 == NULL && pFile20 == NULL && pFile21 == NULL && pFile22 == NULL && pFile23 == NULL && pFile24 == NULL && pFile25 == NULL && pFile26 == NULL
			&& pFile27 == NULL && pFile28 == NULL && pFile29 == NULL && pFile30 == NULL && pFile31 == NULL && pFile32 == NULL && pFile33 == NULL && pFile34 == NULL
			&& pFile35 == NULL && pFile36 == NULL && pFile37 == NULL && pFile38 == NULL && pFile39 == NULL && pFile40 == NULL && pFile41 == NULL && pFile42 == NULL
			&& pFile43 == NULL && pFile44 == NULL && pFile45 == NULL && pFile46 == NULL && pFile47 == NULL && pFile48 == NULL && pFile49 == NULL && pFile50 == NULL
			&& pFile51 == NULL && pFile52 == NULL && pFile53 == NULL && pFile54 == NULL && pFile55 == NULL && pFile56 == NULL && pFile57 == NULL && pFile58 == NULL)
		{
			WRITELOG(_T("<MonitorLogFiles> 더 보낼 로그가 없다. while break"));
			break;
		}

		if(pFile) WRITELOG(_T("still alive : pFile"));
		if(pFile2) WRITELOG(_T("still alive : pFile2"));
		if(pFile3) WRITELOG(_T("still alive : pFile3"));
		if(pFile4) WRITELOG(_T("still alive : pFile4"));
		if(pFile7) WRITELOG(_T("still alive : pFile7"));
		if(pFile8) WRITELOG(_T("still alive : pFile8"));
		if(pFile9) WRITELOG(_T("still alive : pFile9"));
		if(pFile10) WRITELOG(_T("still alive : pFile10"));
		if(pFile11) WRITELOG(_T("still alive : pFile11"));
		if(pFile12) WRITELOG(_T("still alive : pFile12"));
		if(pFile13) WRITELOG(_T("still alive : pFile13"));
		if(pFile14) WRITELOG(_T("still alive : pFile14"));
		if(pFile15) WRITELOG(_T("still alive : pFile15"));
		if(pFile16) WRITELOG(_T("still alive : pFile16"));
		if(pFile17) WRITELOG(_T("still alive : pFile17"));
		if(pFile18) WRITELOG(_T("still alive : pFile18"));
		if(pFile19) WRITELOG(_T("still alive : pFile19"));
		if(pFile20) WRITELOG(_T("still alive : pFile20"));
		if(pFile21) WRITELOG(_T("still alive : pFile21"));
		if(pFile22) WRITELOG(_T("still alive : pFile22"));
		if(pFile23) WRITELOG(_T("still alive : pFile23"));
		if(pFile24) WRITELOG(_T("still alive : pFile24"));
		if(pFile25) WRITELOG(_T("still alive : pFile25"));
		if(pFile26) WRITELOG(_T("still alive : pFile26"));
		if(pFile27) WRITELOG(_T("still alive : pFile27"));
		if(pFile28) WRITELOG(_T("still alive : pFile28"));
		if(pFile29) WRITELOG(_T("still alive : pFile29"));
		if(pFile30) WRITELOG(_T("still alive : pFile30"));
		if(pFile31) WRITELOG(_T("still alive : pFile31"));
		if(pFile32) WRITELOG(_T("still alive : pFile32"));
		if(pFile33) WRITELOG(_T("still alive : pFile33"));
		if(pFile34) WRITELOG(_T("still alive : pFile34"));
		if(pFile35) WRITELOG(_T("still alive : pFile35"));
		if(pFile36) WRITELOG(_T("still alive : pFile36"));
		if(pFile37) WRITELOG(_T("still alive : pFile37"));
		if(pFile38) WRITELOG(_T("still alive : pFile38"));
		if(pFile39) WRITELOG(_T("still alive : pFile39"));
		if(pFile40) WRITELOG(_T("still alive : pFile40")); // 2015.03.16, keede122
		if(pFile41) WRITELOG(_T("still alive : pFile41"));
		if(pFile47) WRITELOG(_T("still alive : pFile47")); // PC보안감사 by yoon.2019.01.08.
		if(pFile48) WRITELOG(_T("still alive : pFile48"));
		if(pFile49) WRITELOG(_T("still alive : pFile49"));
		if(pFile50) WRITELOG(_T("still alive : pFile50"));
		if(pFile51) WRITELOG(_T("still alive : pFile51"));
		if(pFile52) WRITELOG(_T("still alive : pFile52"));
		if(pFile53) WRITELOG(_T("still alive : pFile53"));
		if(pFile54) WRITELOG(_T("still alive : pFile54"));
		if(pFile55) WRITELOG(_T("still alive : pFile55"));
		if(pFile56) WRITELOG(_T("still alive : pFile56"));
		if(pFile57) WRITELOG(_T("still alive : pFile57"));
		if(pFile58) WRITELOG(_T("still alive : pFile58"));
		if(pFileR) WRITELOG(_T("still alive : pFileR"));

		ProcMessage();
	} while(TRUE);

	DeleteOfflineDiscoveryPrevFolder(sLogPath);
	MonitoringDiscoveryOfflineReqLog(sLogPath);

	WRITELOG(_T("[MonitorLogFiles] End"));
}

void CClientManager::MonitoringDiscoveryOfflineReqLog(__in CString sLogPath)
{
	WRITELOG(_T("[CClientManager::MonitoringDiscoveryOfflineReqLog] Start!!"));
	CString sDiscoveryOfflineReqLogPath = _T("");
	sDiscoveryOfflineReqLogPath = GetOfflineClearPath(sLogPath);

	WIN32_FIND_DATA fdsFileData;
	HANDLE hFileHandle;
	hFileHandle = FindFirstFile((sDiscoveryOfflineReqLogPath+_T("*.*")), &fdsFileData);
	if(hFileHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (((fdsFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) && CString(fdsFileData.cFileName) != _T(".") && CString(fdsFileData.cFileName) != _T(".."))
			{
				WRITELOG(_T("[CClientManager::MonitoringDiscoveryOfflineReqLog] 폴더 발견 name:%s"), fdsFileData.cFileName);
				CString sFolderPath = _T("");
				sFolderPath.Format(_T("%s%s\\"), sDiscoveryOfflineReqLogPath, fdsFileData.cFileName);
				ProcessDiscoveryOfflineEndDataLog(sFolderPath);

			}
		} while(FindNextFile(hFileHandle, &fdsFileData));
	}
	FindClose(hFileHandle);
}

CString CClientManager::GetOfflineClearPath(__in CString sLogPath)
{
	CString sRet = _T("");
	sRet.Format(_T("%s%s\\%s\\"), sLogPath, _T("rdata"), BMDC_OFFILNE_DISCOVERY_TEMP_FOLDER);

	return sRet;
}

// 파일을 삭제한다.
BOOL CClientManager::DeleteFile(CFileIndex *pFile)
{
	CString sFileName = pFile->m_sFileName;
	BOOL bClose = g_Log.CloseFile(pFile);
	if (bClose == FALSE)
	{
		DWORD dwErr = GetLastError();
		WRITELOG(_T("======================="));
		WRITELOG(_T("$$$ Close 되지 않는 파일 발생 $$$ : %s(%d) ") + sFileName, dwErr);
		WRITELOG(_T("======================="));
	}

	g_pBMSCommAgentDlg->PrintMsg(_T("로그를 삭제하는 중입니다..."));
	BOOL bRet = TRUE;
	bRet = ::DeleteFile(sFileName);
	if (bRet == FALSE)
	{
		DWORD dwErr = GetLastError();
		// 지워지지 않는 목록에 추가한다.
		//g_Log.AddRemovedFileList(sFileName);
		WRITELOG(_T("======================="));
		WRITELOG(_T("$$$ Delete 되지 않는 파일 발생 $$$ : %s(%d)"), sFileName, dwErr);
		
		// 파일을 rename한다.
		CString sGBFileName;
		sGBFileName.Format(_T("%s%s_%ld_%s"),
			GetFolderNameOnly(sFileName), BM_RENAME, GetTickCount(), GetFileNameOnly(sFileName));
		int rename_result = _trename(sFileName, sGBFileName);
		g_Log.AddRemovedFileList(sGBFileName);
		WRITELOG(_T("삭제되지 않아 rename했다(%d):%s, %d"), rename_result, sGBFileName, GetLastError());
		WRITELOG(_T("======================="));
	}

	return bRet;
}

DWORD CClientManager::CreateRemoveData(CString sFileName)
{
	CString sPath = _T("");
	DWORD dwErr = 0;
	sPath.Format(_T("%sRemoveData"), g_pBMSCommAgentDlg->m_sBMSDATLogPath);
	CreateFolder(sPath);
	sPath.AppendFormat(_T("\\RemoveList"));
	sFileName.AppendFormat(_T("\n"));
	MultiLanguageWriteFile(sPath, sFileName, CHARSET_MULTIBYTE);
	WRITELOG(_T("[CreateRemoveData] ErrorCheck : %d"), dwErr);
	dwErr = GetLastError();
	return dwErr;
}

int CClientManager::CreateEdata(LPCTSTR path)
{
	WRITELOG(_T("[CClientManager] CreateEdata Start"));
	//NULL DACL
	DWORD dwResult = 0;
	SECURITY_ATTRIBUTES LogPathSA;
	SECURITY_DESCRIPTOR LogPathSD;
	// security inits
	ZeroMemory(&LogPathSA,sizeof(SECURITY_ATTRIBUTES));
	if (InitializeSecurityDescriptor(&LogPathSD, SECURITY_DESCRIPTOR_REVISION) == FALSE)
	{
		WRITELOG(_T("[CClientManager] CreateEdata::InitializeSecurityDescriptor error : %d"), GetLastError());
		return -1;
	}
	// set NULL DACL on the SD
	if (SetSecurityDescriptorDacl(&LogPathSD, TRUE, (PACL)NULL, FALSE) == FALSE)
	{
		WRITELOG(_T("[CClientManager] CreateEdata::SetSecurityDescriptorDacl error : %d"), GetLastError());
		return -2;
	}
	// now set up the security attributes
	LogPathSA.nLength = sizeof(SECURITY_ATTRIBUTES);
	LogPathSA.bInheritHandle  = TRUE; 
	LogPathSA.lpSecurityDescriptor = &LogPathSD;
	if (CreateDirectory(path, &LogPathSA) == FALSE)
	{
		WRITELOG(_T("[CClientManager] CreateEdata::CreateDirectory error : %d"), GetLastError());
		return -3;
	}
	WRITELOG(_T("[CClientManager] CreateEdata End"));
	return 0;
}

// 파일을 삭제한다.
BOOL CClientManager::MoveFile(CFileIndex *pFile)
{
	WRITELOG(_T("[CClientManager] MoveFile Start : %s"), pFile->m_sFileName);
	CString sFileName = pFile->m_sFileName;
	BOOL bClose = g_Log.CloseFile(pFile);
	if (bClose == FALSE)
	{
		DWORD dwErr = GetLastError();
		WRITELOG(_T("======================="));
		WRITELOG(_T("$$$ Close 되지 않는 파일 발생 $$$ : ") + sFileName);
		WRITELOG(_T("======================="));
	}

	g_pBMSCommAgentDlg->PrintMsg(_T("로그를 삭제하는 중입니다..."));
	BOOL bRet = TRUE;
	CString sPath = GetFolderNameOnly(sFileName), sName = GetFileNameOnly(sFileName);
	CreateEdata(sPath + _T("edata\\"));
	// TickCount 생성 후 파일 이름 뒤에 붙이기.
	CString sTickCount = _T("");
	sTickCount.Format(_T("%d"), GetTickCount());
	bRet = ::MoveFile(sPath + sName, sPath + _T("edata\\") + sName + _T("_") + sTickCount);
	if (bRet == FALSE)
	{
		WRITELOG(_T("[CClientManager] MoveFile error : %d"), GetLastError());
	}
	WRITELOG(_T("[CClientManager] MoveFile End : %d"), bRet);

	return bRet;
}

// 서버에 사용자 이름을 전달한다.
void CClientManager::ProcRequestAgentID(BOOL bCreateSubpcAutomatic)
{
	// 패킷 헤더와 본체를 만들어서 서버에 전달한다.
	CString sUserName = g_pBMSCommAgentDlg->m_pUserRegExDlg->m_sUserName;
	int nLen = sUserName.GetLength()+sizeof(TCHAR);

	TCHAR *pBuf = (TCHAR*)malloc(nLen*sizeof(TCHAR));
	ZeroMemory(pBuf, nLen+sizeof(TCHAR));
	_sntprintf(pBuf, nLen, _T("%s"), sUserName);
	
	CString agentid = _T(""), deptname = _T(""), username = _T(""), temp = _T("");
	CString sRespone = SendUserNameToServer(pBuf, nLen, bCreateSubpcAutomatic);
	int i = 0;

	PrintAgentList(sRespone);
	free(pBuf);

	WRITELOG(_T("서버측에 사용자이름 보냄"));
}

void CClientManager::ProcCreateSubpcAutoMatic(CString sSubpcMappingType)
{
	// 패킷 헤더와 본체를 만들어서 서버에 전달한다.
	CString sLine, sMacAddress;
	TCHAR chSeparator = FIELD_SEPARATOR;
	CString sError = _T("");

	// AgentID를 얻는다.
	int nIndex = -1;
	nIndex = g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.GetNextItem(-1, LVNI_SELECTED);
	if (nIndex < 0)
	{
		return;
	}

	CString sAgentID = _T(""), sUserName = _T(""), sDeptName = _T(""), sAgentKey = _T("");
	int nAgentID = 0;
	sDeptName = g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.GetItemText(nIndex, 0);
	sUserName = g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.GetItemText(nIndex, 1);
	sAgentKey.Format(_T("%s@%s"), sDeptName, sUserName);
	m_map_AgentID.Lookup(sAgentKey, nAgentID);
	sAgentID.Format(_T("%d"), nAgentID);

	CString sPCName = GetPCName();
	sMacAddress = GetAllMACAddress();

	int nSize = sAgentID.GetLength()+sMacAddress.GetLength()+sSubpcMappingType.GetLength()+sPCName.GetLength()+sizeof(TCHAR)*3;
	TCHAR *pBuf = (TCHAR*)malloc(nSize*sizeof(TCHAR)+sizeof(TCHAR));
	ZeroMemory(pBuf, nSize*sizeof(TCHAR)+sizeof(TCHAR));

	_sntprintf(pBuf, nSize*sizeof(TCHAR), _T("%s%c%s%c%s%c%s"), sAgentID, g_Separator, sSubpcMappingType, g_Separator, sMacAddress, g_Separator, sPCName);

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSREG, sServerURL);
	WRITELOG(_T("[CClientManager::ProcCreateSubpcAutoMatic] - m_stPageInfo.m_GSREG - %s"), m_stPageInfo.m_GSREG);

	CString sResp = _T(""), sCmd = _T(""), sReq = pBuf;
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSREG : sPageName, _T("18107"));

	if (pBuf)
	{
		free(pBuf); pBuf= NULL;
	}

	int nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sResp, m_bIsHttps);

	// 2011/07/13, avilon
	// 기존 설치 된 정보를 알려 준다.
	WRITELOG(_T("[CClientManager::ProcCreateSubpcAutoMatic] sResp=%s"), sResp);
	CString sDupFlag = _T("");
	AfxExtractSubString(sDupFlag, sResp, 0, g_Separator);
	if (sDupFlag.CompareNoCase(_T("ALERT")) == 0)
	{
		CString sPopUpContents=_T(""), sWidth=_T(""), sHeight=_T("");
		AfxExtractSubString(sWidth, sResp, 1, chSeparator);
		AfxExtractSubString(sHeight, sResp, 2, chSeparator);
		AfxExtractSubString(sPopUpContents, sResp, 3, chSeparator);
		CHtmlViewDlg cHtmlDlg;
		cHtmlDlg.m_sContentsUrl = sPopUpContents;
		cHtmlDlg.m_nWidth = _ttoi(sWidth);
		cHtmlDlg.m_nHeight = _ttoi(sHeight);
		cHtmlDlg.DoModal();

		return;
	}

	if (nRet == BM_SERVER_ERR_INSTALLED_USER)
	{
		WRITELOG(_T("[CClientManager::ProcCreateSubpcAutoMatic] 중복 설치 방지."));
		g_pBMSCommAgentDlg->m_pUserRegExDlg->MessageBox(LoadLanString(160), LoadLanString(186), MB_OK);
		return;
	}
	else if (nRet != 0)
	{
		WRITELOG(_T("[CClientManager::ProcCreateSubpcAutoMatic] SendAndReceive 실패"));
		sError.Format(_T("%s%d"), LoadLanString(163), nRet);
		g_pBMSCommAgentDlg->m_pUserRegExDlg->MessageBox(sError, LoadLanString(186), MB_OK);
		return;
	}
	else
	{
		if (IsValidResponse(sResp, sCmd) == FALSE)
		{
			WRITELOG(_T("[CClientManager::ProcCreateSubpcAutoMatic] Response가 유효하지 않다."));
			return;
		}
	}

	CString sAllocName[3] = {_T("AGENTID="), _T("USERNAME="), _T("DEPTNAME=")};
	CString sAllocValue[3] = {sAgentID, sUserName, sDeptName};
	for (int nCnt = 0 ; nCnt < 3; nCnt++)
	{
		int nSPos = 0, nEPos = 0;
		nSPos = sResp.Find(sAllocName[nCnt]);
		if (nSPos > -1)
		{
			nSPos += sAllocName[nCnt].GetLength();
			nEPos = sResp.Find(_T("\n"), nSPos);
			if (nEPos != -1)
			{
				sAllocValue[nCnt] = sResp.Mid(nSPos, nEPos - nSPos);
			}
			else
			{
				sAllocValue[nCnt] = sResp.Mid(nSPos);
			}
			
			WRITELOG(_T("[CClientManager::ProcCreateSubpcAutoMatic] %s%s"), sAllocName[nCnt], sAllocValue[nCnt]);
		}
		else
		{
			continue;
		}
	}
	
	m_sAgentID = sAllocValue[0];
	m_sUserName = sAllocValue[1];
	m_map_AgentID.RemoveAll();

	// 2011/03/29, avilon
	// agentID를 저장 해 둔다..
	CString sAgentIdFileName = _T("");
	sAgentIdFileName.Format(_T("%stmp\\SID_%s"), g_pBMSCommAgentDlg->m_sBMSDATLogPath, m_sAgentID);
	HANDLE hAID = NULL;
	hAID = CreateFile(sAgentIdFileName, FILE_ALL_ACCESS, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hAID != INVALID_HANDLE_VALUE)
		CloseHandle(hAID);

	WRITELOG(_T("[CClientManager::ProcSelectAgentID] agentid : %s, deptname : %s, username : %s"), m_sAgentID, sDeptName, m_sUserName);
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTID, m_sAgentID, _T(""));
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_DEPTNAME, sAllocValue[2], _T(""));
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTNAME, m_sUserName, _T(""));

	// 2011/03/03 by realhan CC 모듈 수정
	if (m_bIsLogEncoded == FALSE)
	{
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTID, m_sAgentID);	
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTNAME, m_sUserName);	
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_DEPTNAME, sDeptName);
	}

	if (m_bNeedRebootAfterMapping == TRUE)
	{
		RequireReboot();
	}

	m_bNeedAgentID = FALSE;
	PostMessage(g_pBMSCommAgentDlg->m_hWnd, BMSM_USER_REG_DIALOG_CLOSE, NULL, (LPARAM)m_bNeedRebootAfterMapping);
	if(g_pBMSCommAgentDlg->SetTimer(MONITORING_TIMER_ID, 10000, NULL))
		g_pBMSCommAgentDlg->m_bMonitoringTimer = TRUE;

	WRITELOG(_T("[CClientManager::ProcSelectAgentID] 서버측에 AgentID, MacAddress 보냄"));
}

// 서버에 사용자가 선택한 AgentID와 MAC Address를 전달한다.
void CClientManager::ProcSelectAgentID()
{
	// 패킷 헤더와 본체를 만들어서 서버에 전달한다.
	CString sLine, sMacAddress;
	TCHAR chSeparator = FIELD_SEPARATOR;

	// AgentID를 얻는다.
	int nIndex = -1;
	nIndex = g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.GetNextItem(-1, LVNI_SELECTED);
	if (nIndex < 0)
	{
		return;
	}

	CString sAgentID = _T(""), sUserName = _T(""), sDeptName = _T(""), sAgentKey = _T("");
	int nAgentID = 0;
	sDeptName = g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.GetItemText(nIndex, 0);
	sUserName = g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.GetItemText(nIndex, 1);
	sAgentKey.Format(_T("%s@%s"), sDeptName, sUserName);
	m_map_AgentID.Lookup(sAgentKey, nAgentID);
	sAgentID.Format(_T("%d"), nAgentID);


	sMacAddress = GetAllMACAddress();

	int nSize = sAgentID.GetLength()+sMacAddress.GetLength()+sizeof(TCHAR);
	TCHAR *pBuf = (TCHAR*)malloc(nSize*sizeof(TCHAR)+sizeof(TCHAR));
	ZeroMemory(pBuf, nSize*sizeof(TCHAR)+sizeof(TCHAR));

	_sntprintf(pBuf, nSize*sizeof(TCHAR), _T("%s%c%s"), sAgentID, g_Separator, sMacAddress);

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSREG, sServerURL);
	WRITELOG(_T("[CClientManager::ProcSelectAgentID] - m_stPageInfo.m_GSREG - %s"), m_stPageInfo.m_GSREG);

	CString sResp = _T(""), sCmd = _T(""), sReq = pBuf;
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSREG : sPageName, _T("18100"));

	free(pBuf);

	int nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sResp, m_bIsHttps);

	// 2011/07/13, avilon
	// 기존 설치 된 정보를 알려 준다.
	WRITELOG(_T("[CClientManager::ProcSelectAgentID] sResp=%s"), sResp);
	CString sDupFlag = _T("");
	AfxExtractSubString(sDupFlag, sResp, 0, g_Separator);
	if (sDupFlag.CompareNoCase(_T("ALERT")) == 0)
	{
		CString sPopUpContents=_T(""), sWidth=_T(""), sHeight=_T("");
		AfxExtractSubString(sWidth, sResp, 1, chSeparator);
		AfxExtractSubString(sHeight, sResp, 2, chSeparator);
		AfxExtractSubString(sPopUpContents, sResp, 3, chSeparator);
		CHtmlViewDlg cHtmlDlg;
		cHtmlDlg.m_sContentsUrl = sPopUpContents;
		cHtmlDlg.m_nWidth = _ttoi(sWidth);
		cHtmlDlg.m_nHeight = _ttoi(sHeight);
		cHtmlDlg.DoModal();

		return;
	}

	if (nRet == BM_SERVER_ERR_INSTALLED_USER)
	{
		WRITELOG(_T("[CClientManager::ProcSelectAgentID] 중복 설치 방지."));
		g_pBMSCommAgentDlg->m_pUserRegExDlg->MessageBox(LoadLanString(160), LoadLanString(186), MB_OK);
		return;
	}
	else if (nRet != 0)
	{
		WRITELOG(_T("[CClientManager::ProcSelectAgentID] SendAndReceive 실패"));
		return;
	}
	else
	{
		WRITELOG(_T("[CClientManager::ProcSelectAgentID] SendAndReceive 성공!!"));
	}

	m_sAgentID = sAgentID; 
	m_sUserName = sUserName;
	m_map_AgentID.RemoveAll();
	
	
	// 2011/03/29, avilon
	// agentID를 저장 해 둔다..
	CString sAgentIdFileName = _T("");
	sAgentIdFileName.Format(_T("%stmp\\SID_%s"), g_pBMSCommAgentDlg->m_sBMSDATLogPath, sAgentID);
	HANDLE hAID = NULL;
	hAID = CreateFile(sAgentIdFileName, FILE_ALL_ACCESS, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hAID != INVALID_HANDLE_VALUE)
		CloseHandle(hAID);

	WRITELOG(_T("[CClientManager::ProcSelectAgentID] agentid : %s, deptname : %s, username : %s"), m_sAgentID, sDeptName, m_sUserName);
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTID, m_sAgentID, _T(""));
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_DEPTNAME, sDeptName, _T(""));
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTNAME, m_sUserName, _T(""));

	// 2011/03/03 by realhan CC 모듈 수정
	if (m_bIsLogEncoded == FALSE)
	{
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTID, m_sAgentID);	
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTNAME, m_sUserName);	
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_DEPTNAME, sDeptName);
	}
	

	if (m_bNeedRebootAfterMapping == TRUE)
	{
		RequireReboot();
	}

	m_bNeedAgentID = FALSE;
	PostMessage(g_pBMSCommAgentDlg->m_hWnd, BMSM_USER_REG_DIALOG_CLOSE, NULL, (LPARAM)m_bNeedRebootAfterMapping);
	if(g_pBMSCommAgentDlg->SetTimer(MONITORING_TIMER_ID, 10000, NULL))
		g_pBMSCommAgentDlg->m_bMonitoringTimer = TRUE;
	
	WRITELOG(_T("[CClientManager::ProcSelectAgentID] 서버측에 AgentID, MacAddress 보냄"));
}

// by grman, 2008/03/07
// 서버로 부터 자동 할당 받은 agentid를 local에 등록하고 그 값을 다시 서버에 전송한다.
BOOL CClientManager::ProcAutoSelectedAgentID(CReceivedData *pReceivedData)
{
	// CString으로 옮겨담는다.
	TCHAR *pBuf = (TCHAR*)malloc(pReceivedData->m_nReceivedDataLength*sizeof(TCHAR)+sizeof(TCHAR));
	ZeroMemory(pBuf, pReceivedData->m_nReceivedDataLength*sizeof(TCHAR)+sizeof(TCHAR));
	_tmemcpy(pBuf, pReceivedData->m_pData, pReceivedData->m_nReceivedDataLength*sizeof(TCHAR));
	CString sLine = pBuf;
	free(pBuf);
	
	WRITELOG(_T("[CClientManager::ProcAutoSelectedAgentID] 받은 내용:%s"), sLine);

	if(sLine.GetLength() == 0 || sLine.Find(FIELD_SEPARATOR) < 0)
	{
		// 검색된 agent가 없다.
		return FALSE;
	}

	// 패킷 헤더와 본체를 만들어서 서버에 전달한다.
	CString sMacAddress = _T("");
	TCHAR chSeparator = FIELD_SEPARATOR;

	// sLine에는 agentid와 이름이 들어있다.
	CString sDeptName = _T("");
	AfxExtractSubString(m_sAgentID, sLine, 0, FIELD_SEPARATOR);
	AfxExtractSubString(m_sUserName, sLine, 1, FIELD_SEPARATOR);
	AfxExtractSubString(sDeptName, sLine, 2, FIELD_SEPARATOR);

	if(m_sAgentID.GetLength() == 0 || m_sUserName.GetLength() == 0)
	{
		// 뭔가 잘못된 경우다.
		return FALSE;
	}

	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTID, m_sAgentID, _T(""));
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_DEPTNAME, sDeptName, _T(""));
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTNAME, m_sUserName, _T(""));
	// 2011/03/03 by realhan CC 모듈 수정
	if (m_bIsLogEncoded == FALSE)
	{
 		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTID, m_sAgentID);
 		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTNAME, m_sUserName);
 		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_DEPTNAME, sDeptName);
	}

	sMacAddress = GetMACAddress();

	int nSize = m_sAgentID.GetLength()+sMacAddress.GetLength()+sizeof(TCHAR);
	pBuf = (TCHAR*)malloc(nSize*sizeof(TCHAR)+sizeof(TCHAR));

	int nLen = m_sAgentID.GetLength();
	_tmemcpy(pBuf, (LPCTSTR)m_sAgentID, nLen);
	_tmemcpy(pBuf+nLen, &chSeparator, 1);									nLen++;
	_tmemcpy(pBuf+nLen, (LPCTSTR)sMacAddress, sMacAddress.GetLength());	nLen+=sMacAddress.GetLength();

	WRITELOG(_T("서버측에 AgentID, MacAddress 보냄"));

	return TRUE;
}

// 서버에 Agent의 상태 변경을 요청한다.
void CClientManager::SendAgentStatus(CString sStatus)
{
	// 패킷 헤더와 본체를 만들어서 서버에 전달한다.
	WRITELOG(_T("[CClientManager::SendAgentStatus] SendAgentStatus Start"));
	UINT nSize = sStatus.GetLength() + m_sAgentID.GetLength() + sizeof(TCHAR) + sizeof(TCHAR);
	
	TCHAR *pBuf = (TCHAR*)malloc(nSize*sizeof(TCHAR) + sizeof(TCHAR));
	UINT nLen = 0;
	ZeroMemory(pBuf, nSize*sizeof(TCHAR)+sizeof(TCHAR));

	_sntprintf(pBuf, nSize*sizeof(TCHAR), _T("%s%c%s"), m_sAgentID, g_Separator, sStatus);

	CString sResp = _T(""), sCmd = _T(""), sServerURL = _T(""), sReq =pBuf;
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::SendAgentStatus] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);

	free(pBuf); pBuf = NULL;
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18004"));

	int ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sResp, m_bIsHttps);
	if(ret != 0)
	{
		WRITELOG(_T("[CClientManager::SendAgentStatus] SendAgentStatus::SendAndReceive 실패"));
		return;
	}
	WRITELOG(_T("[CClientManager::SendAgentStatus] SendAgentStatus End"));
}

// 서버에 Agent의 2차 상태(Locked, Paused)를 전송한다.
void CClientManager::SendAgentAdditionalStatus()
{
	int nLen = 3;
	TCHAR pBuf[8];
	_sntprintf(pBuf, 7, _T("%c%c%c"), m_bLocked?_T('T'):_T(' '), FIELD_SEPARATOR, m_bPaused?_T('T'):_T(' '));

	/*HHHHH
	Network_SendHeader(HANDSHAKING_REQUEST_CHANGE_STATUS_ADD, nLen);
	SendData(pBuf, nLen);
	*/

	WRITELOG(_T("서버측에 Agent 추가 상태 요청 보냄(%s)"), pBuf);
}

// NEW - 2009/01/12
// CommAgent가 서버에 처음 붙으면 사용자정보를 전달한다.
CString CClientManager::GetConnectReqString()
{
	// 패킷 헤더와 본체를 만들어서 서버에 전달한다.

	TCHAR chSeparator = FIELD_SEPARATOR;
	CString sPCName = GetPCName();
	CString sIPAddr = GetIPAddress();
	CString sLoginName = GetLoginName();
	CString sTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));

	// 2009/09/04 avilon
	// Vista 이상 OS에서는 권한 부족으로 인해 화면보호기와 로그인암호 적용 상태를 얻어 오지 못 한다.
	TCHAR lpBuf[MAX_PATH] = {0, };
	CString sScreenSaver = _T("") ,sLoginPW = _T("");
	if (m_bADUnLock == FALSE)
	{
		CBMSSupport Support;
		Support.GetSaverAndLoginPw(lpBuf, 10000, m_bUseMultiLan);
	}
	AfxExtractSubString(sScreenSaver, lpBuf, 0, _T(' '));
	AfxExtractSubString(sLoginPW, lpBuf, 1, _T(' '));

	CString sSharedFolderNames = GetSharedFolderNames();
	CString sWindowsVersionName = GetWindowsVersionNames();
	CString sVaccineName = GetVaccineNames();
	CString sRegVersion = GetAgentInfoValue(BMS_AGENTINFO_CVN);

	// by grman, 2008/02/26
	CString sBSInfo = _T("");
	sBSInfo.Format(_T("%d"), CheckTodayMiniDump());

	// by grman, 2008/10/06
	CString sAgentVersion = _T("3.0");
	// 2010.03.31 HANGEBS	하드디스크 시리얼
	CString sSubKey = GetIDESerial();

	CString sRet = _T("");
	sRet.Format(_T("%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c"),
		m_sAgentID, FIELD_SEPARATOR,
		sTime, FIELD_SEPARATOR,
		sPCName, FIELD_SEPARATOR,
		sIPAddr, FIELD_SEPARATOR,
		sLoginName, FIELD_SEPARATOR,
		sRegVersion, FIELD_SEPARATOR,
		sScreenSaver, FIELD_SEPARATOR,
		sLoginPW, FIELD_SEPARATOR,
		sSharedFolderNames, FIELD_SEPARATOR,
		sWindowsVersionName, FIELD_SEPARATOR,
		sVaccineName, FIELD_SEPARATOR,
		sBSInfo, FIELD_SEPARATOR,
		sAgentVersion, FIELD_SEPARATOR,
		m_sMACIPList, FIELD_SEPARATOR,
		sSubKey, FIELD_SEPARATOR,
		// // 2012.08.22 by realhan, 에이전트 시간대 전달.
		getTimeZoneBiasString(), FIELD_SEPARATOR
		);

	return sRet;
}

#define BMS_CERT_ERROR_FILE_NAME	_T("BMSCERTERR.TMP")
BOOL CClientManager::IsErrorCertificate()
{
	CString s_bmsdat_tmp_path = _T("");
	s_bmsdat_tmp_path.Format(_T("%stmp"), m_sMonitoringFolder);
	BOOL bRet = FileExists(s_bmsdat_tmp_path + _T("\\")BMS_CERT_ERROR_FILE_NAME);
	if (bRet == TRUE)
	{
		if (::DeleteFile(s_bmsdat_tmp_path + _T("\\")BMS_CERT_ERROR_FILE_NAME) == FALSE)
		{
			BOOL bDeleteFile = FALSE;
			WRITELOG(_T("[IsErrorCertificate] DeleteFile 실패 err(%d) 보완을 위해서 삭제를 재시도 한다."), s_bmsdat_tmp_path + _T("\\")BMS_CERT_ERROR_FILE_NAME);
			
			for (int i = 0; i < 3; i++)
			{
				Sleep(1000);
				if (::DeleteFile(s_bmsdat_tmp_path + _T("\\")BMS_CERT_ERROR_FILE_NAME) == TRUE)
				{
					WRITELOG(_T("[IsErrorCertificate] DeleteFile 성공"));
					bDeleteFile = TRUE;
				}
			}
			if (bDeleteFile == FALSE)
			{
				WRITELOG(_T("[IsErrorCertificate] DeleteFile 재시도도 실패 했다. err(%d)"), GetLastError());
			}
		}
	}
	WRITELOG(_T("[IsErrorCertificate] end ret(%s)"), (bRet) ? _T("TRUE") : _T("FALSE"));

	return bRet;
}

CString CClientManager::GetLogContentsToSaveCopy(CString sSaveCopy)
{
	if (m_bUseMultiLan == TRUE)
	{
		return MultiLanguageReadFile(sSaveCopy, CHARSET_UTF8);
	}
	else
	{
		return MultiLanguageReadFile(sSaveCopy, CHARSET_MULTIBYTE);
	}
}

// NEW - 2009/01/12
// 접속 후 처리
int CClientManager::ProcessConnect(CString sReceived)
{
	if (m_sAgentID == BM_AGENT_ID_SVRSMTP)		// 2008.11.18	HANGEBS		exchange server
	{
		return -2;
	}

	WRITELOG(_T("[CClientManager::ProcessConnect] - LoginHistoryID, Version, DownloadURL, sLocked, sPaused 받음 : %s"), sReceived);
	CString sInstallMode = _T(""), sServerVersion = _T(""), sDownURL = _T("");
	AfxExtractSubString(sInstallMode, sReceived, 0, FIELD_SEPARATOR);
	AfxExtractSubString(sServerVersion, sReceived, 1, FIELD_SEPARATOR);
	AfxExtractSubString(sDownURL, sReceived, 2, FIELD_SEPARATOR);
	m_server_upgrade_URL = sDownURL;

	CString sLocalVersion = _T("");
	sLocalVersion = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_CVN, _T(""));
	WRITELOG(_T("[CClientManager::ProcessConnect] - sLocalVersion : %s"), sLocalVersion);

	UINT nSVer, nCVer;
	nSVer = Str2Int(NumFilter(sServerVersion));
	nCVer = Str2Int(NumFilter(sLocalVersion));

	if (m_bUpgradePass == TRUE && m_dwRetryCount != 0)
	{
		WRITELOG(_T("[ProcessConnect] Upgrade Pass!! m_dwRetryCount : %d"), m_dwRetryCount);
	}
	// Version 비교
	else if((nSVer > nCVer) && (g_pBMSCommAgentDlg->m_sCmdLine.Find(_T("restart")) == -1))
	{
		WRITELOG(_T("[ProcessConnect] Upgrade 진행하러 옴..%s"), g_pBMSCommAgentDlg->m_sCmdLine);
		// 2010/01/26, avilon
		// Upgrade는 처음 한번만..
		if (m_bUpgrade == TRUE)
		{
			// 업그레이드 필요!!
			m_bUpgrade = FALSE;

			if (IsErrorCertificate() == FALSE)
			{
				CreateAgentUpgradeFile(nCVer, nSVer);
				AgentUpgrade();
				return BMS_AGENTUPGRADE;
			}
			else
			{
				WRITELOG(_T("[ProcessConnect] 업그레이드 상황이지만 인증서 오류 파일을 확인했다. 업그레이드를 진행하지 않는다."));
			}
		}
	}
	else if ((nSVer > nCVer) && (g_pBMSCommAgentDlg->m_sCmdLine.Find(_T("restart")) != -1))
	{
		WRITELOG(_T("[ProcessConnect] 어떠한 이유에서 Upgrade를 진행하지 않았다. nSVer:%d, nCver:%d, m_sCmdLine:%s"), nSVer, nCVer, g_pBMSCommAgentDlg->m_sCmdLine);
	}
	else
	{
		UpdateLook(4, 0);
		WRITELOG(_T("[ProcessConnect] nSVer:%d, nCver:%d, m_sCmdLine:%s"), nSVer, nCVer, g_pBMSCommAgentDlg->m_sCmdLine);
	}

	CString sAgentName = _T(""), sDeptName = _T(""), sLocked = _T(""), sPaused = _T(""), sInstanLevelID = _T("");
	CString sInstantLevelDue = _T(""), sSub = _T(""), sDateTime = _T(""), sTimeZone = _T(""), sTimeSyncSec = _T("");
	AfxExtractSubString(sAgentName, sReceived, 3, FIELD_SEPARATOR);
	AfxExtractSubString(sDeptName, sReceived, 4, FIELD_SEPARATOR);
	AfxExtractSubString(sPaused, sReceived, 5, FIELD_SEPARATOR);
	AfxExtractSubString(sLocked, sReceived, 6, FIELD_SEPARATOR);
	AfxExtractSubString(sInstanLevelID, sReceived, 7, FIELD_SEPARATOR);
	AfxExtractSubString(sInstantLevelDue, sReceived, 8, FIELD_SEPARATOR);
	AfxExtractSubString(sSub, sReceived, 9, FIELD_SEPARATOR);
	AfxExtractSubString(sDateTime, sReceived, 10, FIELD_SEPARATOR);
	AfxExtractSubString(sTimeZone, sReceived, 15, FIELD_SEPARATOR);
	AfxExtractSubString(sTimeSyncSec, sReceived, 18, FIELD_SEPARATOR);

	BOOL bIsInstant = !sSub.Compare(_T("1"));
	
	m_bLocked = (sLocked == _T("T")) ? TRUE:FALSE;
	m_bPaused = (sPaused == _T("T") && g_pBMSCommAgentDlg->m_bUserBase == FALSE) ? TRUE:FALSE;

	// 임시 정책이 존재하는 경우 타이머 가동
	int instant_level_id = Str2Int(sInstanLevelID);
	if (instant_level_id > 0)
	{
		g_pBMSCommAgentDlg->SetInstantLevelTimer(sInstantLevelDue);
	}

	if (m_bLocked == FALSE)
	{
		if (FileExistRegInfo() == TRUE)
		{
			WRITELOG(_T("[ProcessConnect] RegInfo 파일 발견. REGINFO_STRING_PCLOCKED"));
			g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_REGINFO_FILENAME, BMS_REGINFO_LOCKED, _T(""), _T(""));
		}
		else
		{
			RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, BMS_REGINFO_LOCKED, _T(""));
		}
	}

	CString sTempAgentName = _T(""), sTempDeptName = _T("");
	sTempAgentName = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTNAME, _T(""));
	sTempDeptName = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_DEPTNAME, _T(""));

	// 2012/03/09, avilon - 부서이동시 워터마크 출력을 위해 수정.
	WRITELOG(_T("## sTempAgentname : %s, sAgentName : %s"), sTempAgentName, sAgentName);
	if (sTempAgentName != sAgentName)
	{
		WRITELOG(_T("[CilentManager::ProcessConnect] Agent 이름 변경 OLD - %s, NEW - %s"), sTempAgentName, sAgentName);
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTNAME, sAgentName, _T(""));
	}
	WRITELOG(_T("## sTempDeptName : %s, sDeptName : %s"), sTempDeptName, sDeptName);
	if (sTempDeptName != sDeptName)
	{
		WRITELOG(_T("[CilentManager::ProcessConnect] 부서 이름 변경 OLD - %s, NEW - %s"), sTempDeptName, sDeptName);
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_DEPTNAME, sDeptName, _T(""));
	}

	if (g_pBMSCommAgentDlg->m_bUserBase == TRUE && g_pBMSCommAgentDlg->m_bUserBaseForSSO == FALSE)
	{
		CreateOfflinePolicy(BM_OFFLINE_POLICY_BLOCK_ALL_EXCEPT_WLAN);
		RequestUpdatePolicies(TRUE);
		ApplyPolicies();

		return BMS_OFFLINE_AGENT;
	}

	if (!RequestAllowList(m_sAgentID, TRUE))
	{
		if (bIsInstant == TRUE)
		{
			if (m_bGSALLOW_62 == TRUE)
			{
				UpdateInstantLevelEndDate();
				CreateInstantLevel();
			}
			else
			{
				StartInstantPolicies();
			}
			// 2011/09/23, avilon - GRADIUS 임시정책 적용 중이면 디스커버리는 다시 정책 요청한다.
			if (m_bUseDiscovery == TRUE)
			{
				RequestDiscoveryPolicy();
				//RequestEncryptKeyValue();
			}

			g_pBMSCommAgentDlg->SetTimer(ALLOW_INSTANTLEVEL_TIMER_ID, 1000*60, NULL);
		}
	}
	// 2009/10/12 avilon, 처음 시작시 목록을 한번 요청한다.
	/* RequestAllowList(m_sAgentID, TRUE); */
	RequestAllowList(m_sAgentID, FALSE);
	// 2011/11/18, avilon
	// 트레이가 실행 되어 있는 상태에서 컴에이전트가 재시작 될 때 정책을 내려 받기 전에 리프레쉬를 날리면 트레이는 정책이 존재하지 않아 종료 되는 현상 발생하여 막음.
	// 초기 접속시에는 트레이가 실행 되어 있지 않기 때문에 굳이 갱신 메시지를 날릴 필요가 없다.
	// SendReactTrayRefresh();

	if (m_bTimeSync)
	{
		sTimeSyncSec.Format(_T("%d"), m_nTimeSync_sec);

		if (sDateTime.GetLength() == 14 && (!_tcsicmp(sDateTime.Mid(0, 2), _T("20"))))
		{
			if (SetClientDateTime(sDateTime, sTimeSyncSec) == FALSE)
			{
				WRITELOG(_T("[ClientManager::LookServer]시간 동기화 실패!!"));
			}
		}
		else
		{
			WRITELOG(_T("[ClientManager::ProcessConnect]시간 형식이나 해당 일자가 잘못 되었습니다. DateTime : %s, Length : %d"), sDateTime, sDateTime.GetLength());
		}

// 서버의 타임존과 PC의 타임존을 비교하여 시간 계산
// 		if (sDatetime.GetLength() == 14 && (!_tcsicmp(sDatetime.Mid(0, 2), _T("20"))))
// 		{
// 			SetClientDateTime_new(sDatetime, sTimeZone, m_nTimeSync_sec);
// 		}
// 		else
// 		{
// 			WRITELOG(_T("[CClientManager::ProcessConnect]시간 형식이나 해당 일자가 잘못 되었습니다. DateTime : %s, Length : %d"), sDatetime, sDatetime.GetLength());
// 		}
	}
	else // 시간 동기화 정책으로 빠져서 메이저 버전 올릴때 삭제가 필요한 부분
	{
		// 2009/08/24 avilon, 서버 시간과 동기화
		if (sDateTime.GetLength() == 14 && (!_tcsicmp(sDateTime.Mid(0, 2), _T("20"))))
		{
			if (SetClientDateTime(sDateTime, sTimeSyncSec) == FALSE)
			{
				WRITELOG(_T("[CClientManager::ProcessConnect]시간 동기화 실패!!"));
			}
		}
		else
		{
			WRITELOG(_T("[CClientManager::ProcessConnect]시간 형식이나 해당 일자가 잘못 되었습니다. DateTime : %s, Length : %d"), sDateTime, sDateTime.GetLength());
		}
	}


	// 2012 /04/12 realhana, 대림산업 커스텀마이징 부서코드 데이터 추가. BMSPrintAgent 에서 읽어 워터마크로 처리한다.
	CString sDeptCode  = _T("");
	AfxExtractSubString(sDeptCode, sReceived, 14, FIELD_SEPARATOR);
	if ((FileExistRegInfo() == TRUE) && (sDeptCode.IsEmpty() == FALSE))
	{
		CAtlString sReginfoData = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_REGINFO_FILENAME, _T(""), BMS_ENVIRONMENT_FILE_COMMON_SECTION);
		if (sReginfoData.IsEmpty() == FALSE)
		{
			CAtlString sPrevDeptCode = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_REGINFO_FILENAME, BMS_REGINFO_DEPTCODE, _T(""));
			if (sPrevDeptCode.CompareNoCase(sDeptCode))
			{
				g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_REGINFO_FILENAME, BMS_REGINFO_DEPTCODE, sDeptCode, _T(""));
			}
		}
		else
		{
			WRITELOG(_T("[CilentManager::ProcessConnect] BMSReginfo.ini/ged 파일이 잘못됨. 복구 시켜준다."));

			CString sAgentTmpPath = _T(""), sAgentPath = _T("");
			sAgentTmpPath.Format(_T("%stmp"), g_pBMSCommAgentDlg->m_sBMSDATLogPath);
			sAgentPath.Format(_T("%s"), g_pBMSCommAgentDlg->m_sMainAgentPath);
			
			CopyFile(sAgentTmpPath + _T("\\BMSRegInfo.ini"), sAgentPath + _T("\\BMSRegInfo.ini"), FALSE);
			CopyFile(sAgentTmpPath + _T("\\BMSRegInfo.ged"), sAgentPath + _T("\\BMSRegInfo.ged"), FALSE);
		}

		if (m_bIsLogEncoded == FALSE)
		{
			RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REGINFO_STRING_DEPT_CODE, sDeptCode);
		}
		WRITELOG(_T("################## test3"));
	}
	
	//agentid, agentname을 넣어주자
	if (m_bIsLogEncoded == FALSE)
	{
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTID, m_sAgentID);
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTNAME, sAgentName);
	}

	if (ExdsList(EXDS_REQ_TYPE) == 0)
	{

	}
	ExdsList(EXDS_ALLOW_TYPE);

	return 0;
}

// 서버로부터 전달받은 에이전트 리스트를 출력한다.
void CClientManager::PrintAgentList(LPCTSTR pBuf)//CReceivedData *pReceivedData)
{
	CString sReceived = pBuf;
	WRITELOG(_T("[CClientManager::PrintAgentList] - 사용자 목록 받음 : %s"), sReceived);

	// 사용자 목록이 없는 경우
	if (sReceived == _T(""))
	{
		g_pBMSCommAgentDlg->m_pUserRegExDlg->PrintErrorMsg_NoUserName();
		return;
	}

	CString sLine = _T(""), sAgentID = _T(""), sDept = _T(""), sName = _T(""), sState = _T("");
	int i = 0;

	g_pBMSCommAgentDlg->m_pUserRegExDlg->ClearListCtrl();

	BOOL bSuccess = TRUE;
	CString sErrMsg = _T(""), sWidth = _T("");
	while (TRUE)
	{
		AfxExtractSubString(sLine, sReceived, i++, _T('|'));
		if(sLine.GetLength() == 0)
			break;

		AfxExtractSubString(sAgentID, sLine, 0, g_Separator);
		AfxExtractSubString(sDept, sLine, 1, g_Separator);
		AfxExtractSubString(sName, sLine, 2, g_Separator);
		AfxExtractSubString(sState, sLine, 3, g_Separator);
		WRITELOG(_T("[CClientManager::PrintAgentList] while1:%d(%s,%s,%s)"), i, sAgentID, sDept, sName);
		WRITELOG(_T("[CClientManager::PrintAgentList] while2:%d,%x"), i, g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl);

		CString tmpstr = _T(""), sAgentKey = _T("");
		tmpstr.Format(_T("%s:%s:%s"), sAgentID, sDept, sName);
		sAgentKey.Format(_T("%s@%s"), sDept, sName);
		m_map_AgentID.SetAt(sAgentKey, _ttoi(sAgentID));
		g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.InsertItem(i-1, sDept);
		g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.SetItem(i-1, 1, LVIF_TEXT, sName, NULL, NULL, NULL, NULL);
		g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.SetItem(i-1, 2, LVIF_TEXT, ChangeStatusText(sState), NULL, NULL, NULL, NULL);
		g_pBMSCommAgentDlg->m_pUserRegExDlg->m_ListControl.SetItemData(i-1, i-1);
		
		if (sWidth.GetLength() < tmpstr.GetLength())
		{
			sWidth = tmpstr;
		}
		WRITELOG(_T("[PrintAgentList] while6:%d"), i);
	}

	// 2013/09/30 by realhan, 좌우 스크롤바 지원.

	g_pBMSCommAgentDlg->m_pUserRegExDlg->SetVisibleControls(bSuccess);
	WRITELOG(_T("[CClientManager::PrintAgentList] 2"));
	if (bSuccess == FALSE)
	{
		MessageBox(NULL, sErrMsg, LoadLanString(196), MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL);
	}

}

//
BOOL CClientManager::GetInstallInfo(CReceivedData *pReceivedData, CString &sInstallMode, CString &sSVN, CString &sInstallURL)
{
	// CString으로 옮겨담는다.
	TCHAR *pBuf = (TCHAR*)malloc(pReceivedData->m_nReceivedDataLength*sizeof(TCHAR)+sizeof(TCHAR));
	ZeroMemory(pBuf, pReceivedData->m_nReceivedDataLength*sizeof(TCHAR)+sizeof(TCHAR));
	_tmemcpy(pBuf, pReceivedData->m_pData, pReceivedData->m_nReceivedDataLength*sizeof(TCHAR));
	CString sReceived = pBuf;
	free(pBuf);

	WRITELOG(_T("[CClientManager::GetInstallInfo] %s"), sReceived);

	AfxExtractSubString(sInstallMode, sReceived, 0, FIELD_SEPARATOR);
	AfxExtractSubString(sSVN, sReceived, 1, FIELD_SEPARATOR);
	AfxExtractSubString(sInstallURL, sReceived, 2, FIELD_SEPARATOR);

	return TRUE;
}

// 정책을 요청한다.
void CClientManager::RequestPolicies(BOOL bRequestResetInstantLevel, BOOL bInstantLevelCancel)
{
	EnterCriticalSection(&m_RequestPoliciesCS);
	m_sDLPPolicyTime = _T("");
	if (bRequestResetInstantLevel == TRUE)
	{
		// 임시 정책이 만료되어 정책을 요청하는 경우이다.
		// 이 때는 RESET을 먼저 요청한 후 정책을 요청해야 한다.
		WRITELOG(_T("서버측에 임시 정책 해제를 요청 flag를 추가함"));
	}

	// 2011/06/02, avilon
	// 정책 재요청 전에 임시정책 사용중인지 판단한다.
	if (m_bIsCurrentUsingInstantLevel == TRUE && bInstantLevelCancel == FALSE)
	{
		WRITELOG(_T("[CClientManager::RequestPolicies] 현재 임시정책 사용 중 이므로 임시정책을 요청한다."));
		if (m_bGSALLOW_62 == TRUE)
		{
			UpdateInstantLevelEndDate();
			CreateInstantLevel();
		}
		else
		{
			StartInstantPolicies();
		}

		LeaveCriticalSection(&m_RequestPoliciesCS);
		return;
	}

	WRITELOG(_T("[CClientManager::RequestPolicies] Start"));

	CString sReq = _T("");
	sReq.Format(_T("%c%c%s"), bRequestResetInstantLevel?_T('1'):_T('0'), FIELD_SEPARATOR, m_sAgentID);

	CString sUserId = ReadUserInfoFile(USERINFO_LOGINID);
	if (sUserId.IsEmpty() == FALSE)
	{
		sReq += (CString)((TCHAR)FIELD_SEPARATOR + (CString)sUserId);
	}

	m_bPolicyReceived = FALSE;
	CString sReceived = _T(""), sServerURL = _T(""), sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::RequestPolicies] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);

	CString sCmd = _T("");
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18001"));

	int nRet = 0;
	CStringA sAReceived = "";
	CString sHashCmd = _T("");
	sHashCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18007")); //해쉬값 요청
	DWORD dwCheckCount = 0;
	BOOL bCompareHash = FALSE;
	DWORD dwStartPos = 0, dwLastPos = 0;
	DWORD dwPolicyEndPos = 0;
	while (dwCheckCount < POLICY_INTEGRITY_CHECK_COUNT)
	{
		sReceived.Empty();
		sAReceived.Empty();
		nRet = SendAndReceiveUTF8((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, sAReceived, m_bIsHttps);
		WRITELOG(_T("[CClientManager] RequestPolicies sReceived = %s"), sReceived);

		if (m_bIsLogEncoded == TRUE)
		{
			CString sHashedServerPolicy = _T("");
			int nHashRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sHashCmd, m_sAgentID, sHashedServerPolicy, m_bIsHttps);
			if (nHashRet == 0)
			{				
				if (sHashedServerPolicy != _T(""))
				{
					dwCheckCount ++;
					sAReceived.Replace("\x0A", "\x0D\x0A");
					sAReceived += "\x0D\x0A\x0D\x0A";

					bCompareHash = CheckPolicyIntegrity(sAReceived, sHashedServerPolicy);
					if (bCompareHash == TRUE)
					{
						WRITELOG(_T("[RequestPolicies] CompareHashData TRUE"));
						m_sHashedPolicy = sHashedServerPolicy;
						SavePolicyIntegrityFile(sAReceived);
						break;
					}
					else 
					{
						WRITELOG(_T("[RequestPolicies] CompareHashData FALSE 재요청함 count = %d"), dwCheckCount);
					}
				}
				else
				{
					WRITELOG(_T("[RequestPolicies] Server로 부터 받은 hash값이 없음(%s Error) - 기존 루틴"), sHashedServerPolicy);
					dwCheckCount ++;
				}
			}
			else
			{
				WRITELOG(_T("[RequestPolicies] Server로 부터 받은 hash값이 없음(%d Error) - 기존 루틴"), nHashRet);
				break;
			}
		}
		else
		{
			WRITELOG(_T("[RequestPolicies] CC버전이 아니므로 해쉬값을 요청하지 않는다!!"));
			break;
		}
		
	}
	if (dwCheckCount >= 1 && m_bIsLogEncoded == TRUE)
	{
		SendPolicyIntegrityResultToServer(bCompareHash);
	}
	if (nRet == 0 && (sReceived.GetLength() > 0))
	{


		BOOL bValidPolicy = TRUE;
		//	sReceived.Empty();
		if (!DeleteDuplicatePolicy(sReceived))
		{
			bValidPolicy = FALSE;
			if (!RestorePolicyFileOnPolicyError(sReceived))
			{
				//_T("정책 생성 오류로 관리자에게 문의하시기 바랍니다.")
				MessageBox(NULL,LoadLanString(213), LoadLanString(212), MB_ICONWARNING | MB_SERVICE_NOTIFICATION);
				LeaveCriticalSection(&m_RequestPoliciesCS);
				return;
			}
		}
		m_policy_update_time = _T("");

		if (m_OSVerMajor >= 6)
		{
			TCHAR tempBuf[MAX_PATH] = {0, };
			GetCSIDLDirectory(tempBuf, CSIDL_PROFILE);
			CString sUserDir = tempBuf;
			DWORD dwPos = sUserDir.ReverseFind(_T('\\'));
			sUserDir = sUserDir.Left(dwPos + 1);
			sUserDir += m_sSupportName;
			_sntprintf(tempBuf, MAX_PATH-1, _T("%s\\AppData\\Local\\VirtualStore\\Windows\\System32\\"), sUserDir);
			WRITELOG(_T("[CClientManager] Remove dir dir path : %s"), tempBuf);
			TCHAR VirbmsdatPath[MAX_PATH], VirbmsbinPath[MAX_PATH];
			_sntprintf(VirbmsbinPath, MAX_PATH-sizeof(TCHAR), _T("%sbmsbin\\"),tempBuf);
			_sntprintf(VirbmsdatPath, MAX_PATH-sizeof(TCHAR), _T("%sbmsdat\\"),tempBuf);
			RemoveAllDir(VirbmsbinPath);
			RemoveAllDir(VirbmsdatPath);
		}
		FILE *fp	= NULL;
		TCHAR *data_ptr	= NULL;

		if (sReceived.GetLength() == 0)
		{
			WRITELOG(_T("CBMSCommAgentDlg::GetPolicies - 정책을 받지 못함"));
			LeaveCriticalSection(&m_RequestPoliciesCS);
			return;
		}

		if (sReceived.Find(_T("Level")) == -1 && sReceived.Find(_T("RegulationList")) == -1)
		{
			WRITELOG(_T("CBMSCommAgentDlg::GetPolicies - 정상적인 정책이 아니다 sReceived : %s"), sReceived);
			LeaveCriticalSection(&m_RequestPoliciesCS);
			return;
		}

		m_bPolicyReceived = TRUE;
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sReceived, _T(""));

		//2018.01.29 kjh4789 SSO 연동 Offlinie 정책 생성
		if (g_pBMSCommAgentDlg->m_bUserBaseForSSO == TRUE)
		{
			CString sSSOOfflinePolicy = _T("");
			sSSOOfflinePolicy.Format(_T("%stmp\\%s"), g_pBMSCommAgentDlg->m_sBMSDATLogPath, BMS_SSO_OFFLINE_POLICY_FILE);
			if (MultiLanguageWriteFile(sSSOOfflinePolicy, sReceived, CHARSET_UTF8) == FALSE)
			{
				WRITELOG(_T("[CClientManager::RequestPolicies] MultiLanguageWriteFile Fail : %d"), GetLastError());
			}
		}
		else if (sUserId.IsEmpty() == FALSE)
		{
			//2017.06.16 kjh4789 공용 PC Offline 정책 생성
			CString sPCID = _T("");
			sPCID = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTID, _T(""));
			WRITELOG(_T("[CClientManager] 공용 PC Offline 정책을 위해 PC ID의 정책을 확인한다."));
			CString sPCIDReq = _T("");
			sPCIDReq.Format(_T("%c%c%s"), bRequestResetInstantLevel?_T('1'):_T('0'), FIELD_SEPARATOR, sPCID);

			CString sPublicPCOfflinePolicy = _T("");
			sAReceived.Empty();
			nRet = SendAndReceiveUTF8((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sPCIDReq, sPublicPCOfflinePolicy, sAReceived, m_bIsHttps);
			WRITELOG(_T("[CClientManager] 공용 PC Offline 정책 sPublicPCOfflinePolicy = %s"), sPublicPCOfflinePolicy);


			CString sUserBaseOfflinePolicy = _T("");
			TCHAR lpLogPath[MAX_PATH] = {0, };
			GetAgentLogPath(lpLogPath, MAX_PATH);
			sUserBaseOfflinePolicy.Format(_T("%s%s\\UserBaseOfflinePolicy"), lpLogPath, BM_RDATA);
			if (MultiLanguageWriteFile(sUserBaseOfflinePolicy, sPublicPCOfflinePolicy, CHARSET_UTF8) == FALSE)
			{
				WRITELOG(_T("[CClientManager::RequestPolicies] MultiLanguageWriteFile Fail : %d"), GetLastError());
			}
		}

		//2020.02.20 kjh4789 공용 PC OTP 정책 생성
		if (g_pBMSCommAgentDlg->m_bUserBase)
		{
			CString sCmd = _T(""), sOTPPolicyResp = _T(""), sPolicyData = _T("");
			sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18506"));

			//sReceived.Empty();
			sAReceived.Empty();
			nRet = SendAndReceiveUTF8((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, _T(""), sOTPPolicyResp, sAReceived, m_bIsHttps);

			if (nRet == 0 && sOTPPolicyResp.IsEmpty() == FALSE)
			{
				CString sOTPPolicyPath = _T("");
				TCHAR lpLogPath[MAX_PATH] = {0, };
				GetAgentLogPath(lpLogPath, MAX_PATH);
				sOTPPolicyPath.Format(_T("%s%s\\UserBaseOTPPolicy"), lpLogPath, BM_RDATA);

				if (sOTPPolicyResp == _T("0"))
				{
					sPolicyData = sReceived;
				}
				else if (sOTPPolicyResp == _T("1"))
				{
					LoadFileFromRes(sOTPPolicyPath, _T("allow_utf8.xml"));
					sPolicyData = MultiLanguageReadFile(sOTPPolicyPath + _T("allow_utf8.xml"), CHARSET_UTF8);
					::DeleteFile(sOTPPolicyPath + _T("allow_utf8.xml"));

				}
				else if (sOTPPolicyResp == _T("2"))
				{
					LoadFileFromRes(sOTPPolicyPath, _T("block_utf8.xml"));
					sPolicyData = MultiLanguageReadFile(sOTPPolicyPath + _T("block_utf8.xml"), CHARSET_UTF8);
					::DeleteFile(sOTPPolicyPath + _T("block_utf8.xml"));
				}
				else
				{
					sPolicyData = sOTPPolicyResp;
				}

				//Create OTP Policy
				if (MultiLanguageWriteFile(sOTPPolicyPath, sPolicyData, CHARSET_UTF8))
				{
					WRITELOG(_T("[CClientManager::RequestPolicies] OTP Policy Create Success : %s\r\n%s"), sOTPPolicyPath, sPolicyData);
				}
			}
		}

		CString sFilePath = _T(""), OriginPolicy = _T("");
		TCHAR lpLogPath[MAX_PATH] = {0, };
		GetAgentLogPath(lpLogPath, MAX_PATH);
		OriginPolicy.Format(_T("%s%s\\OriginPolicy"), lpLogPath, BM_RDATA);
		if (MultiLanguageWriteFile(OriginPolicy, sReceived, CHARSET_UTF8) == FALSE)
		{
			WRITELOG(_T("[CClientManager::RequestPolicies] MultiLanguageWriteFile Fail : %d"), GetLastError());
		}

		m_sDLPPolicyTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));

		WRITELOG(_T("[RequestPolicies] 정책 파일 생성 성공, GetLastError(%d)"), GetLastError());

		// Agent들에게 알림
		RequestUpdatePolicies();

		// 2009/08/13 by avilon, 사본 확장자 제외 목록
		CGRegulationManager reg_man;
		if (reg_man.Read() < 0)
		{
			WRITELOG(_T("CBMSCommAgentDlg::GetPolicies ERROR! - GRegulationManager Read Fail!!"));
		}

		m_sExcepExpan = reg_man.getGlobalString(_T("copysave_ex_ext_list"));
		m_sExcepExpan = Make_lower(m_sExcepExpan);

		m_OfflinePolicy = reg_man.getGlobalValue(_T("offlinepolicy"));
		m_nOfflineLevelId = reg_man.getGlobalValue(_T("offlinepolicy_levelid"));
		WRITELOG(_T("[CClientManager::RequestPolicies()] OfflinePolicy : %d, offlinePolicyLevel : %d"), m_OfflinePolicy, m_nOfflineLevelId);
		
		CString sOfflinePolicy = _T("");
		sOfflinePolicy.Format(_T("%d"), m_OfflinePolicy);
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_OFFLINEPOLICY, sOfflinePolicy, _T(""));

		m_bTimeSync = reg_man.getGlobalValue(_T("timesync"));
		m_nTimeSync_sec = reg_man.getGlobalValue(_T("timesync_sec"));

		if (m_OfflinePolicy == BM_OFFLINE_POLICY_SELECT_LEVEL)
		{
			RequestSelectedOfflinePolicy();
		}

		if (!bValidPolicy)
		{
			//_T("변경된 정책의 오류로 이전 정책을 적용합니다.\r\n원활한 사용을 위해 관리자에게 문의하시기 바랍니다.")
			MessageBox(NULL, LoadLanString(214), LoadLanString(212), MB_ICONWARNING | MB_SERVICE_NOTIFICATION);
		}
		//////////////////////////////////////////////////////////////////////////avilon - End
	}
	else
	{
		WRITELOG(_T("[CClientManager] RequestPolicies 정책 수신 실패. 오프라인 정책 설정."));
		// 정책 백업..
		BackupPolicyFile();
		CreateOfflinePolicy(m_OfflinePolicy);
		RequestUpdatePolicies(TRUE);
		ApplyPolicies();
	}

	LeaveCriticalSection(&m_RequestPoliciesCS);

	WRITELOG(_T("[CClientManager] RequestPolicies End"));
}

// 2011/09/22, avilon
void CClientManager::RequestDiscoveryPolicy()
{
	m_sDISPolicyTime = _T("");

	CString sCmd, sReceived = _T(""), policy_enc_data=_T("");
	CString sReq = m_sAgentID;

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::RequestDiscoveryPolicy()] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);

	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("19001"));
	int nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, m_bIsHttps);

	if (sReceived.GetLength() == 0)
	{
		WRITELOG(_T("CBMSCommAgentDlg::RequestDiscoveryPolicy - 정책을 받지 못함"));
		return;
	}

	if (sReceived.Find(_T("Level")) == -1 && sReceived.Find(_T("RegulationList")) == -1)
	{
		WRITELOG(_T("CBMSCommAgentDlg::RequestDiscoveryPolicy - 정상적인 정책이 아니다 sReceived : %s"), sReceived);
		return;
	}
	
	if (!nRet && (sReceived.GetLength() > 0))
	{
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DISCOVERY_POLICY_FILENAME, sReceived, _T(""));
		m_sDISPolicyTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));
	}
	
	HWND hWnd = ::FindWindow(_T("#32770"), BMDC_MAIN_UI_WND_TITLE);
	if (hWnd)
	{
		WRITELOG(_T("[CClientManager::RequestDiscoveryPolicy] - 디스커버리 정책 업데이트 알림."));
		::PostMessage(hWnd, BMDC_UPDATE_POLICY, NULL, NULL);
	}
	
	hWnd = ::FindWindow(_T("#32770"), BMDC_DOC_CHECKER_WND_TITLE);
	if (hWnd)
	{
		WRITELOG(_T("[CBMSCommAgentDlg::RequestDiscoveryPolicy] - 디스커버리 정책 업데이트 알림.(ENG)"));
		::PostMessage(hWnd, BMDC_UPDATE_POLICY, NULL, NULL);
	}
}


// 2015/03/16, keede122
void CClientManager::RequestAssetPolicies()
{
	WRITELOG(_T("[CClientManager::RequestAssetPolicies] 자산관리 정책 요청"));
	m_sASSPolicyTime=_T("");

	CString sCmd, sReceived = _T(""), policy_enc_data=_T("");
	CString sReq = m_sAgentID;

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::RequestAssetPolicies()] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);

	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("20001"));
	int nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, m_bIsHttps);

	if (sReceived.GetLength() == 0)
	{
		WRITELOG(_T("CBMSCommAgentDlg::RequestAssetPolicies - 정책을 받지 못함"));
		return;
	}

	if (sReceived.Find(_T("Level")) == -1 && sReceived.Find(_T("RegulationList")) == -1)
	{
		WRITELOG(_T("CBMSCommAgentDlg::RequestAssetPolicies - 정상적인 정책이 아니다 sReceived : %s"), sReceived);
		return;
	}

	if (!nRet && (sReceived.GetLength() > 0))
	{
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_ASSET_POLICY_FILENAME, sReceived, _T(""));
		m_sASSPolicyTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));
	}
	WRITELOG(_T("[RequestAssetPolicies] 자산관리 정책 파일 생성 성공, GetLastError(%d)"), GetLastError());

	POSITION pos = m_prcList.GetHeadPosition();
	while (pos)
	{
		PPRCLIST prc = (PPRCLIST)m_prcList.GetAt(pos);
		if (prc->sAgentProcess == BM_ASSETMAN_PROCESS_NAME || prc->sAgentProcess == BM_ASSETMAN_x64_PROCESS_NAME)
		{
			HWND hWnd = ::FindWindow(_T("#32770"), prc->sAgentWnds);
			if(hWnd != NULL)
			{
				WRITELOG(_T("[CClientManager::RequestAssetPolicies] - 자산관리 정책 업데이트 알림."));
				::PostMessage(hWnd, BMSM_UPDATE_ASSET_POLICY, (WPARAM)NULL, (LPARAM)NULL);
				ProcMessage(); 
			}
			else
			{
				hWnd = ::FindWindow(NULL, prc->sAgentWnds);
				if(hWnd != NULL)
				{
					WRITELOG(_T("[CClientManager::RequestAssetPolicies] - 자산관리 정책 업데이트 알림."));
					::PostMessage(hWnd, BMSM_UPDATE_ASSET_POLICY, (WPARAM)NULL, (LPARAM)NULL);
					ProcMessage();
				}
			}
		}
		m_prcList.GetNext(pos);
	}
}



// 2013/09/05, blueturban
BOOL CClientManager::WindowsUpdateForce()
{
	BOOL bIsWow64 = FALSE;
	IsWow64Process(GetCurrentProcess(), &bIsWow64);
	HWND hWnd = NULL;
	if (bIsWow64 == TRUE)
	{
		hWnd = ::FindWindow(_T("#32770"), BM_APPAGENT_x64_WND_TITLE);
	}
	else
	{
		hWnd = ::FindWindow(_T("#32770"), BM_APPAGENT_WND_TITLE);
	}
	if (hWnd)
	{
		if (::SendMessage(hWnd, BMSM_WIN_UPDATE_FORCE, NULL, NULL))
		{
			return TRUE;
		}
	}
	return FALSE;
}

// 2012/03/23, avilon
void CClientManager::RequestPauseDiscovery()
{
	TCHAR szAgentPath[MAX_PATH] = {0,};
	GetAgentPath(szAgentPath, MAX_PATH);

	CString sPolicyPath = szAgentPath;
	sPolicyPath += BM_DISCOVERY_POLICY_GED_FILENAME;

	if (FileExists(sPolicyPath) == TRUE)
	{
		::DeleteFile(sPolicyPath);
	}

	CString strTmpPolicy = _T("allow_dspolicy.ged");
	LoadFileFromRes(szAgentPath, strTmpPolicy);

	sPolicyPath = szAgentPath;
	sPolicyPath += strTmpPolicy;

	if (FileExists(sPolicyPath) == TRUE)
	{
		CString sNewName;
		sNewName.Format(_T("%s%s"), szAgentPath, BM_DISCOVERY_POLICY_GED_FILENAME);
		_trename(sPolicyPath, sNewName);
	}

	HWND hWnd = ::FindWindow(_T("#32770"), BMDC_DOC_CHECKER_WND_TITLE);
	if (hWnd)
	{
		WRITELOG(_T("CClientManager::RequestPauseDiscovery - 디스커버리 정책 업데이트 알림."));
		::PostMessage(hWnd, BMDC_UPDATE_POLICY, NULL, NULL);
	}
}

// 에이전트를 중지한다.
void CClientManager::AgentStop()
{
	TCHAR szBuf[1024] = {0, }, szSysBuf[MAX_PATH] = {0, };
	GetAgentPath(szBuf);
	GetWow64DirBeforeGetSysDirectory(szSysBuf, MAX_PATH);
	CString sAgentPath = szBuf, sSysPath = _T(""), sExeutePAth = _T("");

	sAgentPath += INSTALL_AGENT_FILE;
	sSysPath.Format(_T("%s\\%s"), szSysBuf, INSTALL_AGENT_FILE);

	sExeutePAth = (FileExists(sAgentPath)) ? sAgentPath : sSysPath;

	ShellExecute(NULL, _T("open"), sExeutePAth, INSTALL_AGENT_STOP_PARAM, NULL, SW_SHOW);
}

// 에이전트를 제거한다.
void CClientManager::AgentRemove(CString sReason)
{
	SendAgentStatus(_T("U"));
	TCHAR szBuf[1024], szSysBuf[MAX_PATH];
	GetAgentPath(szBuf);
	GetWow64DirBeforeGetSysDirectory(szSysBuf, MAX_PATH);
	CString sAgentPath = szBuf, sAgentRemoveParam = szBuf, sSysPath = _T(""), sExeutePAth = _T(""), sCmd = _T("");

	sAgentPath += INSTALL_AGENT_FILE;
	sAgentRemoveParam += _T("AgentRemoveParam");
	sSysPath.Format(_T("%s\\%s"), szSysBuf, INSTALL_AGENT_FILE);

	sExeutePAth = (FileExists(sAgentPath) == TRUE) ? sAgentPath : sSysPath;

	//sCmd.Format(_T("%s%c%s"), INSTALL_AGENT_REMOVE_FROM_REASON, (TCHAR)FIELD_SEPARATOR, sReason);

	MultiLanguageWriteFile(sAgentRemoveParam, sReason, CHARSET_UTF8);

	WRITELOG(_T("[AgentRemove] sExecutePAth : %s, sCmd : %s"), sExeutePAth, sCmd);
	ShellExecute(NULL, _T("open"), sExeutePAth, INSTALL_AGENT_REMOVE_PARAM, NULL, SW_SHOW);
}

// by grman, 2008/07/01
// 에이전트 일시 중지
void CClientManager::AgentPause()
{
	WRITELOG(_T("################# AgentPause() 로그 남기기"));
	// 전체 허용 정책을 강제로 적용하여 모든 Agent에 전달한다.
	if (IsUserBase() == TRUE)
	{
		WRITELOG(_T("################# 공용 PC인 경우에는 중지하지 않는다."));
		return;
	}
	m_bPaused = TRUE;
	RequestUpdatePolicies();

	// 상태를 서버에 전달한다.
	SendAgentAdditionalStatus();
}

// by grman, 2008/07/01
// 에이전트 다시 시작
void CClientManager::AgentResume()
{
	m_bPaused = FALSE;

	RequestPolicies();
	SendAgentAdditionalStatus();
}

// 에이전트를 업그레이드한다.
void CClientManager::AgentUpgrade()
{
	if (g_pBMSCommAgentDlg->m_bIsMainSession == TRUE && g_pBMSCommAgentDlg->m_hMailSlot != INVALID_HANDLE_VALUE)
	{
		WriteDataToAllSessionMailSlot(g_pBMSCommAgentDlg->m_dwSessionId, BM_COMMAGENT_UPGRADE);
	}
	WRITELOG(_T("[CClientManager] AgentUpgrade Start"));

	// 2008.11.18	HANGEBS	exchange server mode
	if (m_sAgentID == BM_AGENT_ID_SVRSMTP)
	{
		return;
	}

	if (m_bMultiURL)
	{
		CString sServerURL = _T("");
		int nEPos = m_stPageInfo.m_GSHS.Find(_T("server/"));
		if (nEPos > -1)
		{
			m_server_upgrade_URL = m_stPageInfo.m_GSHS.Left(nEPos) + _T("module/ver.txt");
			WRITELOG(_T("[RegistAgentID] Multi URL upgrade URL : %s"), m_server_upgrade_URL);
		}
	}

	CString sParam = _T("");
	if (g_pBMSCommAgentDlg->m_bIsMainSession == TRUE)
	{
		sParam = _T("I ");
		sParam += m_server_upgrade_URL;
	}
	else
	{
		sParam.Format(_T("%s"), INSTALL_SESSION_UPGRADE);
	}
	WRITELOG(_T("[AgentUpgrade] sParam = %s"), sParam);
	TCHAR szBuf[1024] = {0, }, szSysBuf[MAX_PATH] = {0, };
	GetAgentPath(szBuf);
	GetWow64DirBeforeGetSysDirectory(szSysBuf, MAX_PATH);
	CString sAgentPath = szBuf, sSysPath = _T(""), sExeutePAth = _T("");
	sAgentPath += INSTALL_AGENT_FILE;
	sSysPath.Format(_T("%s\\%s"), szSysBuf, INSTALL_AGENT_FILE);

	if (FileExists(sAgentPath) == FALSE)
	{
		GetAgentLogPath(szBuf);
		CString strTempPath = szBuf;
		strTempPath = strTempPath + _T("tmp\\") + INSTALL_AGENT_FILE;

		BOOL bRet = CopyFile(strTempPath, sAgentPath, FALSE);
		if (bRet == TRUE)
		{
			WRITELOG(_T("Upgrade 하기전 InstallAgent를 copy했다:%d"), bRet);
		}
		else
		{
			WRITELOG(_T("Upgrade 하기전 InstallAgent의 copy실패!!:%d"), bRet);
		}
	}

	sExeutePAth = (FileExists(sAgentPath)) ? sAgentPath : sSysPath;
	HINSTANCE hError = ShellExecute(NULL, _T("open"), sExeutePAth, sParam, NULL, SW_SHOW);

	WRITELOG(_T("[CClientManager] AgentUpgrade End - 0x%x"), hError);
}

void CClientManager::CreateAgentUpgradeFile(UINT uCVer, UINT uSVer)
{
	if (g_pBMSCommAgentDlg->m_bUserBase == TRUE)
	{
		WRITELOG(_T("[CreateAgentUpgradeFile] Start!!"));
		CString sFilePath = _T(""), sFileData = _T("");
		TCHAR szBuf[1024] = {0, }, szSysBuf[MAX_PATH] = {0, };
		GetAgentPath(szBuf, MAX_PATH);
		WRITELOG(_T("[CreateAgentUpgradeFile] AgentPath : %s"), szBuf);
		WRITELOG(_T("[CreateAgentUpgradeFile] test11"));
		sFilePath.Format(_T("%s%s"), szBuf, _T("AgentUpgrade"));
		sFileData.Format(_T("%d%c%d"), uCVer, FIELD_SEPARATOR, uSVer);
		MultiLanguageWriteFile(sFilePath, sFileData, CHARSET_UTF8);
		WRITELOG(_T("[CreateAgentUpgradeFile] End!! sFilePath - %s, sFileData - %s"), sFilePath, sFileData);
	}
}

void CClientManager::AgentRestart()
{
	WRITELOG(_T("[CClientManager] AgentRestart Start"));

	if (m_sAgentID == BM_AGENT_ID_SVRSMTP)
	{
		return;
	}

	CString sParam = _T("");
	if (g_pBMSCommAgentDlg->m_bIsMainSession == TRUE)
	{
		sParam = INSTALL_AGENT_RESTART;
	}
	else
	{
		return;
	}
	WRITELOG(_T("[AgentRestart] sParam = %s"), sParam);

	TCHAR szBuf[1024] = {0, }, szSysBuf[MAX_PATH] = {0, };
	GetAgentPath(szBuf);
	GetWow64DirBeforeGetSysDirectory(szSysBuf, MAX_PATH);
	CString sAgentPath = szBuf, sSysPath = _T(""), sExeutePAth = _T("");
	sAgentPath += INSTALL_AGENT_FILE;
	sSysPath.Format(_T("%s\\%s"), szSysBuf, INSTALL_AGENT_FILE);

	if (FileExists(sAgentPath) == FALSE)
	{
		GetAgentLogPath(szBuf);
		CString strTempPath = szBuf;
		strTempPath = strTempPath + _T("tmp\\") + INSTALL_AGENT_FILE;

		BOOL bRet = CopyFile(strTempPath, sAgentPath, FALSE);
		if (bRet == TRUE)
		{
			WRITELOG(_T("Restart 하기전 InstallAgent를 copy했다:%d"), bRet);
		}
		else
		{
			WRITELOG(_T("Restart 하기전 InstallAgent의 copy실패!!:%d"), bRet);
		}
	}

	sExeutePAth = (FileExists(sAgentPath)) ? sAgentPath : sSysPath;
	ShellExecute(NULL, _T("open"), sExeutePAth, sParam, NULL, SW_SHOW);

	WRITELOG(_T("[CClientManager] AgentRestart End"));
}

// by grman, 2008/09/03
// 임시 정책을 저장하고 타이머를 기동한다.
void CClientManager::SetInstantLevelInfo(CReceivedData *pReceivedData)
{
	// CString으로 옮겨담는다.
	TCHAR *pBuf = (TCHAR*)malloc(pReceivedData->m_nReceivedDataLength*sizeof(TCHAR)+sizeof(TCHAR));
	ZeroMemory(pBuf, pReceivedData->m_nReceivedDataLength*sizeof(TCHAR)+sizeof(TCHAR));
	_tmemcpy(pBuf, pReceivedData->m_pData, pReceivedData->m_nReceivedDataLength*sizeof(TCHAR));
	CString sReceived = pBuf;
	free(pBuf);

	WRITELOG(_T("### SetInstantLevelInfo : %s"), sReceived);

	CString sDueDT;
	AfxExtractSubString(sDueDT, sReceived, 1, FIELD_SEPARATOR);

	g_pBMSCommAgentDlg->SetInstantLevelTimer(sDueDT);
}

CString CClientManager::GetIPAddress()
{
	CHAR localIP[64];
	CHAR chName[255];
	PHOSTENT pHostEntry;
	IN_ADDR inAddr;

	if (gethostname( chName, 255 ) != 0 )
	{        
		return _T("");
	}
	else
	{    
		if ((pHostEntry = gethostbyname( chName ) ) == NULL)
		{
			return _T("");
		}
		else
		{
			memcpy( &inAddr, pHostEntry->h_addr, 4 );
			//sprintf( localIP, "%s", inet_ntoa( inAddr ) );
			_snprintf(localIP, _countof(localIP)-1, "%s", inet_ntoa( inAddr ));
			USES_CONVERSION;
			return CString(A2W(localIP));
		}
	}
	return _T("");
}


CString CClientManager::GetMACAddress()
{
	PIP_ADAPTER_INFO Info;
	DWORD size;
	int result;
	CString Mac;

	ZeroMemory(&Info, sizeof(PIP_ADAPTER_INFO));
	size = sizeof( PIP_ADAPTER_INFO );

	result = GetAdaptersInfo( Info, &size);

	if( result == ERROR_BUFFER_OVERFLOW)    // GetAdaptersInfo가 메모리가 부족하면 재 할당하고 재호출
	{
		Info = (PIP_ADAPTER_INFO)malloc(size);

		GetAdaptersInfo( Info, &size);
	}

	Mac.Format(_T("%0.2x:%0.2x:%0.2x:%0.2x:%0.2x:%0.2x") ,Info->Address[0],
														Info->Address[1],
														Info->Address[2],
														Info->Address[3],
														Info->Address[4],
														Info->Address[5]);
	return Mac;
}

CString CClientManager::GetScrsaver()
{
	TCHAR lpBuf[100];
	ZeroMemory(lpBuf, 100);
	RegGetStringValue(HKEY_CURRENT_USER, REG_PATH_DESKTOP, REG_STRING_SCREENSAVEACTIVE, lpBuf, 100);
	CString sBuf = lpBuf;
	if(sBuf == _T("1"))
	{
		ZeroMemory(lpBuf, 100);
		RegGetStringValue(HKEY_CURRENT_USER, REG_PATH_DESKTOP, REG_STRING_SCREENSAVEISSECURE, lpBuf, 100);
		CString sBuf = lpBuf;
		if(sBuf == _T("1"))
		{
			return _T("T");
		}
	}
	else// if(sBuf==_T(""))
	{
		ZeroMemory(lpBuf, 100);
		RegGetStringValue(HKEY_CURRENT_USER, _T("Software\\Policies\\Microsoft\\Windows\\Control Panel\\Desktop"), REG_STRING_SCREENSAVEACTIVE, lpBuf, 100);
		CString sPoliciesBuf = lpBuf;
		if(sPoliciesBuf == _T("1"))
		{
			ZeroMemory(lpBuf, 100);
			RegGetStringValue(HKEY_CURRENT_USER, _T("Software\\Policies\\Microsoft\\Windows\\Control Panel\\Desktop"), REG_STRING_SCREENSAVEISSECURE, lpBuf, 100);
			CString sPoliBuf = lpBuf;
			if(sPoliBuf == _T("1"))
			{
				return _T("T");
			}
		}
	}

	return _T("F");
}

// 사용자의 로긴 패스워드가 설정되어 있는지의 여부를 알려준다.
CString CClientManager::GetLoginPW()
{
	USES_CONVERSION;

	// 사용자의 로그인 아이디를 얻어온다.
	CString sUserName;
	DWORD nSize = 128;
	if(GetUserName(sUserName.GetBuffer(nSize), &nSize)) 
	{
		sUserName.ReleaseBuffer();
	}

	TCHAR charid[32] = {0,};
	TCHAR charpass[32] = {0,};
	TCHAR charnewpass[32] = {0,};
	_tcscpy(charid, sUserName); //사용자 
	_tcscpy(charpass, _T(""));
	_tcscpy(charnewpass, _T(""));


	NET_API_STATUS nStatus;

	nStatus = NetUserChangePassword(NULL, T2W(charid), T2W(charpass), T2W(charnewpass));

	switch(nStatus)
	{
		case ERROR_INVALID_PASSWORD :
			return _T("T");
	}

	return _T("F");
}

// 오늘 날짜로 생성된 미니덤프의 갯수를 리턴한다.
UINT CClientManager::CheckTodayMiniDump()
{
	TCHAR szWinDir[MAX_PATH];
	UINT nRet = 0;

	CTimeSpan ts_yesterday(1, 0, 0, 0);
	CTime yesterday = CTime::GetCurrentTime() - ts_yesterday;
	int year_yest = yesterday.GetYear() - 2000;
	int month_yest = yesterday.GetMonth();
	int day_yest = yesterday.GetDay();

	if(GetWindowsDirectory(szWinDir, MAX_PATH) > 0)
	{
		CString sWinDir = szWinDir;
		sWinDir += _T("\\minidump\\mini*.*");

		WIN32_FIND_DATA filedata; // 파일 데이터의 스트럭처 
		HANDLE filehandle; // 검색의 핸들 

		// 모든 디렉토리의 정보를 읽어온다. 
		filehandle = FindFirstFile((LPCTSTR)sWinDir, &filedata); 
		if(filehandle == INVALID_HANDLE_VALUE) 
		{
			FindClose(filehandle);
			return 0; 
		}

		do {
			if((filedata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 
			&& CString(filedata.cFileName) != _T(".") 
			&& CString(filedata.cFileName) != _T("..")) 
			{
				CString sFileName = filedata.cFileName;
				sFileName = sFileName.Mid(4, 6);
				if(sFileName.GetLength() == 6)
				{
					int year = _ttoi(sFileName.Right(2));
					int month = _ttoi(sFileName.Left(2));
					int day = _ttoi(sFileName.Mid(2,2));
					if(year == year_yest && month == month_yest && day == day_yest)
					{
						nRet++;
					}
				}
			}
		} while(FindNextFile(filehandle, &filedata));

		FindClose(filehandle);
	}
	
	return nRet;
}
/*
// 정책을 읽어서 레지스트리에 저장한다.
void CClientManager::GetPolicies(CReceivedData *pReceivedData)
{
	// CString으로 옮겨담는다.
	TCHAR *pBuf = (TCHAR*)malloc(pReceivedData->m_nReceivedDataLength+1);
	ZeroMemory(pBuf, pReceivedData->m_nReceivedDataLength+1);
	memcpy(pBuf, pReceivedData->m_pData, pReceivedData->m_nReceivedDataLength);
	CString sReceived = pBuf;
	free(pBuf);

	WRITELOG(CString("### 정책(Policy) 받음 : ") + sReceived);

	// 레지스트리에 있는 기존 정책들을 제거한다.
	RegRecurseDeleteKey(HKEY_LOCAL_MACHINE, REG_PATH_IAS, "Policy");
	
	// 정책을 읽어서 레지스트리에 저장한다.
	int index = 0;
	CString sTmpStr;
	CString sRegKey;

	while(TRUE)
	{
		sRegKey = "Reg";
		sRegKey += Int2Str(index);
		BOOL bRet = AfxExtractSubString(sTmpStr, (LPCTSTR)sReceived, index, FIELD_SEPARATOR);
		if(bRet && sTmpStr.GetLength() > 0)
			RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS_POLICY, sRegKey, sTmpStr);
		else
			break;
		index++;
	}
}
*/
// 모든 Agent들에게 새로운 정책들로 업데이트 하도록 요청한다.
void CClientManager::RequestUpdatePolicies(BOOL bOffline)
{
	// by grman, 2008/07/04
	// Pause 상태라면 현재 정책을 무조건 '전체허용'으로 변경한다.
	if(m_bPaused)
	{
		CreateOfflinePolicy(BM_OFFLINE_POLICY_ALLOW_ALL);
		if (g_pBMSCommAgentDlg->m_bUserBaseForSSO && ReadUserInfoFile(USERINFO_LOGINID)!= _T(""))
		{
			WRITELOG(_T("[RequestUpdatePolicies] 정책 중지로 인하여 SSO연동 로그 아웃을 진행한다!!"));
			if (!ShellExecute(NULL, _T("open"), g_pBMSCommAgentDlg->m_sMainAgentPath + BM_LOGIN_PROCESS_NAME, _T("SSO -nac \"-authid:\""), NULL, SW_SHOW))
			{
				WRITELOG(_T("<OnTimer:INPUT_MONITORING_TIMER_ID> ShellExecute 실패 err(%d)"), GetLastError());
			}
		}
	}	
	WRITELOG(_T("[CClientManager::RequestUpdatePolicies] - Post Message 전송 전"));
	if (IsPolicyChanged() == TRUE)
	{
		WRITELOG(_T("[CClientManager::RequestUpdatePolicies] - 정책이 변경되었다!!"));
		FindAgentANDSendPostMSG(bOffline);
	}
}

BOOL CClientManager::IsPolicyChanged()
{
	BOOL bRet = FALSE;

	if (g_sEncryptPolicy == _T(""))
	{
		WRITELOG(_T("[CClientManager::IsPolicyChanged] - 값이 비어있으므로 정책을 읽는다"));
		g_sEncryptPolicy = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigData(BMS_DLP_POLICY_FILENAME);
		bRet = TRUE;
	}
	else
	{
		CString sGEDPolicyData = _T("");
		sGEDPolicyData = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigData(BMS_DLP_POLICY_FILENAME);			
		if (g_sEncryptPolicy == sGEDPolicyData && g_pBMSCommAgentDlg->m_bUserBase == FALSE)
		{
			WRITELOG(_T("[CClientManager::IsPolicyChanged] - 기존 정책과 일치!"));
			bRet = FALSE;
		}
		else
		{
			WRITELOG(_T("[CClientManager::IsPolicyChanged] - 기존 정책과 불일치! 혹은 공용 PC 사용!!"));
			g_sEncryptPolicy = sGEDPolicyData;
			bRet = TRUE;
		}
	}
	return bRet;
}

// ReactionAgent에게 Reaction에 따른 정보를 보낸다.
void CClientManager::UnlockPCToReactionAgent()
{
	// 2011/03/03 by realhan CC 모듈 수정
	if (FileExistRegInfo() == TRUE)
	{
		WRITELOG(_T("[CClientManager::UnlockPCToReactionAgent] RegInfo 파일 발견. REGINFO_STRING_SITENAME"));
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_REGINFO_FILENAME, BMS_REGINFO_LOCKED, _T(""), _T(""));
	}
	else
	{
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, BMS_REGINFO_LOCKED, _T(""));
	}

	HWND hWnd = ::FindWindow(_T("#32770"), _T("BMS IAS ReactionAgent (BlueMoonSoft co,. ltd. Copyright 2005)"));
	if (hWnd != NULL)
	{
		::SendMessage(hWnd, BMSM_REACTION_UNBLOCKPC, 0, 0);
	}
}

// 화면 저장 파일들을 제거하거나 서버로 전송한다.
void CClientManager::RemoveNSendScrRecItem()
{
	CString sFileName = m_sMonitoringFolder + CString(_T("\\")) + CString(SCREEN_JPG_FILENAME);

	WIN32_FIND_DATA filedata; // 파일 데이터의 스트럭처 
	HANDLE filehandle; // 검색의 핸들 
	CString sRetFileName;

	// 파일의 이름만 구한다.
	CString sFileNameOnly, sFilePathOnly;
	int nPos = sFileName.ReverseFind(_T('\\'));
	if(nPos < 0)
		return;
	
	sFileNameOnly = sFileName.Right(sFileName.GetLength()-nPos-1);
	sFilePathOnly = sFileName.Left(nPos+1);

	// 해당 파일이름으로 시작되는 모든 파일의 정보를 읽어온다. 
	filehandle = FindFirstFile((sFileName+_T("*.*")), &filedata); 
	if(filehandle == INVALID_HANDLE_VALUE) 
		return;

	do {
		if((filedata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 
		&& CString(filedata.cFileName) != _T(".") 
		&& CString(filedata.cFileName) != _T("..")
		&& CString(filedata.cFileName) != sFileNameOnly) 
		{
			sRetFileName = sFilePathOnly;
			sRetFileName += filedata.cFileName;

			// 파일이름에서 날짜를 추출하여 비교한다.
			RemoveNSendScrRecItem(sRetFileName);
		}
	} while (FindNextFile(filehandle, &filedata));

	FindClose(filehandle);
}

// 화면 저장 파일들을 제거하거나 서버로 전송한다.
void CClientManager::RemoveNSendScrRecItem(CString sFileName)
{
	POSITION pos = NULL;
	PRECITEM pRecItem = NULL;

	CString sFileNameOnly = GetFileNameOnly(sFileName);
	CString sTargetTime = sFileName.Right(14);
	
	if (sTargetTime.GetLength() >= 14)
	{
		int nYear = Str2Int(NumFilter(sTargetTime.Left(4)));
		int nMonth = Str2Int(NumFilter(sTargetTime.Mid(4, 2)));
		int nDay = Str2Int(NumFilter(sTargetTime.Mid(6, 2)));
		int nHour = Str2Int(NumFilter(sTargetTime.Mid(8, 2)));
		int nMin = Str2Int(NumFilter(sTargetTime.Mid(10, 2)));
		int nSec = Str2Int(NumFilter(sTargetTime.Mid(12, 2)));
		CTime TargetTime(nYear, nMonth, nDay, nHour, nMin, nSec);

		CTimeSpan tTimeSpan(0, 0, 5, 0);			// 5분전
		CTime CurTime = CTime::GetCurrentTime();
		CurTime -= tTimeSpan;

		BOOL bIsBefore = FALSE;
		BOOL bSend = FALSE;

		pos = m_RecItemList.GetHeadPosition();
		while(pos)
		{
			pRecItem = (PRECITEM)m_RecItemList.GetAt(pos);

			// 시간내에 있으면
			if(pRecItem->m_FromTime <= TargetTime && pRecItem->m_ToTime >= TargetTime)
			{
				bSend = TRUE;
				break;
			}
			else if(pRecItem->m_FromTime > TargetTime)
			{
				bIsBefore = TRUE;
			}
			
			// ToTime에서 5분이상 지났으면 해당 RecItem을 제거한다.
			if(pRecItem->m_ToTime < CurTime)
			{
				POSITION pos2 = pos;
				m_RecItemList.GetNext(pos);
				m_RecItemList.RemoveAt(pos2);
				delete pRecItem;
				continue;
			}

			m_RecItemList.GetNext(pos);
		}

		// 서버에 보내야 하는 데이터인 경우
		if(bSend)
		{
			CFileIndex *pFile = g_Log.OpenFile(sFileName, FALSE);
			if(pFile != NULL)
			{
				if(m_bConnected)
				{
					WRITELOG(_T("<MonitorLogFiles> 화면 저장을 서버로 전송 : %s"), sFileName);
					// Log파일의 내용을 서버에 전달한다.
					ProcessActionLog(COMMAND_SCREENREC_JPG_ACTIONLOG, pFile);
				}
			}
		}
		// 유출 전 시간대의 로그는 제거한다.
		else if(bIsBefore)
		{
			// 파일을 삭제한다.
			::DeleteFile(sFileName);
		}
	}
}

// 공유폴더 목록을 얻어서 돌려준다.
CString CClientManager::GetSharedFolderNames()
{
	PSHARE_INFO_502 pBuf, p;
	LPTSTR   lpszServer = _T("");

	CString sMsg;
	TCHAR sBuf[1024];
	ZeroMemory(sBuf, 1024);

	DWORD dwEntries=0, dwTotalEntries=0, dwResume=0;

	NET_API_STATUS res;

	// 2011/03/18, avilon
	// Debugging 모드에서 정상적으로 얻어 오지만 Release에서는 동작하지 않음.
	//res = NetShareEnum((LPWSTR)lpszServer, 502, (LPBYTE*) &pBuf, -1, &dwEntries, &dwTotalEntries, &dwResume);
	res = NetShareEnum(NULL, 502, (LPBYTE*) &pBuf, -1, &dwEntries, &dwTotalEntries, &dwResume);
	WRITELOG(_T("res=%d, lpszServer=%s, dwEntries=%d"), res, lpszServer, dwEntries);
	if(res == ERROR_SUCCESS)
	{
		p=pBuf;

		for(int i=1;i<=dwEntries;i++)
		{
//USES_CONVERSION;

			if(p->shi502_type == STYPE_DISKTREE)
			{
				//LPTSTR pszNetName = ((LPWSTR)p->shi502_netname);
				LPTSTR pszNetName = p->shi502_netname;
				//sprintf(sBuf, _T("%s"), pszNetName);
				if (_tcsicmp(pszNetName, _T("ipc$")) != 0 
					&& _tcsicmp(pszNetName, _T("print$")) != 0
					&& _tcsicmp(pszNetName, _T("fax$")) != 0)
				{
					_sntprintf(sBuf, 1023, _T("%s"), pszNetName);
					sMsg += sBuf;
					sMsg += _T(", ");
				}	
			}
			
			p++;
		}

		//AfxMessageBox(sMsg);
		//
		// Free the allocated buffer.
		//
		NetApiBufferFree((void*)pBuf);
	}

	return sMsg;
}

// 윈도우즈의 버전과 서비스팩버전을 돌려준다.
CString CClientManager::GetWindowsVersionNames()
{
	CString sRet, sTmp;

	TCHAR lpBuf[100];
	ZeroMemory(lpBuf, 100);
	RegGetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_MS_WINDOWS_CURVER, REG_STRING_WINDOWS_PRODUCTNAME, lpBuf, 100);
	sRet = lpBuf;
	sRet += _T(", ");

	ZeroMemory(lpBuf, 100);
	RegGetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_MS_WINDOWS_CURVER, REG_STRING_WINDOWS_VERSION, lpBuf, 100);
	sTmp = lpBuf;
	sRet += sTmp;

	return sRet;
}

CString CClientManager::GetVaccineNames()
{
	DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;

    if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
        return _T("");

    // Calculate how many process identifiers were returned.

    cProcesses = cbNeeded / sizeof(DWORD);

    // Print the name and process identifier for each process.
    for ( i = 0; i < cProcesses; i++ )
	{
        CString sProcessName = GetProcessName( aProcesses[i] );
		sProcessName.MakeUpper();

		POSITION pos = NULL;
		pos = m_VaccineList.GetHeadPosition();
		while(pos)
		{
			CString sVName = m_VaccineList.GetAt(pos);
			if(sProcessName == sVName)
				return sProcessName;

			m_VaccineList.GetNext(pos);
		}
	}

	return _T("");
}

CString CClientManager::GetProcessName(DWORD processID)
{
    TCHAR szProcessName[MAX_PATH] = _T("unknown");

    // Get a handle to the process.

    HANDLE hProcess = NULL;
	hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );

    // Get the process name.

    if (NULL != hProcess )
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), 
             &cbNeeded) )
        {
            GetModuleBaseName( hProcess, hMod, szProcessName, 
                               sizeof(szProcessName) );
        }
		else
		{
			ZeroMemory(szProcessName, MAX_PATH);
		}
    }
    else
	{
		ZeroMemory(szProcessName, MAX_PATH);
	}

    // Print the process name and identifier.
    //printf( "%s (Process ID: %u)\n", szProcessName, processID );

	if (hProcess)
		CloseHandle( hProcess );

	return szProcessName;
}

void CClientManager::InitVaccineList(CString sIniFile)
{
	// INI의 내용을 List에 넣는다.
	CString sVacList = _T("");
	sVacList = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_IASVACC_FILENAME, _T(""), BMS_ENVIRONMENT_FILE_COMMON_SECTION);

	m_VaccineList.RemoveAll();

	CString sVName = _T("");
	DWORD dwVacLen = sVacList.GetLength();
	for(DWORD i = 0; i < dwVacLen; i++)
	{
		TCHAR chTmp = sVacList[i];

		// Line 분리
		if(chTmp == 0x0a)
		{
			sVName.MakeUpper();
			if(sVName.CompareNoCase(BMS_ENVIRONMENT_FILE_COMMON_SECTION) != 0)
				m_VaccineList.AddTail(sVName);

			sVName = _T("");
		}
		// 라인끝은 0x0d, 0x0a이므로 0x0d는 무시하고 실질적인 끝은 0x0a로 판단한다.
		else if(chTmp == 0x0d)
		{
			// skip
		}
		else
		{
			sVName += chTmp;
		}
	}

	if(sVName.GetLength() > 0)
	{
		sVName.MakeUpper();
		m_VaccineList.AddTail(sVName);
	}
}


//////////////////////////////////////////////////////////////////////////

// 화면 저장을 위해 recitem 추가
void 
CClientManager::AddScreenRecordItem(const CTime& outflow_time)
{
	CTimeSpan Before(0, 0, RECITEM_BEFORE_MIN, RECITEM_BEFORE_SEC);
	CTimeSpan After(0, 0, RECITEM_AFTER_MIN, RECITEM_AFTER_SEC);

	PRECITEM pRecItem = new RECITEM;
	pRecItem->m_FromTime = outflow_time - Before;
	pRecItem->m_ToTime = outflow_time + After;

	m_RecItemList.AddTail(pRecItem);
}

// file로부터 내용을 얻음
void 
CClientManager::GetLogContents(CFileIndex* file, tstring& contents)
{
	UINT read_len = 0;
	TCHAR buffer[2049];

	contents	= _T("");

	assert(file != NULL);
	if(file->m_File.GetLength() <= 0)
		return;

	file->m_File.SeekToBegin();

	while(read_len = file->m_File.Read(buffer, 2048), read_len > 0)
	{
		buffer[read_len]	= 0;

		contents += buffer;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
// by grman, 2007/10/02

int LoadFileFromRes(CString strPath, CString strFile)
{
    HGLOBAL hResourceLoaded;  // handle to loaded resource
    HRSRC   hRes;              // handle/ptr to res. info.
    TCHAR    *lpResLock;        // pointer to resource data
    DWORD   dwSizeRes;

	CString strCustomResName = _T("XML");

    hRes = FindResource(NULL, 
                        strFile,//MAKEINTRESOURCE(nResourceId), 
                        strCustomResName
                    );

	if(!hRes) return -1;

    hResourceLoaded = LoadResource(NULL, hRes);
    lpResLock = (TCHAR *) LockResource(hResourceLoaded);
    dwSizeRes = SizeofResource(NULL, hRes);

	CString strFilePath = strPath + strFile;

	DeleteFile(strFilePath);

	FILE *fp;
	fp = _tfopen(strFilePath, _T("wb"));
	if(fp)
	{
		fwrite(lpResLock, dwSizeRes, 1, fp);
		fclose(fp);
	}
	else
	{
		return -1;
	}

	return 0;
}

// TODO : Discovery 정책 오프라인 정책 작업.
// 오프라인 정책을 생성하고 적용한다.
void CClientManager::CreateOfflinePolicy(int nOfflinePolicy)
{
	WRITELOG(_T("[CClientManager::CreateOfflinePolicy] - Start offline_policy(%d)"), nOfflinePolicy);

	TCHAR szXml[MAX_PATH + 1] = {0, };
	TCHAR szPrevXml[MAX_PATH + 1] = {0, };

	TCHAR szGedXml[MAX_PATH] = {0, };
	TCHAR szPrevGedXml[MAX_PATH] = {0, };

	TCHAR szCFGXml[MAX_PATH] = {0, };
	TCHAR szPrevCFGXml[MAX_PATH] = {0, };

	// 정책 이름, 기존 정책 이름
	GetPolicyFilePath(szXml);
	GetPrevPolicyFilePath(szPrevXml);

	GetPolicyGEDFilePath(szGedXml, MAX_PATH, FALSE);
	GetPrevPolicyGEDFilePath(szPrevGedXml, MAX_PATH);
	
	GetPolicyCFGFilePath(szCFGXml, MAX_PATH);
	GetPrevPolicyCFGFilePath(szPrevCFGXml, MAX_PATH);

	// 기존 정책이 없는데 기존 정책 사용이면 전체 허용 정책으로 전환
	if (nOfflinePolicy == BM_OFFLINE_POLICY_USE_PREV)
	{
		if ((FileExists(szPrevXml) == FALSE) && (FileExists(szPrevGedXml) == FALSE) && (FileExists(szPrevCFGXml) == FALSE))
		{
			nOfflinePolicy = BM_OFFLINE_POLICY_ALLOW_ALL;
		}
	}
	else if (nOfflinePolicy == BM_OFFLINE_POLICY_SELECT_LEVEL)
	{
		// 2016.02.17 solip
		TCHAR szPolicyPath[MAX_PATH+1];
		GetAgentPath(szPolicyPath);
		CString sOfflinePolicyFile = _T("");
		sOfflinePolicyFile.Format(_T("%s\%s"), szPolicyPath, BMS_DLP_OFFLINE_POLICY_FILENAME);
		BOOL bError = TRUE;
		if (FileExists(sOfflinePolicyFile) == TRUE)
		{
			CString sOffliePolicy = _T("");
			sOffliePolicy = MultiLanguageReadFile(sOfflinePolicyFile, m_bUseMultiLan? CHARSET_UTF8 : CHARSET_MULTIBYTE);
			if (sOffliePolicy != _T("") && sOffliePolicy.Find(_T("Level")) != -1 && sOffliePolicy.Find(_T("RegulationList")) != -1)
			{
				g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sOffliePolicy, _T(""));
				bError = FALSE;
			}

		}
		if (bError == TRUE)
		{
			nOfflinePolicy = BM_OFFLINE_POLICY_USE_PREV;
		}
	}
	
	if (nOfflinePolicy == BM_OFFLINE_POLICY_USE_PREV)
	{
		CString sReqData = GetReqData();
		if (IsInstantPolicies(sReqData) == TRUE)
		{
			CreateOfflineInstantPolicy(sReqData);
			if (m_bIsCurrentUsingInstantLevel == FALSE)
			{
				m_bIsCurrentUsingInstantLevel = TRUE;
				g_pBMSCommAgentDlg->SetTimer(ALLOW_INSTANTLEVEL_TIMER_ID, 1000*60, NULL);
			}
		}
		else
		{
			RestorePolicyFile();
			WRITELOG(_T("CreateOfflinePolicy - 기존 정책(%s) 사용함 - "), szPrevXml);
		}
	}
	else if (nOfflinePolicy != BM_OFFLINE_POLICY_SELECT_LEVEL)
	{
		// 경로
		TCHAR szPolicyPath[MAX_PATH+1];
		GetAgentPath(szPolicyPath);

		// 임시 정책명 (리소스에서 읽을 이름)
		CString strTmpPolicy = _T("");
		WRITELOG(_T("offline policy = %d"), nOfflinePolicy);
		if(nOfflinePolicy == BM_OFFLINE_POLICY_BLOCK_ALL) // 모두 차단 정책
		{
			if(m_bUseMultiLan)
			{
				strTmpPolicy = _T("block_utf8.xml");
			}
			else
			{
				strTmpPolicy = _T("block.xml");
			}
			WRITELOG(_T("CreateOfflinePolicy - 전체 차단 정책 사용함"));
		}
		else if(nOfflinePolicy == BM_OFFLINE_POLICY_ALLOW_ALL) // 모두 허용 정책
		{
			if(m_bUseMultiLan)
			{
				strTmpPolicy = _T("allow_utf8.xml");
			}
			else
			{
				strTmpPolicy = _T("allow.xml");
			}
			WRITELOG(_T("CreateOfflinePolicy - 전체 허용 정책 사용함"));
		}
		else if (nOfflinePolicy == BM_OFFLINE_POLICY_BLOCK_ALL_EXCEPT_WLAN)
		{
			WRITELOG(_T("[CretaeOfflinePolicy] 전체 차단이지만 WLAN만 허용"));
			if(m_bUseMultiLan)
			{
				strTmpPolicy = _T("block_excpet_wlan_utf8.xml");
			}
			else
			{
				strTmpPolicy = _T("block_excpet_wlan.xml");
			}
		}

		if (m_sAgentID == BM_AGENT_ID_SVRSMTP)
		{
			// exchange
			if(m_bUseMultiLan)
			{
				strTmpPolicy = _T("svrsmtp_utf8.xml");
			}
			else
			{
				strTmpPolicy = _T("svrsmtp.xml");
			}
		}		

		// 리소스에서 읽기
		LoadFileFromRes(szPolicyPath, strTmpPolicy);
		_tcsncat(szPolicyPath, strTmpPolicy, MAX_PATH);

		CString sEncryptedPolicyData = _T(""), sReceived = _T("");
		if(m_bUseMultiLan)
		{
			sReceived = MultiLanguageReadFile(szPolicyPath, CHARSET_UTF8);
		}
		else
		{
			sReceived = MultiLanguageReadFile(szPolicyPath, CHARSET_MULTIBYTE);
		}

		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sReceived, _T(""));
	}
	//2018.01.29 kjh4789 SSO 연동 offline 정책
	if (g_pBMSCommAgentDlg->m_bUserBaseForSSO == TRUE && m_bPolicyReceived == FALSE)
	{
		CString sSSOOfflinePolicy = _T(""), sContents = _T(""), sAgentID = _T(""), sCmd = _T("");
		sSSOOfflinePolicy.Format(_T("%stmp\\%s"), g_pBMSCommAgentDlg->m_sBMSDATLogPath, BMS_SSO_OFFLINE_POLICY_FILE);
		if (FileExists(sSSOOfflinePolicy) == TRUE)
		{
			if (ChangeLastLoginInfoToUserIDFile() == TRUE)
			{
				sContents = MultiLanguageReadFile(sSSOOfflinePolicy, CHARSET_UTF8);
				g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sContents, _T(""));
				WRITELOG(_T("[CreateOfflinePolicy] SSO File Offline Policy!! sContents : %s"), sContents);
			}
			else
			{
				WRITELOG(_T("[CreateOfflinePolicy] ChangeLastLoginInfoToUserIDFile Fail!! 일반 오프라인 정책 적용"));
			}	
		}
	}

	//CreateOfflinePolicy 호출 이 후 무조건 RequestUpdatePolicies를 호출하고 있는데 아래의 작업을 RequestUpdatePolicies에서 처리한다.
// 	if (IsPolicyChanged() == TRUE)
// 	{
// 		WRITELOG(_T("[CreateOfflinePolicy] 정책이 변경되었다!!"));
// 		FindAgentANDSendPostMSG();
// 
// 		HWND hWnd = ::FindWindow(_T("#32770"), BMDC_MAIN_UI_WND_TITLE);
// 		if (hWnd)
// 		{
// 			WRITELOG(_T("[CClientManager::RequestDiscoveryPolicy] - 디스커버리 정책 업데이트 알림."));
// 			::PostMessage(hWnd, BMDC_UPDATE_POLICY, NULL, NULL);
// 		}
// 
// 		hWnd = ::FindWindow(_T("#32770"), BMDC_DOC_CHECKER_WND_TITLE);
// 		if (hWnd)
// 		{
// 			WRITELOG(_T("[CBMSCommAgentDlg::RequestDiscoveryPolicy] - 디스커버리 정책 업데이트 알림.(ENG)"));
// 			::PostMessage(hWnd, BMDC_UPDATE_POLICY, NULL, NULL);
// 		}
// 	}

	WRITELOG(_T("CreateOfflinePolicy End"));
}

BOOL CClientManager::CreateOfflineInstantPolicy(CString sReqData)
{
	BOOL bRet = FALSE;

	TCHAR szLogPath[MAX_PATH] = {0, };
	GetAgentLogPath(szLogPath, MAX_PATH);

	CString sOriginPolicy = _T(""), sPolicyContents = _T("");
	sOriginPolicy.Format(_T("%s%s\\OriginPolicy"), szLogPath, BM_RDATA);
	sPolicyContents = MultiLanguageReadFile(sOriginPolicy, CHARSET_UTF8);
	WRITELOG(_T("[CreateOfflineInstantPolicy] sPolicyContents : %s"), sPolicyContents);

	EnterCriticalSection(&m_AllowDataMapCS);
	int nInstantLevelCount = m_cAllowDataMap.GetCount();
	POSITION stPos = m_cAllowDataMap.GetStartPosition();
	LeaveCriticalSection(&m_AllowDataMapCS);

	CMap<CString, LPCTSTR, BOOL, BOOL&> cInstantPolicyActionMap;

	CString sInstantPolicyAction = _T("");
	while(stPos)
	{
		CString sKey = _T("");	
		ALLOW_DATA stAllowData;
		EnterCriticalSection(&m_AllowDataMapCS);
		m_cAllowDataMap.GetNextAssoc(stPos, sKey, stAllowData);
		LeaveCriticalSection(&m_AllowDataMapCS);

		sInstantPolicyAction += sKey + _T(",");

#define INSTANT_POLICY_ACTION_INDEX 1
#define INSTANT_POLICY_REACTION_INDEX 2

		CString sAction = _T(""), sMapKey = _T(""), sReaction = _T(""), sDateTime = _T("");
		AfxExtractSubString(sAction, stAllowData.m_sAction, INSTANT_POLICY_ACTION_INDEX, g_Separator);
		AfxExtractSubString(sReaction, stAllowData.m_sAction, INSTANT_POLICY_REACTION_INDEX, g_Separator);
		
		BOOL bReserved = TRUE;
		if (sReaction == _T("WATERMARK"))
		{
			sMapKey = sAction + g_Separator + _T("WATERMARK_EX");
			cInstantPolicyActionMap.SetAt(sMapKey, bReserved);
		}
		else if (sReaction == _T("WATERMARK_EX"))
		{
			sReaction = _T("WATERMARK");
		}

		sMapKey = sAction + g_Separator + sReaction;
		WRITELOG(_T("[CreateOfflineInstantPolicy] sMapKey : %s"), sMapKey);
		cInstantPolicyActionMap.SetAt(sMapKey, bReserved);

		SYSTEMTIME stCurTime;
		GetLocalTime(&stCurTime);
// 		GetSystemTime(&stCurTime);
// 		stCurTime.wHour += 9;
// 		if(stCurTime.wHour >= 24)
// 		{
// 			stCurTime.wHour -= 24;
// 			stCurTime.wDay++;
// 		}
		CString sCurrentTime = _T("");
		sCurrentTime.Format(_T("%d%02d%02d%02d%02d%02d"), stCurTime.wYear, stCurTime.wMonth, stCurTime.wDay, stCurTime.wHour, stCurTime.wMinute, stCurTime.wSecond);

		if (sCurrentTime > stAllowData.m_sEndDate)
		{
			WRITELOG(_T("[CreateOfflineInstantPolicy] 임시정책 만료!!"));
			cInstantPolicyActionMap.RemoveKey(sMapKey);
			ModifyReqData(sReqData, stAllowData.m_dwTableid);
		}
	}

	WriteAllowData(sReqData, TRUE);

	WRITELOG(_T("[CreateOfflineInstantPolicy] sInstantPolicyAction(%s)"), sInstantPolicyAction);
	//DeleteDuplicatePolicy(sPolicyContents);
	if(sInstantPolicyAction.IsEmpty() == FALSE)
	{
		CXMLFile xmlfile;
		if(!xmlfile.LoadFromString(sPolicyContents))
		{
			WRITELOG(_T("[CreateOfflineInstantPolicy] xmlfile.LoadFromString 실패"));
			return FALSE;
		}

		HRESULT hr;

		// get root element
		CComPtr<IXMLDOMElement> pRootElem;
		hr = xmlfile.GetRootNode(&pRootElem);
		if(FAILED(hr))
		{
			WRITELOG(_T("[CreateOfflineInstantPolicy] xmlfile.GetRootNode 실패"));
			return FALSE;
		}

		// read regulation list
		CComPtr<IXMLDOMNodeList> pRegulationList;
		hr = xmlfile.GetPathedNodeList(pRootElem, _T("RegulationList/Regulation"), &pRegulationList);
		if(FAILED(hr))
		{
			WRITELOG(_T("[CreateOfflineInstantPolicy] xmlfile.GetPathedNodeList 실패"));
			return FALSE;
		}

		// 정책을 읽어 등록한다.
		long n_regulations;
		hr = pRegulationList->get_length(&n_regulations);
		if(FAILED(hr))
		{
			WRITELOG(_T("[CreateOfflineInstantPolicy] xmlfile.get_length 실패"));
			return FALSE;
		}

		for(int l=0; l<n_regulations; l++) 
		{
			CString str;
			CComPtr<IXMLDOMNode> pRegulNode;
			hr = pRegulationList->get_item(l, &pRegulNode);
			if(FAILED(hr))
			{
				WRITELOG(_T("[CreateOfflineInstantPolicy] xmlfile.get_item 실패"));
				return FALSE;
			}

			CComQIPtr<IXMLDOMElement> pRegulElem;
			pRegulElem = pRegulNode;

			CString sReaction;
			xmlfile.GetAttributeValue(pRegulElem, _T("reaction"), sReaction);
			if(!sReaction.CompareNoCase(_T("BLOCK")) || !sReaction.CompareNoCase(_T("WATERMARK")) || !sReaction.CompareNoCase(_T("WATERMARK_EX")))
			{
				CString sAction;
				xmlfile.GetAttributeValue(pRegulElem, _T("action"), sAction);

				CString sKey = sAction + g_Separator + sReaction;
				WRITELOG(_T("[CreateOfflineInstantPolicy] sKey : %s"), sKey);
				BOOL bReserved = FALSE;

				if(cInstantPolicyActionMap.Lookup(sKey, bReserved))// if(sInstantLevelAction.Find(sAction + g_Separator + sReaction) > -1)
				{
					WRITELOG(_T("[CreateOfflineInstantPolicy] xml 편집(%s)"), sAction + g_Separator + sReaction);

					CComPtr<IXMLDOMNode> pParentNode;
					if(xmlfile.GetParentNode(pRegulElem, &pParentNode))
					{
						CComPtr<IXMLDOMNode> pOldNode;
						hr = pParentNode->removeChild(pRegulElem, &pOldNode);
						if(FAILED(hr))
						{
							WRITELOG(_T("[CreateOfflineInstantPolicy] xmlfile.removeChild 실패"));
							return FALSE;
						}						
						else
						{
							// 2017.09.19 hangebs 이 분기문 안에서 USBPORT와 같이 정책 내용이 글로벌과 관계 있을 경우 수정이 필요하다
							if (sAction.CompareNoCase(_T("USBPORT")) == 0)
							{
								xmlfile.SetAttributeValue(pRootElem, _T("usbportdefault"), _T("0"));
								xmlfile.SetAttributeValue(pRootElem, _T("usbportlogsave"), _T("1"));
								xmlfile.SetAttributeValue(pRootElem, _T("usbportlogblock"), _T("1"));
								WRITELOG(_T("[CreateInstantPolicy] USBPORT Global Set"));
							}
						}
					}
					else
					{
						WRITELOG(_T("[CreateOfflineInstantPolicy] xmlfile.GetParentNode 실패"));
					}
				}
			}
		}
		CString sInstantLevelXML = _T("");
		xmlfile.GetXML(sInstantLevelXML);
		sInstantLevelXML.Replace(_T("<?xml version=\"1.0\"?>"), _T("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
		sPolicyContents = sInstantLevelXML;
	}
	else
	{
		WRITELOG(_T("[CreateOfflineInstantPolicy] 임시 정책 미적용 상태"));
	}

	m_policy_update_time = _T("");

	if(m_OSVerMajor >= 6)
	{
		TCHAR tempBuf[MAX_PATH];
		GetCSIDLDirectory(tempBuf, CSIDL_PROFILE);
		CString sUserDir = tempBuf;
		DWORD dwPos = sUserDir.ReverseFind(_T('\\'));
		sUserDir = sUserDir.Left(dwPos + 1);
		sUserDir += m_sSupportName;
		_sntprintf(tempBuf, MAX_PATH-1, _T("%s\\AppData\\Local\\VirtualStore\\Windows\\System32\\"), sUserDir);
		WRITELOG(_T("[CreateOfflineInstantPolicy] Remove dir dir path : %s"), tempBuf);
		TCHAR VirbmsdatPath[MAX_PATH], VirbmsbinPath[MAX_PATH];
		_sntprintf(VirbmsbinPath, MAX_PATH-1, _T("%sbmsbin\\"),tempBuf);
		_sntprintf(VirbmsdatPath, MAX_PATH-1, _T("%sbmsdat\\"),tempBuf);
		RemoveAllDir(VirbmsbinPath);
		RemoveAllDir(VirbmsdatPath);
	}
	FILE*		fp	= NULL;
	TCHAR*		data_ptr	= NULL;

	if(sPolicyContents.GetLength() == 0)
	{
		WRITELOG(_T("[CreateOfflineInstantPolicy] CreateInstantPolicy - 정책이 존재하지 않음"));
		return FALSE;
	}

	m_bPolicyReceived = TRUE;

	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sPolicyContents, _T(""));
	m_sDLPPolicyTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));

	WRITELOG(_T("[CreateOfflineInstantPolicy] CreateInstantPolicy : 정책 파일 생성"));

	// Agent들에게 알림
	//RequestUpdatePolicies();
	// 2013.08.13, kih5893 임시정책 요청 후에도 copysave 값 가져올 수 있도록 수정 
	//ApplyPolicies();


	return bRet;
}

BOOL CClientManager::IsInstantPolicies(CString sContents)
{
	BOOL bRet = FALSE;
	
	CString sTemp = _T(""), sState = _T(""), sEndTime = _T(""), sSub = _T("");

	int nCnt = 0;
	while(AfxExtractSubString(sTemp, sContents, nCnt++, _T('|')))
	{
		AfxExtractSubString(sState, sTemp, 8, (TCHAR)FIELD_SEPARATOR);
		ALLOW_DATA stAllowData;
		stAllowData.m_dwTableid = 0;
		stAllowData.m_sAction = _T("");
		stAllowData.m_sEndDate = _T("");
		stAllowData.m_sState = sState;
		stAllowData.m_sUnit = _T("");

		if (sState == _T("A"))
		{
			bRet = TRUE;
			m_bIsCurrentUsingInstantLevel = TRUE;
			AfxExtractSubString(sSub, sTemp, 0, g_Separator);
			stAllowData.m_dwTableid = _ttoi(sSub);
			stAllowData.m_sAction = sSub;
			stAllowData.m_sAction += g_Separator;
			AfxExtractSubString(sSub, sTemp, 12, g_Separator);
			DWORD dwTerm = _ttoi(sSub);
			AfxExtractSubString(sSub, sTemp, 5, g_Separator);
			stAllowData.m_sAction += sSub;
			// 2009/08/11 by avilon, Action과 Reaction으로 맵키를 지정.
			stAllowData.m_sAction += g_Separator;
			AfxExtractSubString(sSub, sTemp, 16, g_Separator);
			stAllowData.m_sAction += sSub;
			stAllowData.sReqType = stAllowData.sReqType = sSub.CompareNoCase(_T("BLOCK"))==0 ? _T("") : sSub;
			AfxExtractSubString(sEndTime, sTemp, 17, g_Separator);
			AfxExtractSubString(sSub, sTemp, 19, g_Separator);
			if (sSub.GetLength() > 0)
			{
				stAllowData.m_sUnit = sSub;
			}
			else
			{
				stAllowData.m_sUnit = _T("MINUTE");
			}
			WRITELOG(_T("[IsInstantPolicies] sEndTime : %s, sTemp : %s"), sEndTime, sTemp);
			if (sEndTime.IsEmpty() == TRUE)
			{
				WRITELOG(_T("[IsInstantPolicies] ExtractAllowData sEndTime NULL"));
				CalculateDate stDate;

				if(stAllowData.m_sUnit.CompareNoCase(_T("MONTH"))==0)
				{
					stDate.AddMonth(dwTerm);
				}
				else if(stAllowData.m_sUnit.CompareNoCase(_T("HOUR"))==0)
				{
					stDate.AddHour(dwTerm);
				}
				else if(stAllowData.m_sUnit.CompareNoCase(_T("DAY"))==0)
				{
					stDate.AddDay(dwTerm);
				}
				// 2011/01/27, avilon - 위의 조건에 없으면 무조건 분 단위다.
				else
				{
					stDate.AddMinute(dwTerm);
				}

				SYSTEMTIME stCurTime;
				stDate.GetCalcDateTime(stCurTime);

				stAllowData.m_sEndDate.Format(_T("%d%02d%02d%02d%02d%02d"), stCurTime.wYear, stCurTime.wMonth, stCurTime.wDay, stCurTime.wHour, stCurTime.wMinute, stCurTime.wSecond);

				CString sEndDate = _T(""), sTempResult = _T("");
				int nLoc = 0;
				while (AfxExtractSubString(sEndDate, sTemp, nLoc, FIELD_SEPARATOR))
				{
					if(nLoc == 17)
					{
						sTempResult += stAllowData.m_sEndDate;
					}
					else
					{
						sTempResult += sEndDate;
					}
					sTempResult += g_Separator;
					nLoc++;
				}
				sTemp = sTempResult.Left(sTempResult.GetLength()-1);
			}
			else
			{
				WRITELOG(_T("[IsInstantPolicies] ExtractAllowData sEndTime !NULL"));
				stAllowData.m_sEndDate = sEndTime;
			}

			EnterCriticalSection(&m_AllowDataMapCS);
			m_cAllowDataMap.SetAt(stAllowData.m_sAction, stAllowData);
			LeaveCriticalSection(&m_AllowDataMapCS);
		}
	}

	return bRet;
}

// 정책을 백업한다. (.prev)
int CClientManager::BackupPolicyFile()
{
	m_bPolicyReceived = FALSE;
	TCHAR szBuf[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetPolicyFilePath(szBuf);
	if (FileExists(szBuf) == TRUE)
	{
		TCHAR szBufPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		GetPrevPolicyFilePath(szBufPrev);
		::DeleteFile(szBufPrev);
		if (_trename(szBuf, szBufPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("[CClientManager::BackupPolicyFile] - 정책이 존재하여 백업함 : %s"), szBufPrev);
	}
	
	TCHAR szGEDBuf[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetPolicyGEDFilePath(szGEDBuf, MAX_PATH);
	if (FileExists(szGEDBuf) == TRUE)
	{
		TCHAR szGEDBufPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		GetPrevPolicyGEDFilePath(szGEDBufPrev, MAX_PATH);
		::DeleteFile(szGEDBufPrev);
		if (_trename(szGEDBuf, szGEDBufPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - GED 정책이 존재하여 백업함 : %s"), szGEDBufPrev);
	}

	TCHAR szCFGBuf[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetPolicyCFGFilePath(szCFGBuf, MAX_PATH);
	if (FileExists(szCFGBuf) == TRUE)
	{
		TCHAR szCFGBufPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		GetPrevPolicyCFGFilePath(szCFGBufPrev, MAX_PATH);
		::DeleteFile(szCFGBufPrev);
		if (_trename(szCFGBuf, szCFGBufPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - CFG 정책이 존재하여 백업함 : %s"), szCFGBufPrev);
	}

	// 2011/09/28, avilon
	// 디스커버리 정책도 백업한다.
	TCHAR szDisPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szDisPolicy);
	_tcsncat(szDisPolicy, BM_DISCOVERY_POLICY_GED_FILENAME, MAX_PATH);
	if (FileExists(szDisPolicy) == TRUE)
	{
		TCHAR szDisPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szDisPolicyPrev, szDisPolicy, MAX_PATH);
		_tcsncat(szDisPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szDisPolicyPrev);
		if(_trename(szDisPolicy, szDisPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 디스커버리 정책이 존재하여 백업함 : %s"), szDisPolicyPrev);
	}

	TCHAR szDisDatPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szDisDatPolicy);
	_tcsncat(szDisDatPolicy, BM_DISCOVERY_POLICY_DAT_FILENAME, MAX_PATH);
	if (FileExists(szDisDatPolicy) == TRUE)
	{
		TCHAR szDisDatPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szDisDatPolicyPrev, szDisDatPolicy, MAX_PATH);
		_tcsncat(szDisDatPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szDisDatPolicyPrev);
		if (_trename(szDisDatPolicy, szDisDatPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 디스커버리 정책이 존재하여 백업함 : %s"), szDisDatPolicyPrev);
	}

	TCHAR szDisGedPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szDisGedPolicy);
	_tcsncat(szDisGedPolicy, BM_DISCOVERY_POLICY_CFG_FILENAME, MAX_PATH);
	if (FileExists(szDisGedPolicy) == TRUE)
	{
		TCHAR szDisGedPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szDisGedPolicyPrev, szDisGedPolicy, MAX_PATH);
		_tcsncat(szDisGedPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szDisGedPolicyPrev);
		if (_trename(szDisGedPolicy, szDisGedPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 디스커버리 정책이 존재하여 백업함 : %s"), szDisGedPolicyPrev);
	}

	//2015.03.16, keede122
	// 자산관리 정책도 백업한다.
	TCHAR szAsGedPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szAsGedPolicy);
	_tcsncat(szAsGedPolicy, BM_ASSET_POLICY_GED_FILENAME, MAX_PATH);
	if (FileExists(szAsGedPolicy) == TRUE)
	{
		TCHAR szAsGedPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szAsGedPolicyPrev, szAsGedPolicy, MAX_PATH);
		_tcsncat(szAsGedPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szAsGedPolicyPrev);
		if (_trename(szAsGedPolicy, szAsGedPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 자산관리 정책이 존재하여 백업함 : %s"), szAsGedPolicyPrev);
	}

	TCHAR szAsDatPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szAsDatPolicy);
	_tcsncat(szAsDatPolicy, BM_ASSET_POLICY_DAT_FILENAME, MAX_PATH);
	if (FileExists(szAsDatPolicy) == TRUE)
	{
		TCHAR szAsDatPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szAsDatPolicyPrev, szAsDatPolicy, MAX_PATH);
		_tcsncat(szAsDatPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szAsDatPolicyPrev);
		if (_trename(szAsDatPolicy, szAsDatPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 자산관리 정책이 존재하여 백업함 : %s"), szAsDatPolicyPrev);
	}

	TCHAR szAsCfgPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szAsCfgPolicy);
	_tcsncat(szAsCfgPolicy, BM_ASSET_POLICY_CFG_FILENAME, MAX_PATH);
	if (FileExists(szAsCfgPolicy) == TRUE)
	{
		TCHAR szAsCfgPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szAsCfgPolicyPrev, szAsCfgPolicy, MAX_PATH);
		_tcsncat(szAsCfgPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szAsCfgPolicyPrev);
		if (_trename(szAsCfgPolicy, szAsCfgPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 자산관리 정책이 존재하여 백업함 : %s"), szAsCfgPolicyPrev);
	}

	// 2020.01.07, yoon
	// PC보안감사 정책도 백업한다.
	TCHAR szPsGedPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szPsGedPolicy);
	_tcsncat(szPsGedPolicy, BM_PCSECU_POLICY_GED_FILENAME, MAX_PATH);
	if (FileExists(szPsGedPolicy) == TRUE)
	{
		TCHAR szPsGedPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szPsGedPolicyPrev, szPsGedPolicy, MAX_PATH);
		_tcsncat(szPsGedPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szPsGedPolicyPrev);
		if (_trename(szPsGedPolicy, szPsGedPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 하드웨어 디바이스 정책이 존재하여 백업함 : %s"), szPsGedPolicyPrev);
	}

	TCHAR szPsDatPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szPsDatPolicy);
	_tcsncat(szPsDatPolicy, BM_PCSECU_POLICY_DAT_FILENAME, MAX_PATH);
	if (FileExists(szPsDatPolicy) == TRUE)
	{
		TCHAR szPsDatPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szPsDatPolicyPrev, szPsDatPolicy, MAX_PATH);
		_tcsncat(szPsDatPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szPsDatPolicyPrev);
		if (_trename(szPsDatPolicy, szPsDatPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 하드웨어 디바이스 정책이 존재하여 백업함 : %s"), szPsDatPolicyPrev);
	}

	TCHAR szPsCfgPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szPsCfgPolicy);
	_tcsncat(szPsCfgPolicy, BM_PCSECU_POLICY_CFG_FILENAME, MAX_PATH);
	if (FileExists(szPsCfgPolicy) == TRUE)
	{
		TCHAR szPsCfgPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szPsCfgPolicyPrev, szPsCfgPolicy, MAX_PATH);
		_tcsncat(szPsCfgPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szPsCfgPolicyPrev);
		if (_trename(szPsCfgPolicy, szPsCfgPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 하드웨어 디바이스 정책이 존재하여 백업함 : %s"), szPsCfgPolicyPrev);
	}

	// 2020.01.07, yoon
	// 하드웨어 디바이스 정책도 백업한다.
	TCHAR szHdGedPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szHdGedPolicy);
	_tcsncat(szHdGedPolicy, BM_HARDWAREDEVICE_POLICY_GED_FILENAME, MAX_PATH);
	if (FileExists(szHdGedPolicy) == TRUE)
	{
		TCHAR szHdGedPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szHdGedPolicyPrev, szHdGedPolicy, MAX_PATH);
		_tcsncat(szHdGedPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szHdGedPolicyPrev);
		if (_trename(szHdGedPolicy, szHdGedPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 하드웨어 디바이스 정책이 존재하여 백업함 : %s"), szHdGedPolicyPrev);
	}

	TCHAR szHdDatPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szHdDatPolicy);
	_tcsncat(szHdDatPolicy, BM_HARDWAREDEVICE_POLICY_DAT_FILENAME, MAX_PATH);
	if (FileExists(szHdDatPolicy) == TRUE)
	{
		TCHAR szHdDatPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szHdDatPolicyPrev, szHdDatPolicy, MAX_PATH);
		_tcsncat(szHdDatPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szHdDatPolicyPrev);
		if (_trename(szHdDatPolicy, szHdDatPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 하드웨어 디바이스 정책이 존재하여 백업함 : %s"), szHdDatPolicyPrev);
	}

	TCHAR szHdCfgPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szHdCfgPolicy);
	_tcsncat(szHdCfgPolicy, BM_HARDWAREDEVICE_POLICY_CFG_FILENAME, MAX_PATH);
	if (FileExists(szHdCfgPolicy) == TRUE)
	{
		TCHAR szHdCfgPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
		_tcsncpy(szHdCfgPolicyPrev, szHdCfgPolicy, MAX_PATH);
		_tcsncat(szHdCfgPolicyPrev, _T(".prev"), MAX_PATH);
		::DeleteFile(szHdCfgPolicyPrev);
		if (_trename(szHdCfgPolicy, szHdCfgPolicyPrev))
		{
			// 이름 변경 실패
		}
		WRITELOG(_T("BackupPolicyFile - 하드웨어 디바이스 정책이 존재하여 백업함 : %s"), szHdCfgPolicyPrev);
	}
	// 정책이 존재하지 않음
	return 0;
}

// 백업된 정책을 복원한다. (.prev 제거)
int CClientManager::RestorePolicyFile()
{
	TCHAR szBuf[MAX_PATH + 1] = {0, };
	TCHAR szBufPrev[MAX_PATH + 1] = {0, };
	GetPolicyFilePath(szBuf);
	GetPrevPolicyFilePath(szBufPrev);

	if(FileExists(szBufPrev))
	{
		if(FileExists(szBuf)) ::DeleteFile(szBuf);
		if(_trename(szBufPrev, szBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - 백업 정책이 존재하여 복원함 : %s"), szBuf);
	}

	TCHAR szGEDBuf[MAX_PATH + 1] = {0, };
	TCHAR szGEDBufPrev[MAX_PATH + 1] = {0, };
	GetPolicyGEDFilePath(szGEDBuf, MAX_PATH);
	GetPrevPolicyGEDFilePath(szGEDBufPrev, MAX_PATH);

	if(FileExists(szGEDBufPrev))
	{
		if(FileExists(szGEDBuf)) ::DeleteFile(szGEDBuf);
		if(_trename(szGEDBufPrev, szGEDBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - GED 백업 정책이 존재하여 복원함 : %s"), szGEDBuf);
	}

	TCHAR szCFGBuf[MAX_PATH + 1] = {0, };
	TCHAR szCFGBufPrev[MAX_PATH + 1] = {0, };
	GetPolicyCFGFilePath(szCFGBuf, MAX_PATH);
	GetPrevPolicyCFGFilePath(szCFGBufPrev, MAX_PATH);

	if (FileExists(szCFGBufPrev))
	{
		if(FileExists(szCFGBuf)) ::DeleteFile(szCFGBuf);
		if(_trename(szCFGBufPrev, szCFGBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - CFG 백업 정책이 존재하여 복원함 : %s"), szCFGBuf);
	}

	TCHAR szDsBuf[MAX_PATH + 1] = {0, };
	TCHAR szDsBufPrev[MAX_PATH + 1] = {0, };
	GetDsPolicyFilePath(szDsBuf, MAX_PATH);
	GetDsPrevPolicyFilePath(szDsBufPrev);

	if(FileExists(szDsBufPrev))
	{
		if(FileExists(szDsBuf)) ::DeleteFile(szDsBuf);
		if(_trename(szDsBufPrev, szDsBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - 백업 정책이 존재하여 복원함 : %s"), szDsBuf);
	}

	TCHAR szDsGEDBuf[MAX_PATH + 1] = {0, };
	TCHAR szDsGEDBufPrev[MAX_PATH + 1] = {0, };
	GetDsPolicyGEDFilePath(szDsGEDBuf, MAX_PATH);
	GetDsPrevPolicyGEDFilePath(szDsGEDBufPrev, MAX_PATH);

	if(FileExists(szDsGEDBufPrev))
	{
		if(FileExists(szDsGEDBuf)) ::DeleteFile(szDsGEDBuf);
		if(_trename(szDsGEDBufPrev, szDsGEDBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - GED 백업 정책이 존재하여 복원함 : %s"), szDsGEDBuf);
	}

	TCHAR szDsCFGBuf[MAX_PATH + 1] = {0, };
	TCHAR szDsCFGBufPrev[MAX_PATH + 1] = {0, };
	GetDsPolicyGEDFilePath(szDsCFGBuf, MAX_PATH);
	GetDsPrevPolicyGEDFilePath(szDsCFGBufPrev, MAX_PATH);

	if(FileExists(szDsCFGBufPrev))
	{
		if(FileExists(szDsCFGBuf)) ::DeleteFile(szDsCFGBuf);
		if(_trename(szDsCFGBufPrev, szDsCFGBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - GED 백업 정책이 존재하여 복원함 : %s"), szDsCFGBuf);
	}

	// 2015.03.17, keede122
	// 자산관리 정책도 백업된 정책파일을 복원한다.

	TCHAR szAsBuf[MAX_PATH + 1] = {0, };
	TCHAR szAsBufPrev[MAX_PATH + 1] = {0, };
	GetAsPolicyFilePath(szAsBuf, MAX_PATH);
	GetAsPrevPolicyFilePath(szAsBufPrev, MAX_PATH);

	if(FileExists(szAsBufPrev))
	{
		if(FileExists(szAsBuf)) ::DeleteFile(szAsBuf);
		if(_trename(szAsBufPrev, szAsBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - 백업 정책이 존재하여 복원함 : %s"), szAsBuf);
	}

	TCHAR szAsGEDBuf[MAX_PATH + 1] = {0, };
	TCHAR szAsGEDBufPrev[MAX_PATH + 1] = {0, };
	GetAsPolicyGEDFilePath(szAsGEDBuf, MAX_PATH);
	GetPrevPolicyGEDFilePath(szAsGEDBufPrev, MAX_PATH);

	if(FileExists(szAsGEDBufPrev))
	{
		if(FileExists(szAsGEDBuf)) ::DeleteFile(szAsGEDBuf);
		if(_trename(szAsGEDBufPrev, szAsGEDBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - GED 백업 정책이 존재하여 복원함 : %s"), szAsGEDBuf);
	}

	TCHAR szAsCFGBuf[MAX_PATH + 1] = {0, };
	TCHAR szAsCFGBufPrev[MAX_PATH + 1] = {0, };
	GetAsPolicyCFGFilePath(szAsCFGBuf, MAX_PATH);
	GetPrevPolicyCFGFilePath(szAsCFGBufPrev, MAX_PATH);

	if (FileExists(szAsCFGBufPrev))
	{
		if(FileExists(szAsCFGBuf)) ::DeleteFile(szAsCFGBuf);
		if(_trename(szAsCFGBufPrev, szAsCFGBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - CFG 백업 정책이 존재하여 복원함 : %s"), szAsCFGBuf);
	}

	// 2020.01.07, yoon
	// PC보안감사 정책도 백업된 정책파일을 복원한다.

	TCHAR szPsBuf[MAX_PATH + 1] = {0, };
	TCHAR szPsBufPrev[MAX_PATH + 1] = {0, };
	GetPsPolicyFilePath(szPsBuf, MAX_PATH);
	GetPsPrevPolicyFilePath(szPsBufPrev, MAX_PATH);

	if(FileExists(szPsBufPrev))
	{
		if(FileExists(szPsBuf)) ::DeleteFile(szPsBuf);
		if(_trename(szPsBufPrev, szPsBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - 백업 정책이 존재하여 복원함 : %s"), szPsBuf);
	}

	TCHAR szPsGEDBuf[MAX_PATH + 1] = {0, };
	TCHAR szPsGEDBufPrev[MAX_PATH + 1] = {0, };
	GetPsPolicyGEDFilePath(szPsGEDBuf, MAX_PATH);
	GetPsPrevPolicyGEDFilePath(szPsGEDBufPrev, MAX_PATH);

	if(FileExists(szPsGEDBufPrev))
	{
		if(FileExists(szPsGEDBuf)) ::DeleteFile(szPsGEDBuf);
		if(_trename(szPsGEDBufPrev, szPsGEDBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - GED 백업 정책이 존재하여 복원함 : %s"), szPsGEDBuf);
	}

	TCHAR szPsCFGBuf[MAX_PATH + 1] = {0, };
	TCHAR szPsCFGBufPrev[MAX_PATH + 1] = {0, };
	GetPsPolicyCFGFilePath(szPsCFGBuf, MAX_PATH);
	GetPsPrevPolicyCFGFilePath(szPsCFGBufPrev, MAX_PATH);

	if (FileExists(szPsCFGBufPrev))
	{
		if(FileExists(szPsCFGBuf)) ::DeleteFile(szPsCFGBuf);
		if(_trename(szPsCFGBufPrev, szPsCFGBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - CFG 백업 정책이 존재하여 복원함 : %s"), szPsCFGBuf);
	}

	// 2020.01.07, yoon
	// 하드웨어 디바이스 정책도 백업된 정책파일을 복원한다.

	TCHAR szHdBuf[MAX_PATH + 1] = {0, };
	TCHAR szHdBufPrev[MAX_PATH + 1] = {0, };
	GetHdPolicyFilePath(szHdBuf, MAX_PATH);
	GetHdPrevPolicyFilePath(szHdBufPrev, MAX_PATH);

	if(FileExists(szHdBufPrev))
	{
		if(FileExists(szHdBuf)) ::DeleteFile(szHdBuf);
		if(_trename(szHdBufPrev, szHdBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - 백업 정책이 존재하여 복원함 : %s"), szHdBuf);
	}

	TCHAR szHdGEDBuf[MAX_PATH + 1] = {0, };
	TCHAR szHdGEDBufPrev[MAX_PATH + 1] = {0, };
	GetHdPolicyGEDFilePath(szHdGEDBuf, MAX_PATH);
	GetHdPrevPolicyGEDFilePath(szHdGEDBufPrev, MAX_PATH);

	if(FileExists(szHdGEDBufPrev))
	{
		if(FileExists(szHdGEDBuf)) ::DeleteFile(szHdGEDBuf);
		if(_trename(szHdGEDBufPrev, szHdGEDBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - GED 백업 정책이 존재하여 복원함 : %s"), szHdGEDBuf);
	}

	TCHAR szHdCFGBuf[MAX_PATH + 1] = {0, };
	TCHAR szHdCFGBufPrev[MAX_PATH + 1] = {0, };
	GetHdPolicyCFGFilePath(szHdCFGBuf, MAX_PATH);
	GetHdPrevPolicyCFGFilePath(szHdCFGBufPrev, MAX_PATH);

	if (FileExists(szHdCFGBufPrev))
	{
		if(FileExists(szHdCFGBuf)) ::DeleteFile(szHdCFGBuf);
		if(_trename(szHdCFGBufPrev, szHdCFGBuf))
		{
			// 이름 변경 실패
		}

		WRITELOG(_T("RestorePolicyFile - CFG 백업 정책이 존재하여 복원함 : %s"), szHdCFGBuf);
	}
	return 0;
}

BOOL CClientManager::RestorePolicyFileOnPolicyError(CString &sReceived)
{
	WRITELOG(_T("[RestorePolicyFileOnPolicyError] 백업 정책 확인"));
	BOOL bRet = FALSE;
	RestorePolicyFile();

	CBMSConfiguration cConfiguration(BMS_DLP_POLICY_FILENAME);
	cConfiguration.Init();
	bRet = cConfiguration.ReadConfigFile();

	if (bRet)
	{
		cConfiguration.SetSeperator((TCHAR)_T(""));
		sReceived = cConfiguration.GetContents();
		bRet = TRUE;
		WRITELOG(_T("[RestorePolicyFileOnPolicyError] 백업 정책 사용\r\n%s"), sReceived);
	}
	else
	{
		WRITELOG(_T("[RestorePolicyFileOnPolicyError] OriginPolicy 확인"));

		TCHAR szLogPath[MAX_PATH] = {0, };
		GetAgentLogPath(szLogPath, MAX_PATH);

		CString sOriginPolicy = _T("");
		sOriginPolicy.Format(_T("%s%s\\OriginPolicy"), szLogPath, BM_RDATA);
		sReceived = MultiLanguageReadFile(sOriginPolicy, CHARSET_UTF8);

		if (!sReceived.IsEmpty())
		{
			bRet = TRUE;
			WRITELOG(_T("[RestorePolicyFileOnPolicyError] OriginPolicy 사용\r\n%s"), sReceived);
		}
	}
	
	return bRet;
}

BOOL CClientManager::ReMappingOrRemoveAgent()
{
	CString sResp = _T(""), sCmd = _T(""), sReq = _T("");

	sReq = m_sDuplicateAgentID;
	sReq += g_Separator;
	sReq += GetLoginName();
	sReq += g_Separator;
	sReq += GetPCName();
	sReq += g_Separator;
	sReq += GetAllIPaddress();
	sReq += g_Separator;
	sReq += GetAllMACAddress();

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSREG, sServerURL);
	WRITELOG(_T("[CClientManager::ReMappingOrDeleteAgent()] - m_stPageInfo.m_GSREG - %s"), m_stPageInfo.m_GSREG);

	sCmd.Format(_T("%s%s?cmd=18106"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSREG : sPageName);

	CStringW wsCmd = sCmd, wsReq = sReq;
	__int64 nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);

	if (sResp.CompareNoCase(_T("A")) == 0)
	{
		WRITELOG(_T("[ReMappingOrDeleteAgent] A 설치!!"));
		SendRegKey(m_sInstallmode);
	}
	else if (sResp.CompareNoCase(_T("R")) == 0)
	{
		WRITELOG(_T("[ReMappingOrDeleteAgent] R 대기!!"));
		g_pBMSCommAgentDlg->SetTimer(DUPLICATED_USER_TIMER_ID, 30*1000, NULL);
	}
	else if (sResp.CompareNoCase(_T("D")) == 0)
	{
		WRITELOG(_T("[ReMappingOrDeleteAgent] D!! Agent 삭제!"));
		AgentRemove(g_sBMSRemoveParam[BMS_REMOVE_FROM_REMAPPING]);
	}
	else
	{
		WRITELOG(_T("[ReMappingOrDeleteAgent] 서버 처리가 안되어 있음!! Resp : %s"), sResp);
		g_pBMSCommAgentDlg->m_pUserRegExDlg->MessageBox(LoadLanString(160), LoadLanString(186), MB_OK);
		g_pBMSCommAgentDlg->SetTimer(USER_REG_TIMER_ID, 10000, NULL);
	}

	return TRUE;
}

// end grman, 2007/10/02 
///////////////////////////////////////////////////////////////////////////////////////////////

// by grman, 2008/03/07
// RegKey를 전달하여 AgentID를 받아온다.
BOOL CClientManager::SendRegKey(CString sInstallMode)
{
	// 2008/03/07 현재, RegKey는 'loginid', 'pcname'만 존재함
	// 2008/08/25 RegKey가 'manual'인 경우에는 loginid, pcname, ipaddr, macaddr을 차례로 모두 전달한다.
	if(sInstallMode != BM_AGENT_AUTOREG_KEY_LOGINID && sInstallMode != BM_AGENT_AUTOREG_KEY_PCNAME 
		&& sInstallMode != BM_AGENT_AUTOREG_KEY_MANUAL && sInstallMode != BM_AGENT_AUTOREG_KEY_IPADDR
		&& sInstallMode != BM_AGENT_AUTOREG_KEY_MAC && sInstallMode != BM_AGENT_AUTOREG_KEY_EXTERNAL) return FALSE;

	TCHAR chSeparator = FIELD_SEPARATOR;

	CString sRegKey = _T(""), cmd = _T("");
	if(sInstallMode == BM_AGENT_AUTOREG_KEY_LOGINID)
	{
		sRegKey.Format(_T("%s%c%s%c%s"), GetLoginName(), g_Separator, (m_bDupPopUp?_T("SHOWCOMPLETE"):_T("")), g_Separator, GetAllMACAddress());
		cmd = _T("18103");
	}
	else if(sInstallMode == BM_AGENT_AUTOREG_KEY_PCNAME)
	{
		sRegKey.Format(_T("%s%c%s%c%s"), GetPCName(), g_Separator, (m_bDupPopUp?_T("SHOWCOMPLETE"):_T("")), g_Separator, GetAllMACAddress());
		cmd = _T("18103");
	}
	else if(sInstallMode == BM_AGENT_AUTOREG_KEY_IPADDR || sInstallMode == BM_AGENT_AUTOREG_KEY_MAC)
	{
		sRegKey.Format(_T("%s%c%s%c%s"), GetIPAddress(), g_Separator, (m_bDupPopUp?_T("SHOWCOMPLETE"):_T("")), g_Separator, GetAllMACAddress());
		cmd = _T("18103");
	}
	else if (sInstallMode == BM_AGENT_AUTOREG_KEY_EXTERNAL)
	{
		sRegKey.Format(_T("%s%c%s%c%s"), GetExternalData(), g_Separator, (m_bDupPopUp?_T("SHOWCOMPLETE"):_T("")), g_Separator, GetAllMACAddress());
		cmd = _T("18103");
	}
	else if(sInstallMode == BM_AGENT_AUTOREG_KEY_MANUAL)
	{
		// 순서 : loginid, pcname, ipaddr, macaddr, IDE Serial, all ip address
		sRegKey.Format(_T("%s%c%s%c%s%c%s%c%s%c%s"), 
			GetLoginName(),
			chSeparator,
			GetPCName(),
			chSeparator,
			GetIPAddress(),
			chSeparator,
			GetAllMACAddress(),
			chSeparator,
			GetIDESerial(),
			chSeparator,
			GetAllIPaddress()
			);
		cmd = _T("18102");
	}

	if(sRegKey.GetLength() == 0) return FALSE;

	UINT nSize = sInstallMode.GetLength();
	nSize += sRegKey.GetLength();
	nSize += 1; // separator

	CString sResp = _T(""), sCmd = _T(""), sReq = _T("");
	sReq += sInstallMode;
	sReq += g_Separator;
	sReq += sRegKey;

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSREG, sServerURL);
	WRITELOG(_T("[CClientManager::SendRegKey()] - m_stPageInfo.m_GSREG - %s"), m_stPageInfo.m_GSREG);

	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSREG : sPageName, cmd);

	CStringW wsCmd = sCmd, wsReq = sReq;
	__int64 nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);
	if (nRet != 0 || sResp.GetLength() == 0) // 매핑할 사용자가 없는 경우
	{
		WRITELOG(_T("[CClientManager] SendRegKey::SendAndReceive 실패 cmd : %s, nRet : %d"), cmd, nRet);

		/*
		ERR-1003 #찾는 데이터가 없음
		ERR-1004 #
		ERR-1005 #1차 매핑 실패, 매핑 재시도
		ERR-1014 #매핑되는 에이전트 없음 (ERR-1014 #매핑되는 에이전트 없음-ipaddr)
		ERR-1015 #매핑되는 에이전트 둘 이상

		// pcname, ipaddr, macaddr, loginid
		*/

		g_pBMSCommAgentDlg->KillTimer(USER_REG_TIMER_ID);

		CString sInfo = _T(""), sMsg = _T("");
		sInfo = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_MAPPING_FAIL_MSG, BMS_MNG_CUSTOMLIST);
		sMsg.Format(_T("%s\r\nReason : "), (sInfo == _T("")? LoadLanString(211) : sInfo));

		if (sResp.Find(_T("ERR-1014")) > -1)
		{
			sMsg.AppendFormat(_T("%s (%s)"), LoadLanString(209), sInstallMode);
			MessageBox(NULL, sMsg, LoadLanString(208), MB_ICONWARNING);
		}

		if (sResp.Find(_T("ERR-1015")) > -1)
		{
			sMsg.AppendFormat(_T("%s (%s)"), LoadLanString(210), sInstallMode);
			MessageBox(NULL, sMsg, LoadLanString(208), MB_ICONWARNING);
		}
		g_pBMSCommAgentDlg->SetTimer(USER_REG_TIMER_ID, 10000, NULL);


		// by grman, 2010/10/14
		if (cmd == _T("18103"))
		{
			if (sResp.Find(_T("ERR-1003")) > -1)
			{
				WRITELOG(_T("[CClientManager] 찾는 데이터가 없음!! 사용자 이름 등록창 생성!!"));
				g_pBMSCommAgentDlg->KillTimer(USER_REG_TIMER_ID);
				m_sInstallmode = _T("");
				PostMessage(g_pBMSCommAgentDlg->m_hWnd, BMSM_USER_REG_DIALOG_DO, NULL, NULL);
			}
			else if (sResp.Find(_T("ERR-1004")) > -1)
			{
				WRITELOG(_T("[CClientManager] 수동 전환으로 변경"));
				m_sInstallmode = _T("manual");
			}
			else if (sResp.Find(_T("ERR-1005")) > -1)
			{
				WRITELOG(_T("[CClientManager] 1차 매핑 실패 후 재시도 진행!!"));
			}
		}

		return FALSE;
	}

	// 2011/05/24, avilon
	CString sDupFlag = _T("");
	AfxExtractSubString(sDupFlag, sResp, 0, chSeparator);
	// 다중 설치 여부 확인(SubPC), 다중 설치 안함일 경우 삭제 진행 AgentRemove
	// 2010.05.28	HANGEBS
	//if (sResp.Find("18105") > -1)
	// 2011/06/07, avilon
	// Agent ID가 18105인 경우가 발생할 수 있어 조건을 추가한다.
	CString sMainID = _T("");
	AfxExtractSubString(sMainID, sResp, 1, chSeparator);
	if ((sDupFlag.CompareNoCase(_T("18105")) == 0) && (_ttoi(sMainID) > 0))
	{
		g_pBMSCommAgentDlg->KillTimer(USER_REG_TIMER_ID);

		CString sVisbleAlert = _T("");
		BOOL bVisbleAlert = TRUE;
		AfxExtractSubString(sVisbleAlert, sResp, 2, g_Separator);
		if(!sVisbleAlert.IsEmpty())
		{
			WRITELOG(_T("[SendRegKey] 다중 PC 진행창 표시 값(%s)"), sVisbleAlert);
			if(!sVisbleAlert.CompareNoCase(_T("1")))
			{
				bVisbleAlert = FALSE;
			}
		}
		WRITELOG(_T("[SendRegKey] 다중 PC 진행창 표시 여부(%s)"), (bVisbleAlert) ? _T("TRUE") : _T("FALSE"));
		if (bVisbleAlert == TRUE)
		{
			if (::MessageBox(NULL, LoadLanString(200), LoadLanString(201), MB_YESNO) == IDNO)
			{
				AgentRemove(g_sBMSRemoveParam[BMS_REMOVE_FROM_MULTIUSER_REG_CANCEL]);
				return FALSE;
			}
		}

		WRITELOG(_T("## 다중 PC 사용자 설치 진행!"));
		CString sTemp = _T(""), sMainAgentID = _T("");
		AfxExtractSubString(sTemp, sResp, 1, g_Separator);
		sMainAgentID.Format(_T("%s%c"), sTemp, g_Separator);			
		sCmd.Format(_T("%s%s?cmd=18105"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSREG : sPageName);//m_stPageInfo.m_GSREG
		wsCmd = sCmd;
		wsReq = sMainAgentID;
		sResp = _T("");
		nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);
		if (nRet != 0 || sResp.GetLength() == 0 || sResp.Find(_T("ERR-")) > -1)
		{
			::MessageBox(NULL, LoadLanString(202), LoadLanString(203), MB_OK);
			AgentRemove(g_sBMSRemoveParam[BMS_REMOVE_FROM_MULTIUSER_REG_FAIL]);
			return FALSE;
		}
		WRITELOG(_T("## 다중 설치 내용 - %s"), sResp);
	}
	// 2011/05/24, avilon
	// 중복 알림창 생성
	else if(sDupFlag.CompareNoCase(_T("ALERT")) == 0)
	{
		g_pBMSCommAgentDlg->KillTimer(USER_REG_TIMER_ID);
		m_bDupPopUp = TRUE;
		CString sPopUpContents=_T(""), sWidth=_T(""), sHeight=_T("");
		AfxExtractSubString(sWidth, sResp, 1, chSeparator);
		AfxExtractSubString(sHeight, sResp, 2, chSeparator);
		AfxExtractSubString(sPopUpContents, sResp, 3, chSeparator);
		AfxExtractSubString(m_sDuplicateAgentID, sResp, 4, chSeparator);
		CHtmlViewDlg cHtmlDlg;
		cHtmlDlg.m_LanID = m_LangID;
		cHtmlDlg.m_sContentsUrl = sPopUpContents;
		cHtmlDlg.m_nWidth = _ttoi(sWidth);
		cHtmlDlg.m_nHeight = _ttoi(sHeight);
		cHtmlDlg.DoModal();

		g_pBMSCommAgentDlg->KillTimer(USER_REG_TIMER_ID);
		ReMappingOrRemoveAgent();

		//g_pBMSCommAgentDlg->SetTimer(USER_REG_TIMER_ID, 10000, NULL);
		return FALSE;
	}
	else
	{
		CString sShowCompletePopUp;
		if(AfxExtractSubString(sShowCompletePopUp, sResp, 3, g_Separator))
		{
			WRITELOG(_T("sShowCompletePopUp - %s"), sShowCompletePopUp);
			if(sShowCompletePopUp.CompareNoCase(_T("SHOWCOMPLETE")) == 0)
			{
				g_pBMSCommAgentDlg->KillTimer(USER_REG_TIMER_ID);
				MessageBox(NULL, LoadLanString(204), _T(""), MB_OK);
			}
		}
	}

	CString agentid = _T(""), deptname = _T(""), username = _T("");
	AfxExtractSubString(agentid, sResp, 0, g_Separator);
	AfxExtractSubString(deptname, sResp, 1, g_Separator);
	AfxExtractSubString(username, sResp, 2, g_Separator);

	// 2011/12/28, avilon
	if (agentid.GetLength() > 10)
	{
		WRITELOG(_T("[CClientManager] SendRegKey - Agent ID가 이상하게 전달 됨. agentid=%s"), agentid);
		return FALSE;
	}

	CString sMacAddress = GetAllMACAddress();

	// 2009/08/24 avilon, m_sAgentID가 비어 있어서 메모리 에러가 발생하여 agentid로 바꿈.
	nSize = agentid.GetLength()+sMacAddress.GetLength()+1;
	TCHAR *pTempBuf = NULL;
	pTempBuf = (TCHAR*)malloc(nSize*sizeof(TCHAR) + sizeof(TCHAR));
	ZeroMemory(pTempBuf, nSize*sizeof(TCHAR)+sizeof(TCHAR));

	_sntprintf(pTempBuf, nSize, _T("%s%c%s"), agentid, g_Separator, sMacAddress);

	sResp = _T("");
	sReq = _T("");

	if(pTempBuf)
	{
		sReq = pTempBuf;
		free(pTempBuf); pTempBuf = NULL;
	}

	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSREG : sPageName, _T("18100"));
	wsCmd = sCmd;
	wsReq = sReq;
	nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);
	if (nRet == BM_SERVER_ERR_INSTALLED_USER)
	{
		WRITELOG(_T("[CClientManager] SendRegKey::SendAndReceive 실패. 중복 설치 방지."));
		m_sDuplicateAgentID = agentid;
		g_pBMSCommAgentDlg->KillTimer(USER_REG_TIMER_ID);
		ReMappingOrRemoveAgent();

		return FALSE;
	}
	else if (nRet != 0)
	{
		WRITELOG(_T("[CClientManager] SendRegKey::SendAndReceive 실패 cmd : 18100"));
		return FALSE;
	}
	else
	{
		m_bNeedAgentID = FALSE; m_sAgentID = agentid;
		if(g_pBMSCommAgentDlg->SetTimer(MONITORING_TIMER_ID, 10000, NULL))
			g_pBMSCommAgentDlg->m_bMonitoringTimer = TRUE;
 		g_pBMSCommAgentDlg->KillTimer(USER_REG_TIMER_ID);
	}

	// 2011/03/29, avilon
	CString sAgentIdFileName = _T("");
	sAgentIdFileName.Format(_T("%stmp\\SID_%s"), g_pBMSCommAgentDlg->m_sBMSDATLogPath, agentid);
	HANDLE hAID = NULL;
	hAID = CreateFile(sAgentIdFileName, FILE_ALL_ACCESS, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hAID != INVALID_HANDLE_VALUE)
		CloseHandle(hAID);

	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTID, agentid, _T(""));
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_DEPTNAME, deptname, _T(""));
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTNAME, username, _T(""));

	// 2011/03/03 by realhan CC 모듈 수정
	if (m_bIsLogEncoded == FALSE)
	{
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTID, agentid);
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTNAME, username);
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_DEPTNAME, deptname);
	}

	return TRUE;
}

// by realhan
CString CClientManager::SendUserNameToServer(LPCTSTR UserName, DWORD nLen, BOOL bCreateSubpcAutomatic)
{
	WRITELOG(_T("[CClientManager::SendUserNameToServer]"));
	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSREG, sServerURL);
	WRITELOG(_T("[CClientManager::SendUserNameToServer] - m_stPageInfo.m_GSREG - %s"), m_stPageInfo.m_GSREG);


	CString sResp = _T(""), sReq = UserName;
	CString sCmd = _T("");
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSREG : sPageName, _T("18101"));

	if (bCreateSubpcAutomatic == TRUE)
	{
		sReq.AppendFormat(_T("%c%s"), (TCHAR)FIELD_SEPARATOR, _T("CREATE_SUBPC_AUTOMATIC"));
	}

	CStringW wsCmd = sCmd, wsReq = sReq;
	int ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);
	if (ret) return _T("");
	return sResp;
}

BOOL CClientManager::RegistAgentID()
{
	CString sResp = _T(""), sReq = _T("REQUEST_CONFIG"), sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::RegistAgentID] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18000"));  // 안쓰는것 같다. by grman, 2009/07/17

	CStringW wsCmd = sCmd, wsReq = sReq;
	int nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);

	if ((nRet != 0) || (sResp.IsEmpty() == TRUE))
	{
		WRITELOG(_T("[CClientManager::RegistAgentID] SendAndReceive Error : %d, sResp : %s"), nRet, sResp);
		return FALSE;
	}

	CString Installmode = _T(""), vn = _T("");

	AfxExtractSubString(Installmode, sResp, 0, g_Separator);
	AfxExtractSubString(vn, sResp, 1, g_Separator);
	AfxExtractSubString(m_server_upgrade_URL, sResp, 2, g_Separator);

	if (m_bMultiURL)
	{
		int nEPos = sServerURL.Find(_T("server/"));
		if (nEPos > -1)
		{
			m_server_upgrade_URL = sServerURL.Left(nEPos) + _T("module/ver.txt");
			WRITELOG(_T("[RegistAgentID] Multi URL upgrade URL : %s"), m_server_upgrade_URL);
		}
	}

	CString sLocalVersion = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_CVN, _T(""));	

	int nSVer = 0, nCVer = 0;
	nSVer = Str2Int(NumFilter(vn));
	nCVer = Str2Int(NumFilter(sLocalVersion));

	// Version 비교
	if (nSVer > nCVer)
	{
		if (IsErrorCertificate() == FALSE)
		{
			AgentUpgrade();
			return TRUE;
		}
		else
		{
			WRITELOG(_T("[RegistAgentID] 업그레이드 상황이지만 인증서 오류 파일을 확인했다. 업그레이드를 진행하지 않는다."));
		}
	}

	if(!Installmode.CompareNoCase(BM_AGENT_AUTOREG_KEY_LOGINID)
		|| !Installmode.CompareNoCase(BM_AGENT_AUTOREG_KEY_PCNAME)
		|| !Installmode.CompareNoCase(BM_AGENT_AUTOREG_KEY_IPADDR)
		|| !Installmode.CompareNoCase(BM_AGENT_AUTOREG_KEY_MANUAL)
		|| !Installmode.CompareNoCase(BM_AGENT_AUTOREG_KEY_MAC)
		|| !Installmode.CompareNoCase(BM_AGENT_AUTOREG_KEY_EXTERNAL)
		)
	{
		m_sInstallmode = Installmode;
		SetTimer(g_pBMSCommAgentDlg->m_hWnd, USER_REG_TIMER_ID, 10000, NULL);
	}
	else
	{
		WRITELOG(_T("[CClientManager::RegistAgentID] # 사용자 등록창 생성 #"));
		PostMessage(g_pBMSCommAgentDlg->m_hWnd, BMSM_USER_REG_DIALOG_DO, NULL, NULL);
	}

	return TRUE;
}

CString CClientManager::AddGetMethodParameter(LPCTSTR sName, LPCTSTR sValue, BOOL bAddAmpersand)
{
	CString sBuf;
	tstring sValueBuf;

	CHAR *pszUTF8 = NULL;
	if(m_bConnectUTF8)
	{
		WRITELOG(_T("[AddGetMethodParameter] UTF8"));
		int nLen = WideCharToMultiByte(CP_UTF8, 0, sValue, _tcslen(sValue), NULL, 0, NULL, NULL);
		pszUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_UTF8, 0, sValue, _tcslen(sValue), pszUTF8, nLen, NULL, NULL);
		pszUTF8[nLen] = NULL;
	}
	else
	{
		WRITELOG(_T("[AddGetMethodParameter] NOT UTF8"));
		int nLen = WideCharToMultiByte(CP_ACP, 0, sValue, _tcslen(sValue), NULL, 0, NULL, NULL);
		pszUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_ACP, 0, sValue, _tcslen(sValue), pszUTF8, nLen, NULL, NULL);
		pszUTF8[nLen] = NULL;
	}

	CStringA sTempReq = pszUTF8;
	if (pszUTF8)
		delete [] pszUTF8; pszUTF8 = NULL;

	sValueBuf = UriEncode(sTempReq.GetBuffer());
	if(!sValueBuf.length()) return _T("");
	sBuf.Format(_T("%s=%s"), sName, sValueBuf.c_str());
	if(bAddAmpersand) sBuf += _T("&");
	return sBuf;
}

CString CClientManager::AddGetMethodParameter(LPCTSTR sName, DWORD dwValue, BOOL bAddAmpersand)
{
	CString sBuf;
	TCHAR buf[1024];
	if(_itot_s(dwValue, buf, 1024, 10)) return _T("");
	tstring sValueBuf;

	CHAR *pszUTF8 = NULL;
	if(m_bConnectUTF8)
	{
		int nLen = WideCharToMultiByte(CP_UTF8, 0, buf, _tcslen(buf), NULL, 0, NULL, NULL);
		pszUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_UTF8, 0, buf, _tcslen(buf), pszUTF8, nLen, NULL, NULL);
		pszUTF8[nLen] = NULL;
	}
	else
	{
		int nLen = WideCharToMultiByte(CP_ACP, 0, buf, _tcslen(buf), NULL, 0, NULL, NULL);
		pszUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_ACP, 0, buf, _tcslen(buf), pszUTF8, nLen, NULL, NULL);
		pszUTF8[nLen] = NULL;
	}

	CStringA sTempReq = pszUTF8;

	if (pszUTF8)
		delete [] pszUTF8; pszUTF8 = NULL;

	sValueBuf = UriEncode(sTempReq.GetBuffer());
	if(!sValueBuf.length()) return _T("");
	sBuf.Format(_T("%s=%s"), sName, sValueBuf.c_str());
	if(bAddAmpersand) sBuf += _T("&");
	return sBuf;
}

// CString CClientManager::ReadBMSManageListFile(CString sMNGList)
// {
// 	CFile cf;
// 	CString sFileContent;
// 	CString sFilePath;
// 	UINT uFileLength;
// 	
// 	char sAgentPath[MAX_PATH];
// 	GetAgentPath(sAgentPath, MAX_PATH);
// 	sFilePath.Format("%s%s", sAgentPath, BMS_MNG_FILENAME);
// 
// 	if (cf.Open(sFilePath, CFile::modeRead))
// 	{
// 		uFileLength = (UINT)cf.GetLength();	
// 		cf.Read(sFileContent.GetBuffer(uFileLength/sizeof(TCHAR)), uFileLength);	
// 		sFileContent.ReleaseBuffer(uFileLength/sizeof(TCHAR));
// 		cf.Close();
// 	}
// 
// 	if (sFileContent.GetLength() == 0)
// 	{
// 		WRITELOG("## .ini에서 읽어온 내용이 없음!");
// 		return "";
// 	}
// 
// 	CString sContent="";
// 	int iSPos, iEPos;
// 	iSPos = sFileContent.Find(sMNGList);
// 	if (iSPos == -1)  return "";
// 
// 	iEPos = sFileContent.Find(BMS_MNG_ENDLIST, iSPos);
// 
// 	if (iEPos != -1)
// 	{		
// 		iSPos = sFileContent.Find("\r\n", iSPos);
// 		iSPos += 2;
// 		sContent = sFileContent.Mid(iSPos, iEPos - iSPos);		
// 	}
// 
// 	return sContent;
// }

BOOL CClientManager::SetProcessList(CString sContent)
{
	int iEPos;

	if (sContent.GetLength() != 0)
	{
		CString sTemp;

		while (true)
		{
			iEPos = sContent.Find(_T("\r\n"));
			sTemp = sContent.Left(iEPos);
			sContent.Delete(0,iEPos+2);
			if (sTemp.GetLength() ==0)
			{
				break;
			}
			CString sPrcs, sWnd, sMode;
			AfxExtractSubString(sPrcs, sTemp, 0, _T('|'));
			AfxExtractSubString(sWnd, sTemp, 1, _T('|'));
			AfxExtractSubString(sMode, sTemp, 2, _T('|'));

			if (sPrcs.Find(_T("BMSCommAgent.exe")) == -1)
			{
				PPRCLIST prc = new PRCLIST;
				prc->sAgentProcess = sPrcs;
				prc->sAgentWnds = sWnd;
				prc->sRunMode = sMode;
				prc->bAgentEnable = TRUE;

				m_prcList.AddTail(prc);
				WRITELOG(_T("[CClientManager] ADD ProcessList : %s - %s"), sPrcs, sWnd);
			}			
		}		
		return TRUE;
	}

	return FALSE;
}

void CClientManager::WriteFilePolicyUpdateTime(CString sUpdateFileName)
{
	CString sFilePath;
	sFilePath.Format(_T("%s%s"), g_pBMSCommAgentDlg->m_sMainAgentPath, sUpdateFileName);//LAST_POLICY_UPDATE_FILENAME);
	WRITELOG(_T("[WriteFilePolicyUpdateTime] GetDateTime().c_str Before!! sFilePath : %s"), sFilePath);
	CString sDateTime = GetDateTime().c_str();
	WRITELOG(_T("[WriteFilePolicyUpdateTime] GetDateTime().c_str After sDateTime : %s Error : %d"), sDateTime, GetLastError());
	BOOL bRet = MultiLanguageWriteFile(sFilePath, sDateTime, CHARSET_UNICODE);
	WRITELOG(_T("[MultiLanguageWriteFile] bRet : %d, Error : %d"), bRet, GetLastError());
}

void CClientManager::FindAgentANDSendPostMSG(BOOL bOffline)
{
	if (g_pBMSCommAgentDlg->m_bIsMainSession)
	{
		WriteFilePolicyUpdateTime(LAST_POLICY_UPDATE_FILENAME);
		g_pBMSCommAgentDlg->UpdatePolicyFileFromMainSession();
		g_pBMSCommAgentDlg->UpdateFileTempPolicyFileFromMainSession();
		g_pBMSCommAgentDlg->RefreshTrayMsgFromMainSession();
	}

	POSITION pos = m_prcList.GetHeadPosition();
	while (pos)
	{
		PPRCLIST prc = (PPRCLIST)m_prcList.GetAt(pos);
		if (prc)
		{
			HWND hWnd = ::FindWindow(_T("#32770"), prc->sAgentWnds);
			if(hWnd != NULL)
			{
				while(! ::PostMessage(hWnd, BMSM_UPDATE_POLICIES, (WPARAM)bOffline, (LPARAM)NULL))
				{
					WRITELOG(_T("정책 업데이트 실패!! - %s - 0x%x"), prc->sAgentWnds, GetLastError());
					ProcMessage(); 
				}
			}
			else
			{
				hWnd = ::FindWindow(NULL, prc->sAgentWnds);
				if(hWnd != NULL)
				{
					while(! ::PostMessage(hWnd, BMSM_UPDATE_POLICIES, (WPARAM)bOffline, (LPARAM)NULL))
					{
						WRITELOG(_T("정책 업데이트 실패2!! - %s - 0x%x"), prc->sAgentWnds, GetLastError());
						ProcMessage();
					}
				}
			}
		}
		m_prcList.GetNext(pos);
	}
}

// 2012/06/21, avilon
#include "BMSDeviceTempPolicy.h"
#include "BMSFileTempPolicy.h"
#include "BMSAppTempPolicy.h"
#include "BMSWebTempPolicy.h"
void CClientManager::SendFileTempPolicyUpdateMSG()
{
	WRITELOG(_T("SendFileTempPolicyUpdateMSG - Start"));
	if (g_pBMSCommAgentDlg->m_bIsMainSession)
	{
		WriteFilePolicyUpdateTime(LAST_FILETEMP_POLICY_UPDATE_FILENAME);
	}

	POSITION pos = m_prcList.GetHeadPosition();
	int nTestCnt = 0;
	while (pos)
	{
		nTestCnt++;
		WRITELOG(_T("[SendFileTempPolicyUpdateMSG] nTestCnt : %d"), nTestCnt);
		PPRCLIST prc = (PPRCLIST)m_prcList.GetAt(pos);
		if (prc)
		{
			WRITELOG(_T("[SendFileTempPolicyUpdateMSG] prc->sAgentWnds : %s"), prc->sAgentWnds);
			HWND hWnd = ::FindWindow(_T("#32770"), prc->sAgentWnds);
			if(hWnd != NULL)
			{
				while(! ::PostMessage(hWnd, BMSM_UPDATE_FILE_TEMP_LIST, (WPARAM)NULL, (LPARAM)NULL))
					ProcMessage(); 
			}
		}
		m_prcList.GetNext(pos);
	}
	WRITELOG(_T("SendFileTempPolicyUpdateMSG - End"));
}

void CClientManager::SendDevTempPolicyUpdateMSG()
{
	WRITELOG(_T("SendDevTempPolicyUpdateMSG - Start"));
	HWND hWnd = ::FindWindow(_T("#32770"), BM_FILEAGENT_WND_TITLE);
	if(hWnd != NULL)
	{
		while(! ::PostMessage(hWnd, BMSM_UPDATE_POLICIES, (WPARAM)NULL, (LPARAM)NULL))
			ProcMessage(); 
	}
	WRITELOG(_T("SendDevTempPolicyUpdateMSG - End"));
}

void CClientManager::SendAppTempPolicyUpdateMSG()
{
	WRITELOG(_T("SendAppTempPolicyUpdateMSG - Start"));
	HWND hWnd = ::FindWindow(_T("#32770"), BM_APPAGENT_WND_TITLE);
	if(hWnd != NULL)
	{
		while(! ::PostMessage(hWnd, BMSM_UPDATE_APP_TEMP_POLICY, (WPARAM)NULL, (LPARAM)NULL))
			ProcMessage(); 
	}

	hWnd = ::FindWindow(_T("#32770"), BM_APPAGENT_x64_WND_TITLE);
	if(hWnd != NULL)
	{
		while(! ::PostMessage(hWnd, BMSM_UPDATE_APP_TEMP_POLICY, (WPARAM)NULL, (LPARAM)NULL))
			ProcMessage(); 
	}
	WRITELOG(_T("SendAppTempPolicyUpdateMSG - End"));
}

void CClientManager::SendWebTempPolicyUpdateMSG()
{
	WRITELOG(_T("SendWebTempPolicyUpdateMSG - Start"));

	HWND hWnd = ::FindWindow(_T("#32770"), BM_APPAGENT_WND_TITLE);
	if(hWnd != NULL)
	{
		while(! ::PostMessage(hWnd, BMSM_UPDATE_WEB_TEMP_POLICY, (WPARAM)NULL, (LPARAM)NULL))
			ProcMessage(); 
	}

	hWnd = ::FindWindow(_T("#32770"), BM_APPAGENT_x64_WND_TITLE);
	if(hWnd != NULL)
	{
		while(! ::PostMessage(hWnd, BMSM_UPDATE_WEB_TEMP_POLICY, (WPARAM)NULL, (LPARAM)NULL))
			ProcMessage(); 
	}

	hWnd = ::FindWindow(_T("#32770"), BM_MMAGENT_WND_TITLE);
	if(hWnd != NULL)
	{
		while(! ::PostMessage(hWnd, BMSM_UPDATE_WEB_TEMP_POLICY, (WPARAM)NULL, (LPARAM)NULL))
			ProcMessage(); 
	}

	hWnd = ::FindWindow(_T("#32770"), BM_MMAGENT_x64_WND_TITLE);
	if(hWnd != NULL)
	{
		while(! ::PostMessage(hWnd, BMSM_UPDATE_WEB_TEMP_POLICY, (WPARAM)NULL, (LPARAM)NULL))
			ProcMessage(); 
	}

	WRITELOG(_T("SendWebTempPolicyUpdateMSG - End"));
}

/////////////////////////////////////////////////////////////////////////////////////////////
// by grman, 2009/05/14
#include <afxinet.h>
void CClientManager::SetSendLogDate(int d)
{
	CTime c_date = CTime::GetCurrentTime();
	CTimeSpan aday(d,0,0,0);
	c_date += aday;
	CString sCurrentTime = c_date.Format(_T("%Y%m%d"));
	if (FileExistRegInfo() == TRUE)
	{
		WRITELOG(_T("[SetSendLogDate] RegInfo 파일 발견. REGINFO_STRING_SITENAME"));
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_REGINFO_FILENAME, BMS_REGINFO_LOGDATE, sCurrentTime, _T(""));
	}
	else
	{
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REGINFO_STRING_SENDLOGDATE, sCurrentTime);
	}
}

CString GetIPAddressFromDomainName(CString strHost)
{
	CString rstr = _T("");
	HOSTENT *host;
	USES_CONVERSION;
	host = gethostbyname(W2A(strHost));
	if(host)
	{
		struct in_addr *ptr;

		ptr = ( struct in_addr * )host->h_addr_list[ 0 ];

		// Get IP
		int a = ptr->S_un.S_un_b.s_b1;
		int b = ptr->S_un.S_un_b.s_b2;
		int c = ptr->S_un.S_un_b.s_b3;
		int d = ptr->S_un.S_un_b.s_b4;

		rstr.Format(_T("%d.%d.%d.%d"), a,b,c,d);
	}

	return rstr;
}

void CClientManager::SendExecutionLog()
{
	// 2010/10/12, avilon - agentID를 얻어 오지 못 한 경우 agentinfo에서 다시 읽어 온다.
	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::SendExecutionLog] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::SendExecutionLog] - m_sAgentID = %s"), m_sAgentID);
	}
	// 0.정보를 검사한다.
	if(m_sAgentID.GetLength() == 0) return;
	if(m_wshttpServerUrl.GetLength() == 0) return;
	if(m_sProjectName.GetLength() == 0) return;
	
	CString sLogDate = _T("");
	if (FileExistRegInfo() == TRUE)
	{
		WRITELOG(_T("[CClientManager::SendExecutionLog] RegInfo 파일 발견. REGINFO_STRING_SITENAME"));
		CString sReginfoValue = _T("");
		sReginfoValue = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_REGINFO_FILENAME, BMS_REGINFO_LOGDATE, _T(""));

		sLogDate = sReginfoValue;
		sLogDate = sLogDate.Trim();
	}
	else
	{
		TCHAR lpBuf[100] = {0, };
		RegGetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_SENDLOGDATE, lpBuf, 100);
		sLogDate = lpBuf;
		sLogDate = sLogDate.Trim();
	}

	// 2.레지스트리 내용이 없다면 1일 후로 세팅한다.
	if(sLogDate.GetLength() != 8)
	{
		SetSendLogDate(1);
		return;
	}

	// 3.레지스트리 내용이 있고 오늘 날짜보다 작거나 같으면 로그를 보낸다.
	BOOL bResult = FALSE;
	CString str_today = CTime::GetCurrentTime().Format(_T("%Y%m%d"));
	if(str_today >= sLogDate)
	{
		// 2010/06/23, avilon
		CString sPCID = GetIDESerial();

		//TCHAR cSep = 0x11;
		CString bm_addr = GetIPAddressFromDomainName(_T("www.bluemoonsoft.com"));
		bm_addr = _T("www.bluemoonsoft.com");
		CString bm_url;
		bm_url.Format(_T("/aglog/aglog.jsp?INFO=%s%c%s%c%s%c%s%c%s%c%s%c%s"),
			m_sProjectName,
			g_Separator,
			//CUtils::WideToString(m_wshttpServerUrl).c_str(),
			m_wshttpServerUrl,
			g_Separator,
			GetIPAddress(),
			g_Separator,
			GetMACAddress(),
			g_Separator,
			m_sAgentID,
			g_Separator,
			m_sSiteName,
			g_Separator,
			sPCID);
		INTERNET_PORT nPort = 80;
		CInternetSession iSession;
		CHttpConnection *hc = iSession.GetHttpConnection(bm_addr, INTERNET_FLAG_TRANSFER_ASCII, nPort);
		if (hc) 
		{
			CHttpFile *hf = hc->OpenRequest(CHttpConnection::HTTP_VERB_POST, bm_url, NULL, 
				1, NULL, _T("HTTP/1.1"), INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_NO_AUTO_REDIRECT);
			if (hf) 
			{
				try 
				{
					if (hf->SendRequest(NULL, 0, NULL, NULL))
					{
						DWORD dwRet = 0;
						hf->QueryInfoStatusCode(dwRet);
						WRITELOG(_T("[SendExecutionLog] ret=%d"), dwRet);
						if (dwRet == 200)
						{
							bResult= TRUE;
						}
					}
				} 
				catch(CInternetException* e) 
				{
				}
				hf->Close();
				delete hf;
			}
			hc->Close();
			delete hc;
		}
	}

	// 4.로그 전송 후 날짜를 1주일 후로 세팅한다.
	if (bResult == TRUE)
	{
		SetSendLogDate(7);
	}
}

int CClientManager::RequestAllowList(LPCTSTR agentid, BOOL bAgent)
{
	BOOL bNewCmd = TRUE;
	int nResult = 0;
	CString sReq = agentid, sResp = _T(""), sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSALLOW, sServerURL);

	WRITELOG(_T("[CClientManager::RequestAllowList] Start agentid : %s"), agentid);
	WRITELOG(_T("[CClientManager::RequestAllowList] m_stPageInfo.m_GSALLOW : %s"), m_stPageInfo.m_GSALLOW);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSALLOW : sPageName, (bAgent) ? _T("18216") : _T("18204"));
	WRITELOG(_T("[CClientManager::RequestAllowList] sCmd : %s, sReq : %s"), sCmd, sReq);

	CStringW wsCmd = sCmd, wsReq = sReq;
	int ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);
	
	// 7.2 패키지부터 18216 추가되었으며, 기존에 사용하던 18203은 추후에 제거 필요
	if (ret == BM_SERVER_ERR_NOT_SUPPORT_CMD)
	{
		WRITELOG(_T("[CClientManager::RequestAllowList] BM_SERVER_ERR_NOT_SUPPORT_CMD"), sCmd, sReq);

		bNewCmd = FALSE;
		sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSALLOW : sPageName, (bAgent) ? _T("18203") : _T("18204"));
		CStringW wsCmd = sCmd, wsReq = sReq;
		sResp = _T("");
		ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);
		if ((ret != 0)) 
		{
			WRITELOG(_T("[CClientManager::RequestAllowList] RequestAllowList Error : %d"), ret);
			return ret;
		}
	}
	else if (ret == BM_SERVER_ERR_LIST_NOT_EXIST) 
	{
		// 18216에서만 BM_SERVER_ERR_LIST_NOT_EXIST 받고 있음
		// 다른 command에서도 BM_SERVER_ERR_LIST_NOT_EXIST 처리 필요해보임 (서버개발필요)

		WRITELOG(_T("[CClientManager::RequestAllowList] 목록이 존재하지 않는다."));
		sResp = _T("");
	}

	WRITELOG(_T("[CClientManager::RequestAllowList] RequestAllowList sResp : %s"), sResp);

	// 0 : table_id, 5 : 허용시간
	CString sResult = sResp;

	if (bAgent)
	{
		// 에이전트 임시정책 목록 요청에서만 응답 값 유효성 검사하고 있음
		if (ret != BM_SERVER_ERR_LIST_NOT_EXIST)
		{
			CString sSub = _T("");
			int nSepCount = 0;
			while (AfxExtractSubString(sSub, sResult, nSepCount, g_Separator))
			{
				nSepCount++;
				if (nSepCount > 10)
					break;
			}

			if (nSepCount < 10)
			{
				WRITELOG(_T("[CClientManager::RequestAllowList] 서버 에러!!  Error : %d"),  GetLastError());
				WRITELOG(_T("[CClientManager::RequestAllowList] JSon로그인지 확인"));

				CJSonDataParser	cJSonDataParser;
				if (!cJSonDataParser.SetParsingData2(sResult))
				{
					return ret;
				}
				WRITELOG(_T("[CClientManager::RequestAllowList] JSon로그로 진행"));
			}
		}

		/*
		0~4		TableID◀에이전트ID◀에이전트이름◀임시정책요청시간◀요청내용◀
		5~9		Reqaction◀Reqtag◀요청기간◀State◀승인/거절시간◀
		10~14	승인자ID◀승인자이름◀허용기간(Allow_Term)◀Allowdesc◀managerid◀
		15~19	managerName◀ReqReaction◀EndTime◀요청기간단위◀허용기간단위(Allow_TimeType)◀
		20~25	파일별임시정책에서사용
		20~24	세부사항(폴더경로)◀요청대상(파일명)◀파일생성일◀파일수정일◀파일크기◀
		25~29	파일해시◀사용자기반ID◀매니저 파일 다운로드 가능하면 T◀요청한 종료일 지정◀승인된 종료일 지정◀
		30		applybymanager
		*/

		if ((bNewCmd && !sResp.IsEmpty()) || (!bNewCmd && !ret))
		{
			WRITELOG(_T("[CClientManager] RequestAllowList agent 이다."));
			sResult = ExtractAllowData(sResp);
		}

		// 2012/06/21, avilon
		BOOL bExistFileTempList = FALSE, bExistDevTempList = FALSE, bExistAppTempList = FALSE, bExistWebTempList = FALSE;

		bExistFileTempList = WriteInstantItemList(sResult, REQ_TYPE_FILE);
		bExistDevTempList = WriteInstantItemList(sResult, REQ_TYPE_DEV);
		bExistAppTempList = WriteInstantItemList(sResult, REQ_TYPE_APP);
		bExistWebTempList = WriteInstantItemList(sResult, REQ_TYPE_WEB);

		if (bExistFileTempList == TRUE)		SendFileTempPolicyUpdateMSG();
		if (bExistDevTempList == TRUE)		SendDevTempPolicyUpdateMSG();
		if (bExistAppTempList == TRUE)		SendAppTempPolicyUpdateMSG();
		if (bExistWebTempList == TRUE)		SendWebTempPolicyUpdateMSG();
	}

	nResult = WriteAllowData(sResult, bAgent);
	if (nResult > 0)
	{
		WRITELOG(_T("[CClientManager::RequestAllowList] WriteAllowReqData FALSE"));
		nResult = -2;
		return nResult;
	}
	else if (ret == BM_SERVER_ERR_LIST_NOT_EXIST)
	{
		WRITELOG(_T("[CClientManager::RequestAllowList] BM_SERVER_ERR_LIST_NOT_EXIST"));
		return 0;
	}

	WRITELOG(_T("[CClientManager::RequestAllowList] End..."));
	return nResult;
}

// 2011/06/28 by realhan, 임시 정책 해지 리스트를 가져온다.
int CClientManager::GetCancelInstantList(LPCTSTR agentid)
{
	int result = 0;
	WRITELOG(_T("[CClientManager::GetCancelInstantList] GetCancelList Start agentid : %s"), agentid);
	CString sReq = agentid, sResp = _T(""), sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSALLOW, sServerURL);
	WRITELOG(_T("[CClientManager::GetCancelInstantList] - m_stPageInfo.m_GSALLOW - %s"), m_stPageInfo.m_GSALLOW);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSALLOW : sPageName, _T("18207"));

	WRITELOG(_T("[CClientManager::GetCancelInstantList] sCmd : %s, sReq : %s"), sCmd, sReq);
	CStringW wsCmd = sCmd, wsReq = sReq;
	int nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);
	if (nRet != 0)
	{
		WRITELOG(_T("[CClientManager::GetCancelInstantList] GetCancelList"));
		result = -1;
		return result;
	}

	WRITELOG(_T("[CClientManager::GetCancelInstantList] GetCancelList sResp : %s"), sResp);

	// 0 : table_id, 5 : 허용시간
	CString sResult = sResp;

	if (!nRet)
	{
		WRITELOG(_T("[CClientManager::GetCancelInstantList] GetCancelList agent 이다."));
		sResult = ExtractAllowData(sResp);
	}

	// 2012/06/21, avilon
	BOOL bExistFileTempList = FALSE, bExistDevTempList = FALSE, bExistAppTempList = FALSE, bExistWebTempList = FALSE;

	bExistFileTempList = WriteInstantItemList(sResult, REQ_TYPE_FILE);
	bExistDevTempList = WriteInstantItemList(sResult, REQ_TYPE_DEV);
	bExistAppTempList = WriteInstantItemList(sResult, REQ_TYPE_APP);
	bExistWebTempList = WriteInstantItemList(sResult, REQ_TYPE_WEB);

	if (bExistFileTempList == TRUE)		SendFileTempPolicyUpdateMSG();
	if (bExistDevTempList == TRUE)		FindAgentANDSendPostMSG();
	/*if (bExistAppTempList == TRUE)*/		SendAppTempPolicyUpdateMSG();
	/*if (bExistWebTempList == TRUE)*/		SendWebTempPolicyUpdateMSG();
		
	// 정책 재수신 (경로별 임시정책이 중지되었을 경우, 원래 정책으로 돌려놔야한다. MSG까지)

	if (m_bGSALLOW_62 == TRUE)
	{
		UpdateInstantLevelEndDate();
		CreateInstantLevel();
	}
	else 
	{
		if (StartInstantPolicies() == -1)
		{
			//CreateInstantLevel();
			RequestCancleInstantPolicies();
		}
	}

	if (WriteAllowData(sResult, TRUE))
	{
		WRITELOG(_T("[CClientManager] GetCancelList::WriteAllowData FALSE"));
		result = -2;
		return result;
	}

	WRITELOG(_T("[CClientManager] GetCancelList End..."));
	return result;
}

// 2012/06/12, avilon
// 임시정책 리스트 중 승인된 파일 임시정책을 저장한다.
BOOL CClientManager::WriteInstantItemList(LPCTSTR data, int nReqType)
{
	BOOL bRet = FALSE;
	TCHAR lpLogPath[MAX_PATH];
	CString sRowData = _T(""), sResult = _T("");
	CString sLogPath = _T(""), sData = _T("");
	CString sTempPath;
	int i = 0;
	CBMSSupport support;
	CString sOutput = _T("");
	CString sCmd;
	TCHAR szNewPath[MAX_PATH+1] = {0,};
	CString sItemDataName = _T(""), sItemDataKey = _T("");
	CJSonDataParser	cJSonDataParser(g_Separator);

	GetLocalLowLogPath(szNewPath, MAX_PATH);
	_tcsncat_s(szNewPath, _countof(szNewPath), LOG_LIST_DIRECTORY, _TRUNCATE);

	if (nReqType == REQ_TYPE_DEV)
	{
		sItemDataName = BM_ALLOW_DEV_DATA;
		sItemDataKey = BM_REACTION_DEV;
	}
	else if (nReqType == REQ_TYPE_FILE)
	{
		sItemDataName = BM_ALLOW_FILE_DATA;
		sItemDataKey = BM_REACTION_FILE;
	}
	else if (nReqType == REQ_TYPE_APP)
	{
		sItemDataName = BM_ALLOW_APP_DATA;
		sItemDataKey = BM_REACTION_APP;
	}
	else if (nReqType == REQ_TYPE_WEB)
	{
		sItemDataName = BM_ALLOW_WEB_DATA;
		sItemDataKey = BM_REACTION_WEB;
	}

	_tcsncat_s(szNewPath, _countof(szNewPath), sItemDataName, _TRUNCATE);
	GetAgentLogPath(lpLogPath, MAX_PATH);
	sLogPath.Format(_T("%s%s\\%s"), lpLogPath, BM_RDATA, sItemDataName);

	sTempPath.Format(_T("%s%s\\_%s"), lpLogPath, BM_RDATA, sItemDataName);
	WRITELOG(_T("[CClientManager::WriteFileTempList] sTempPath : %s") ,sTempPath);

	HANDLE hReqFile = CreateFile(sLogPath, FILE_ALL_ACCESS, FILE_SHARE_READ | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hReqFile == INVALID_HANDLE_VALUE)
	{
		WRITELOG(_T("[CClientManager::WriteFileTempList] CreateFile INVALID_HANDLE_VALUE : %d"), GetLastError());
		if(GetLastError() == ERROR_ACCESS_DENIED)
		{
			if(_trename(sLogPath, sTempPath))
			{
				WRITELOG(_T("[CClientManager::WriteFileTempList] rename failed error=%d"), GetLastError());
				goto WriteFileTempList_Exit;
			}
			// 2011/03/30, avilon
			// 생성 실패시 이름을 변경하고 다시 생성한다.
			else
			{
				WRITELOG(_T("[CClientManager::WriteFileTempList] rename 성공!!"));
				if((hReqFile = CreateFile(sLogPath, FILE_ALL_ACCESS, FILE_SHARE_READ | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))==INVALID_HANDLE_VALUE)
				{
					WRITELOG(_T("[CClientManager::WriteFileTempList] CreateFile INVALID_HANDLE_VALUE2 : %d"), GetLastError());
					goto WriteFileTempList_Exit;
				}
				::DeleteFile(sTempPath);
			}
		}

		goto WriteFileTempList_Exit;
	}
	else if (!hReqFile)
	{
		WRITELOG(_T("[CClientManager::WriteFileTempList] CreateFile Failed : %d"), GetLastError());
		goto WriteFileTempList_Exit;
	}

	if (hReqFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hReqFile);
		hReqFile = INVALID_HANDLE_VALUE;
	}

	while (AfxExtractSubString(sRowData, data, i, _T('|')))
	{
		cJSonDataParser.SetParsingData2(sRowData);

		InsteadOfWhile :

		CString sFiletemp, sAllowed;
		//AfxExtractSubString(sFiletemp, sRowData, 16, g_Separator);
		sFiletemp = cJSonDataParser.GetStringValueOfCurrentIndex(16, "reqreaction");
		//AfxExtractSubString(sAllowed, sRowData, 8, g_Separator);
		sAllowed = cJSonDataParser.GetStringValueOfCurrentIndex(8, "state");
		if ((sFiletemp.CompareNoCase(sItemDataKey)==0) && (sAllowed.CompareNoCase(_T("A"))==0))
		{
			if (cJSonDataParser.m_bUsedJSonData)
			{
				cJSonDataParser.SetObjectToNewRoot();
			}
			else
			{
				sResult += sRowData;
				sResult += _T("|");
			}
		}
		i++;

		if (cJSonDataParser.m_bUsedJSonData)
		{
			cJSonDataParser.m_nCurrentIndex++;
			if (cJSonDataParser.IsCurrentIndexAnObject())
			{
				goto InsteadOfWhile;
			}
			else
			{
				sRowData = cJSonDataParser.GetStringOfNewRoot();
				sResult += sRowData;
				break;
			}
		}
	}

	if (sResult == _T(""))
	{
		WRITELOG(_T("[CClientManager::WriteFileTempList] 승인 된 파일 임시정책이 없다."));
		goto WriteFileTempList_Exit;
	}

	sResult = sResult.Left(sResult.GetLength() - 1);

	if (m_bIsLogEncoded == TRUE)
	{
		sData = GetBase64EncodeData(sResult);
		WRITELOG(_T("[CClientManager::WriteFileTempList] 인코딩 후 sData = %s"), sData);
	}
	else
	{
		sData = sResult;
	}

	LPCTSTR sEncodeData = sData;

	if (_tcslen(sEncodeData) == 0)
	{
		WRITELOG(_T("[CClientManager::WriteFileTempList] 넘겨 받은 데이터가 없다."));
		goto WriteFileTempList_Exit;
	}

	if (MultiLanguageWriteFile(sLogPath, sEncodeData, m_bUseMultiLan?CHARSET_UTF8:CHARSET_MULTIBYTE) == FALSE)
	{
		WRITELOG(_T("[CClientManager::WriteFileTempList] MultiLanguageWriteFile Failed : charset(%d), error(%d)"), CHARSET_UTF8, GetLastError());
		goto WriteFileTempList_Exit;
	}

	// 2012/09/14, avilon - UAC 사용자를 위해 LocalLow에 써준다.
	//NULL DACL
	DWORD dwResult = 0;
	SECURITY_ATTRIBUTES LogPathSA;
	SECURITY_DESCRIPTOR LogPathSD;

	// security inits
	ZeroMemory(&LogPathSA,sizeof(SECURITY_ATTRIBUTES));
	if (!InitializeSecurityDescriptor(&LogPathSD, SECURITY_DESCRIPTOR_REVISION))
	{
		dwResult = GetLastError();
	}
	// set NULL DACL on the SD
	if (!SetSecurityDescriptorDacl(&LogPathSD, TRUE, (PACL)NULL, FALSE))
	{
		dwResult = GetLastError();
	}

	if (!dwResult)
	{
		// now set up the security attributes
		LogPathSA.nLength = sizeof(SECURITY_ATTRIBUTES);
		LogPathSA.bInheritHandle  = TRUE; 
		LogPathSA.lpSecurityDescriptor = &LogPathSD;

		if (MultiLanguageWriteFile(szNewPath, sEncodeData, m_bUseMultiLan?CHARSET_UTF8:CHARSET_MULTIBYTE, &LogPathSA) == FALSE)
		{
			WRITELOG(_T("[CClientManager::WriteFileTempList] - Write failed to LocalLow - %d"), GetLastError());
			goto WriteFileTempList_Exit;
		}
	}
	else
	{
		WRITELOG(_T("[CClientManager::WriteFileTempList] - SecurityDescriptor Failed - %d"), dwResult);
		goto WriteFileTempList_Exit;
	}
	////////////////////////////////////////////////////////////////////////// end 2012/09/14

	bRet = TRUE;

WriteFileTempList_Exit:

	if (bRet == FALSE)
	{
		::DeleteFile(szNewPath);
	}

	return bRet;
}

int CClientManager::WriteAllowData(LPCTSTR data, BOOL bReq)
{
	WRITELOG(_T("[CClientManager::WriteAllowData] Start"));
	int nResult = 0;
	

	CString sSub = _T(""), sRow = _T(""), sData = _T("");
	int nSepCount = 0;

	AfxExtractSubString(sRow, data, 0, _T('|'));
	
	while (AfxExtractSubString(sSub, sRow, nSepCount, g_Separator))
	{
		nSepCount++;
	}

	// 2011/01/21, avilon - 시간 단위 추가.
	if((nSepCount != 18) && (nSepCount != 20) && ((nSepCount < 26) || (31 < nSepCount)))
	{
		WRITELOG(_T("[CClientManager::WriteAllowData] Bad data : %d"), nSepCount);
		WRITELOG(_T("[CClientManager::WriteAllowData] JSon로그인지 확인"));

		CJSonDataParser	cJSonDataParser;
		if (!cJSonDataParser.SetParsingData2(sRow))
		{
			data = _T("");
		}
		else
		{
			WRITELOG(_T("[CClientManager::WriteAllowData] JSon로그로 진행"));
		}
	}
	CString sTempPath = _T("");
	sTempPath.Format(_T("%s%s\\_%s"), g_pBMSCommAgentDlg->m_sBMSDATLogPath, BM_RDATA, (bReq) ? BM_REQ_DATA : BM_ALLOW_DATA);
	WRITELOG(_T("[CClientManager::WriteAllowData] sTempPath : %s") ,sTempPath);
	
	CString sLogPath = _T("");
	sLogPath.Format(_T("%s%s\\%s"), g_pBMSCommAgentDlg->m_sBMSDATLogPath, BM_RDATA, (bReq) ? BM_REQ_DATA : BM_ALLOW_DATA);
	WRITELOG(_T("[CClientManager::WriteAllowData] sLogPath : %s") ,sLogPath);

	HANDLE hReqFile = CreateFile(sLogPath, FILE_ALL_ACCESS, FILE_SHARE_READ | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hReqFile == INVALID_HANDLE_VALUE)
	{
		WRITELOG(_T("[CClientManager::WriteAllowData] CreateFile INVALID_HANDLE_VALUE : %d"), GetLastError());
		if (GetLastError() == ERROR_ACCESS_DENIED)
		{
			if (_trename(sLogPath, sTempPath))
			{
				WRITELOG(_T("[CClientManager::WriteAllowData] rename failed error=%d"), GetLastError());
				nResult = -1;
				goto WriteAllowReqData_Exit;
			}
			// 2011/03/30, avilon
			// 생성 실패시 이름을 변경하고 다시 생성한다.
			else
			{
				WRITELOG(_T("[CClientManager::WriteAllowData] rename 성공!!"));
				if ((hReqFile = CreateFile(sLogPath, FILE_ALL_ACCESS, FILE_SHARE_READ | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))==INVALID_HANDLE_VALUE)
				{
					WRITELOG(_T("[CClientManager::WriteAllowData] CreateFile INVALID_HANDLE_VALUE2 : %d"), GetLastError());
					nResult = -1;
					goto WriteAllowReqData_Exit;
				}
				::DeleteFile(sTempPath);
			}
		}
		else
		{
			nResult = -1;
			goto WriteAllowReqData_Exit;
		}
		nResult = -1;
		goto WriteAllowReqData_Exit;
	}
	else if (!hReqFile)
	{
		WRITELOG(_T("[CClientManager::WriteAllowData] CreateFile FALSE : %d"), GetLastError());
		nResult = -2;
		goto WriteAllowReqData_Exit;
	}
	DWORD dwWriteByte = 0;

	// 2011/03/07, avilon
	if (m_bIsLogEncoded == TRUE)
	{
		sData = GetBase64EncodeData(data);
		WRITELOG(_T("[CClientManager::WriteAllowData] RequestAllowList 인코딩 후 sData = %s"), sData);
	}
	else
	{
		sData = data;
	}

	LPCTSTR sEncodeData = sData;

	if (hReqFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hReqFile);
		hReqFile = INVALID_HANDLE_VALUE;
	}

	if (_tcslen(sEncodeData) == 0)
	{
		WRITELOG(_T("[CClientManager::WriteAllowData] 넘겨 받은 데이터가 없다."));
		nResult = -4;
		goto WriteAllowReqData_Exit;
	}

	if (m_bUseMultiLan == TRUE)
	{
		if (MultiLanguageWriteFile(sLogPath, sEncodeData, CHARSET_UTF8) == FALSE)
		{
			WRITELOG(_T("[CClientManager::WriteAllowData] MultiLanguageWriteFile as UTF8 FALSE : %d"), GetLastError());
			nResult = -3;
			goto WriteAllowReqData_Exit;
		}
	}
	else
	{
		if (MultiLanguageWriteFile(sLogPath, sEncodeData, CHARSET_MULTIBYTE) == FALSE)
		{
			WRITELOG(_T("[CClientManager::WriteAllowData] MultiLanguageWriteFile as Multi FALSE : %d"), GetLastError());
			nResult = -3;
			goto WriteAllowReqData_Exit;
		}
	}


WriteAllowReqData_Exit:
	if(hReqFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hReqFile);
	}

	WRITELOG(_T("[CClientManager::WriteAllowData] End"));
	return nResult;
}

// 정책을 요청한다.
int CClientManager::StartInstantPolicies()
{
	WRITELOG(_T("[CClientManager::StartInstantPolicies] Start"));

	m_sDLPPolicyTime = _T("");

	// 2010/10/12, avilon - agentID를 얻어 오지 못 한 경우 agentinfo에서 다시 읽어 온다.
	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::StartInstantPolicies] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::StartInstantPolicies] - m_sAgentID = %s"), m_sAgentID);
	}
	CString sReq = _T(""), sReceived = _T(""), sCmd = _T("");

	EnterCriticalSection(&m_AllowDataMapCS);
	if(!m_cAllowDataMap.GetCount())
	{
		WRITELOG(_T("[CClientManager::StartInstantPolicies] !m_cAllowDataList.GetCount()"));
		LeaveCriticalSection(&m_AllowDataMapCS);
		return -1;
	}
	LeaveCriticalSection(&m_AllowDataMapCS);

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSALLOW, sServerURL);
	WRITELOG(_T("[CClientManager::StartInstantPolicies] - m_stPageInfo.m_GSALLOW - %s"), m_stPageInfo.m_GSALLOW);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSALLOW : sPageName, _T("18205"));
	CStringW wsCmd = sCmd, wsReq = _T("");
	int ret = 0; 
	
	int index = 0;

	// 2012/06/18, avilon
	CString sTempReceive = _T("");

	EnterCriticalSection(&m_AllowDataMapCS);
	POSITION stPos = m_cAllowDataMap.GetStartPosition();
	LeaveCriticalSection(&m_AllowDataMapCS);
	while (true)
	{
		if (!stPos)
		{
			WRITELOG(_T("[CClientManager] StartInstantPolicies::stPos 0x%x"), stPos);
			break;
		}

		sReceived = _T("");

		CString sKey = _T("");
		ALLOW_DATA stAllowData;
		EnterCriticalSection(&m_AllowDataMapCS);
		m_cAllowDataMap.GetNextAssoc(stPos, sKey, stAllowData);
		LeaveCriticalSection(&m_AllowDataMapCS);

		index++;
		sReq.Format(_T("%d%c%s%c%s%c"), stAllowData.m_dwTableid, g_Separator, m_sAgentID, g_Separator, stAllowData.m_sEndDate, g_Separator);
		if(index == 1)
		{
			sReq += _T("START");
		}
		else if(!stPos)
		{
			sReq += _T("END");
		}

		// 2012/06/15, avilon
		if(stAllowData.sReqType.IsEmpty() == FALSE)
		{
			sReq += g_Separator;
			sReq += stAllowData.sReqType;
		}

		wsReq = sReq;
		ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sReceived, m_bIsHttps);
		if(ret)
		{
			WRITELOG(_T("[CClientManager] StartInstantPolicies::SendAndReceive FALSE ret : %d"), ret);
		}
		else
		{
			sTempReceive = sReceived;
		}
	}
	
	WRITELOG(_T("[CClientManager::StartInstantPolicies] sReceived.GetLength %d"), sReceived.GetLength());

	// 2012/06/18, avilon
	if((ret==0) && (sTempReceive.GetLength() > 0))
	{
		sReceived = sTempReceive;

		m_policy_update_time = _T("");

		if(m_OSVerMajor >= 6)
		{
			TCHAR tempBuf[MAX_PATH];
			GetCSIDLDirectory(tempBuf, CSIDL_PROFILE);
			CString sUserDir = tempBuf;
			DWORD dwPos = sUserDir.ReverseFind(_T('\\'));
			sUserDir = sUserDir.Left(dwPos + 1);
			sUserDir += m_sSupportName;
			_sntprintf(tempBuf, MAX_PATH-1, _T("%s\\AppData\\Local\\VirtualStore\\Windows\\System32\\"), sUserDir);
			WRITELOG(_T("[CClientManager] Remove dir dir path : %s"), tempBuf);
			TCHAR VirbmsdatPath[MAX_PATH], VirbmsbinPath[MAX_PATH];
			_sntprintf(VirbmsbinPath, MAX_PATH-1, _T("%sbmsbin\\"),tempBuf);
			_sntprintf(VirbmsdatPath, MAX_PATH-1, _T("%sbmsdat\\"),tempBuf);
			RemoveAllDir(VirbmsbinPath);
			RemoveAllDir(VirbmsdatPath);
		}
		FILE*		fp	= NULL;
		TCHAR*		data_ptr	= NULL;

		if (sReceived.GetLength() == 0)
		{
			WRITELOG(_T("[CClientManager] StartInstantPolicies - 정책을 받지 못함"));
			return -2;
		}

		// 2014.09.16, kih5893 : 임시정책 요청의 경우 처음에 process 실행 여부 list를 구하지 않고 있어서 수정
		// 2018.02.08 kjh4789 Look 통신과 ClientManager 객체 분리, LookServer 최초에 한번 구한다
// 		if (m_bPolicyReceived == FALSE && m_bUseAsset == FALSE)
// 		{
// 			WRITELOG(_T("[CClientManager::StartInstantPolicies] - RequestRunningProcessCheckList 호출"));
// 			RequestRunningProcessCheckList();
// 		}
		
		m_bPolicyReceived = TRUE;
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sReceived, _T(""));
		m_sDLPPolicyTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));

		WRITELOG(_T("[CClientManager] StartInstantPolicies : 정책 파일 생성"));

		// Agent들에게 알림
		RequestUpdatePolicies();
		// 2013.08.13, kih5893 임시정책 요청 후에도 copysave 값 가져올 수 있도록 수정 
		ApplyPolicies();

	}
	// 2011/06/27 by realhan, 레벨 변경으로 인한 임시정책 중지.
	else if(ret == BM_ERROR_NO_ALLOWREQUEST)
	{
		WRITELOG(_T("[CClientManager::StartInstantPolicies] 임시정책 ERR-2001 #해당 agent의 임시정책이 정책 변경으로 인하여 해제됨"));

		RequestAllowList(m_sAgentID, TRUE);
		EnterCriticalSection(&m_AllowDataMapCS);
		m_cAllowDataMap.RemoveAll();
		LeaveCriticalSection(&m_AllowDataMapCS);
		m_bIsCurrentUsingInstantLevel = FALSE;
		g_pBMSCommAgentDlg->KillTimer(ALLOW_INSTANTLEVEL_TIMER_ID);
		SendReactTrayRefresh();
		RequestPolicies(FALSE, TRUE);
	}

	if(m_bPolicyReceived)
	{
		// 2009/08/13 by avilon, 사본 확장자 제외 목록
		CGRegulationManager reg_man;
		if(reg_man.Read() < 0)
		{
			WRITELOG(_T("[CClientManager::StartInstantPolicies] ERROR! - GRegulationManager Read Fail!!"));
		}

		m_sExcepExpan = reg_man.getGlobalString(_T("copysave_ex_ext_list"));
		m_sExcepExpan = Make_lower(m_sExcepExpan);
	}

	WRITELOG(_T("[CClientManager::StartInstantPolicies] End"));
	return 0;
}

BOOL CClientManager::GetCSIDLDirectory(LPTSTR lpPathBuf, int CSIDL)
{
	LPITEMIDLIST ip;
	if (SUCCEEDED(SHGetFolderLocation(NULL, CSIDL , NULL, NULL, &ip)))
	{
		LPMALLOC pMalloc;
		SHGetMalloc(&pMalloc);
		SHGetPathFromIDList(ip, lpPathBuf);
		pMalloc->Free(ip);
		pMalloc->Release();
		return TRUE;
	}
	lpPathBuf = NULL;

	return FALSE;
}

int CClientManager::EndInstantPolicies(DWORD dwTableid, CString sReqType)
{
	WRITELOG(_T("[CClientManager::EndInstantPolicies] EndInstantPolicies Start"));

	// 2010/10/12, avilon - agentID를 얻어 오지 못 한 경우 agentinfo에서 다시 읽어 온다.
	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::EndInstantPolicies] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::EndInstantPolicies] - m_sAgentID = %s"), m_sAgentID);
	}

	int result = 0;
	CString sReq = _T(""), sReceived = _T(""), sCmd = _T(""), sServerURL = _T("");

	CString sPageName = GetServerInfo(m_stPageInfo.m_GSALLOW, sServerURL);
	WRITELOG(_T("[CClientManager::EndInstantPolicies] - m_stPageInfo.m_GSALLOW - %s"), m_stPageInfo.m_GSALLOW);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSALLOW : sPageName, _T("18206"));
	CStringW wsCmd = sCmd, wsReq = _T("");
	sReq.Format(_T("%d%c%s"), dwTableid, g_Separator, m_sAgentID);
	// 2012/06/15, avilon
	if (sReqType.CompareNoCase(_T("BLOCK")) == 0 || sReqType.CompareNoCase(_T("FILE")) == 0)
	{
		sReq += g_Separator;
		sReq += sReqType;
	}
	wsReq = sReq;
	int ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sReceived, m_bIsHttps);
	if (ret == 0)
	{
		m_policy_update_time = _T("");

		if(m_OSVerMajor >= 6)
		{
			TCHAR tempBuf[MAX_PATH];
			GetCSIDLDirectory(tempBuf, CSIDL_PROFILE);
			CString sUserDir = tempBuf;
			DWORD dwPos = sUserDir.ReverseFind(_T('\\'));
			sUserDir = sUserDir.Left(dwPos + 1);
			sUserDir += m_sSupportName;
			//sprintf(tempBuf,"%s\\AppData\\Local\\VirtualStore\\Windows\\System32\\", sUserDir);
			_sntprintf(tempBuf, MAX_PATH-1, _T("%s\\AppData\\Local\\VirtualStore\\Windows\\System32\\"), sUserDir);
			WRITELOG(_T("[CClientManager] Remove dir dir path : %s"), tempBuf);
			TCHAR VirbmsdatPath[MAX_PATH], VirbmsbinPath[MAX_PATH];
			//sprintf(VirbmsbinPath,"%sbmsbin\\",tempBuf);
			_sntprintf(VirbmsbinPath, MAX_PATH-1, _T("%sbmsbin\\"),tempBuf);
			//sprintf(VirbmsdatPath,"%sbmsdat\\",tempBuf);
			_sntprintf(VirbmsdatPath, MAX_PATH-1, _T("%sbmsdat\\"),tempBuf);
			RemoveAllDir(VirbmsbinPath);
			RemoveAllDir(VirbmsdatPath);
		}
		FILE*		fp	= NULL;
		TCHAR*		data_ptr	= NULL;

		if(sReceived.GetLength() == 0)
		{
			WRITELOG(_T("[CClientManager] EndInstantPolicies 정책을 받지 못함"));
			return -1;
		}

		m_bPolicyReceived = TRUE;
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sReceived, _T(""));

		// Agent들에게 알림
		RequestUpdatePolicies();
	}
	// 2012/06/18, avilon
	else if(ret == BM_SERVER_ERR_END_FILE_TEMP)
	{
		WRITELOG(_T("[CClientManager] 파일별 임시정책 종료다.(%d)"), ret);
		BOOL bExistFileTempList = FALSE, bExistDevTempList = FALSE, bExistAppTempList = FALSE, bExistWebTempList = FALSE;

		bExistFileTempList = WriteInstantItemList(sReceived, REQ_TYPE_FILE);
		bExistDevTempList = WriteInstantItemList(sReceived, REQ_TYPE_DEV);
		bExistAppTempList = WriteInstantItemList(sReceived, REQ_TYPE_APP);
		bExistWebTempList = WriteInstantItemList(sReceived, REQ_TYPE_WEB);

		if (bExistFileTempList == TRUE)		SendFileTempPolicyUpdateMSG();
		if (bExistDevTempList == TRUE)		FindAgentANDSendPostMSG();
		/*if (bExistAppTempList == TRUE)*/		SendAppTempPolicyUpdateMSG();
		/*if (bExistWebTempList == TRUE)*/		SendWebTempPolicyUpdateMSG();
	}

	WRITELOG(_T("[CClientManager] EndInstantPolicies End"));
	return ret;
}

CString CClientManager::ExtractAllowData( CString sResp )
{
	WRITELOG(_T("[CClientManager] ExtractAllowData Start sResp : %s"), sResp);
	m_cAllowDataMap.RemoveAll();
	CString sRowSub = _T(""), sResult = _T("");
	int i = 0;

	CJSonDataParser cJSonDataParser(g_Separator);

	while (AfxExtractSubString(sRowSub, sResp, i, _T('|')))
	{
		if(!sRowSub.GetLength())
		{
			break;
		}

		cJSonDataParser.SetParsingData2(sRowSub);

		InsteadOfWhile :

		CString sSub = _T(""),sState = _T(""), sEndTime = _T("");;
		//AfxExtractSubString(sSub, sRowSub, 8, g_Separator);
		sState = cJSonDataParser.GetStringValueOfCurrentIndex(8, "state");
		//AfxExtractSubString(sSub, sRowSub, 17, g_Separator);
		sEndTime = cJSonDataParser.GetStringValueOfCurrentIndex(17, "enddatetime");

		ALLOW_DATA stAllowData;
		stAllowData.m_dwTableid = 0;
		stAllowData.m_sAction = _T("");
		stAllowData.m_sEndDate = sEndTime;
		stAllowData.m_sState = sState;
		stAllowData.m_sUnit = _T("");

		WRITELOG(_T("### ExtractAllowData : sState(%s)"), sState);

		if(!sState.CompareNoCase(_T("A")))
		{
			BOOL bEndDate = FALSE;
			m_bIsCurrentUsingInstantLevel = TRUE;
			//AfxExtractSubString(sSub, sRowSub, 0, g_Separator);
			sSub = cJSonDataParser.GetStringValueOfCurrentIndex(0, "reqid");
			stAllowData.m_dwTableid = _ttoi(sSub);
			stAllowData.m_sAction = sSub;
			stAllowData.m_sAction += g_Separator;
			//AfxExtractSubString(sSub, sRowSub, 12, g_Separator);
			sSub = cJSonDataParser.GetStringValueOfCurrentIndex(12, "allowterm");
			DWORD dwTerm = _ttoi(sSub);
			//AfxExtractSubString(sSub, sRowSub, 5, g_Separator);
			sSub = cJSonDataParser.GetStringValueOfCurrentIndex(5, "reqaction");
			stAllowData.m_sAction += sSub;
			// 2009/08/11 by avilon, Action과 Reaction으로 맵키를 지정.
			stAllowData.m_sAction += g_Separator;
			//AfxExtractSubString(sSub, sRowSub, 16, g_Separator);
			sSub = cJSonDataParser.GetStringValueOfCurrentIndex(16, "reqreaction");
			stAllowData.m_sAction += sSub;
			stAllowData.sReqType = sSub.CompareNoCase(_T("BLOCK"))==0 ? _T("") : sSub;
			//AfxExtractSubString(sSub, sRowSub, 19, g_Separator);
			sSub = cJSonDataParser.GetStringValueOfCurrentIndex(19, "allow_timetype");
			if (sSub.GetLength() > 0)
			{
				stAllowData.m_sUnit = sSub;
			}
			else
			{
				stAllowData.m_sUnit = _T("MINUTE");
			}

			if (sEndTime.IsEmpty() == TRUE)
			{
				WRITELOG(_T("[CClientManager] ExtractAllowData sEndTime NULL"));
				CalculateDate stDate;
				BOOL bEndlie = FALSE;

				if(stAllowData.m_sUnit.CompareNoCase(_T("MONTH"))==0)
				{
					stDate.AddMonth(dwTerm);
				}
				else if(stAllowData.m_sUnit.CompareNoCase(_T("HOUR"))==0)
				{
					stDate.AddHour(dwTerm);
				}
				else if(stAllowData.m_sUnit.CompareNoCase(_T("DAY"))==0)
				{
					stDate.AddDay(dwTerm);
				}
				else if (stAllowData.m_sUnit.CompareNoCase(_T("MINUTE"))==0)
				{
					stDate.AddMinute(dwTerm);
				}
				else if (stAllowData.m_sUnit.CompareNoCase(_T("ENDDATE")) == 0)
				{
					CString sEndDate = _T("");
					//AfxExtractSubString(sEndDate, sRowSub, 29, g_Separator);
					sEndDate = cJSonDataParser.GetStringValueOfCurrentIndex(29, "allowEndDatetime");
					bEndDate = TRUE;
					stAllowData.m_sEndDate.Format(_T("%s235959"), sEndDate);
				}

				if (bEndDate == FALSE)
				{
					SYSTEMTIME stCurTime;
					stDate.GetCalcDateTime(stCurTime);

					stAllowData.m_sEndDate.Format(_T("%d%02d%02d%02d%02d%02d"), stCurTime.wYear, stCurTime.wMonth, stCurTime.wDay, stCurTime.wHour, stCurTime.wMinute, stCurTime.wSecond);
				}

				if (cJSonDataParser.m_bUsedJSonData)
				{
					cJSonDataParser.SetStringValueOfCurrentIndex("enddatetime", UTF16ToUTF8(stAllowData.m_sEndDate));
				}
				else
				{
					CString sTemp, sTempResult = _T("");
					int nLoc = 0;
					while (AfxExtractSubString(sTemp, sRowSub, nLoc, FIELD_SEPARATOR))
					{
						if(nLoc == 17)
						{
							sTempResult += stAllowData.m_sEndDate;
						}
						else
						{
							sTempResult += sTemp;
						}
						sTempResult += g_Separator;
						nLoc++;
					}
					sRowSub = sTempResult.Left(sTempResult.GetLength()-1);
				}
			}
			else
			{
				WRITELOG(_T("[CClientManager] ExtractAllowData sEndTime !NULL"));
				stAllowData.m_sEndDate = sEndTime;
			}

			WRITELOG(_T("[CClientManager] ExtractAllowData EndDate : %s") , stAllowData.m_sEndDate);
			EnterCriticalSection(&m_AllowDataMapCS);
			m_cAllowDataMap.SetAt(stAllowData.m_sAction, stAllowData);
			LeaveCriticalSection(&m_AllowDataMapCS);
		}
		else if(!sState.CompareNoCase(REQ_ALLOW_MANAGER_CANCEL))
		{
			CString tmp_key = _T("");
			// 2012/06/15, avilon
			//AfxExtractSubString(sSub, sRowSub, 0, g_Separator);
			sSub = cJSonDataParser.GetStringValueOfCurrentIndex(0, "reqid");
			DWORD tmpTableid = _ttoi(sSub);
			tmp_key = sSub;
			tmp_key += g_Separator;
			//AfxExtractSubString(sSub, sRowSub, 5, g_Separator);
			sSub = cJSonDataParser.GetStringValueOfCurrentIndex(5, "reqaction");
			tmp_key += sSub;
			tmp_key += g_Separator;
			//AfxExtractSubString(sSub, sRowSub, 16, g_Separator);
			sSub = cJSonDataParser.GetStringValueOfCurrentIndex(16, "reqreaction");
			tmp_key += sSub;

			ALLOW_DATA lookup_allow_data;
			EnterCriticalSection(&m_AllowDataMapCS);
			m_cAllowDataMap.Lookup(tmp_key, lookup_allow_data);

			if(lookup_allow_data.m_dwTableid == tmpTableid)
			{
				m_cAllowDataMap.RemoveKey(tmp_key);
				if(m_bGSALLOW_62 == TRUE)
				{
					UpdateInstantLevelEndState(tmpTableid, lookup_allow_data.sReqType);
					CreateInstantLevel();
				}
				else
				{
					EndInstantPolicies(tmpTableid, lookup_allow_data.sReqType);
				}

				WRITELOG(_T("### ExtractAllowData : REQ_ALLOW_MANAGER_CANCEL RemoveKey(%s) id(%d) cnt(%d)"), tmp_key, m_cAllowDataMap.GetCount());

				if(!m_cAllowDataMap.GetCount())
				{
					WRITELOG(_T("### ExtractAllowData : REQ_ALLOW_MANAGER_CANCEL m_cAllowDataMap == 0 타이머 중지. m_bIsCurrentUsingInstantLevel = FALSE"));
					m_bIsCurrentUsingInstantLevel = FALSE;
					g_pBMSCommAgentDlg->KillTimer(ALLOW_INSTANTLEVEL_TIMER_ID);
				}

			}
			LeaveCriticalSection(&m_AllowDataMapCS);
		}

		if (cJSonDataParser.m_bUsedJSonData)
		{
			cJSonDataParser.SetObjectToNewRoot();
			cJSonDataParser.m_nCurrentIndex++;
			if (cJSonDataParser.IsCurrentIndexAnObject())
			{
				goto InsteadOfWhile;
			}
			else
			{
				sRowSub = cJSonDataParser.GetStringOfNewRoot();
				sResult += (!i) ? sRowSub : _T("|") + sRowSub;
				break;
			}
		}
		else
		{
			sResult += (!i) ? sRowSub : _T("|") + sRowSub;
			i++;
		}
	}

	EnterCriticalSection(&m_AllowDataMapCS);
	WRITELOG(_T("### ExtractAllowData : m_cAllowDataMap cnt(%d)"), m_cAllowDataMap.GetCount());
	LeaveCriticalSection(&m_AllowDataMapCS);

	WRITELOG(_T("[CClientManager] ExtractAllowData End sResult : %s"), sResult);
	return sResult;
}

void CClientManager::SendReactTrayRefresh(DWORD dwFlag)
{
	if (g_pBMSCommAgentDlg->m_bIsMainSession)
	{
		WriteFilePolicyUpdateTime(LAST_REACT_TRAY_REFRESH_FILENAME);
	}
	WRITELOG(_T("[CClientManager] SendReactTrayRefresh Start dwFlag : %d"), dwFlag);
	HWND hWnd = FindWindow(_T("#32770"), BM_REACTAGENT_WND_TITLE);
	if(hWnd)
	{
		WRITELOG(_T("[CClientManager] SendSimpleLogGetMethod FindWindow"));
		PostMessage(hWnd, BMSM_GTRAY_REFRESH_LIST, dwFlag, NULL);
	}
	WRITELOG(_T("[CClientManager] SendSimpleLogGetMethod End"));
	
}

int CClientManager::ExtractTextFromFile(CFileIndex *pFile)
{
	WRITELOG(_T("[CClientManager] ExtractTextFromFile Start"));
	int Error = 0;
	ULONGLONG data_len	= pFile->m_File.GetLength();
	BYTE *lpBuf = 0;
	try
	{
		lpBuf = new BYTE[sizeof(BYTE) * data_len + 1];
	}
	catch (CMemoryException* e)
	{
		Error = BM_ERROR_CAN_NOT_ALLOCATE;
		return Error;
	}
	pFile->m_File.Read(lpBuf, data_len + 1);
	lpBuf[data_len] = NULL;
	CString sData = lpBuf;
	delete [] lpBuf; lpBuf = 0;
	
	sData.Trim();
	CString sAction = _T(""), sSub = _T("");
	AfxExtractSubString(sSub, sData, 2, g_Separator);
	sAction = sSub;
	WRITELOG(_T("[CClientManager] ExtractTextFromFile Act : %s"), sAction);
	if(!sAction.CompareNoCase(_T("SHARED")) 
		|| !sAction.CompareNoCase(_T("DEVICE"))
		|| !sAction.CompareNoCase(_T("WEBFILE"))
		|| !sAction.CompareNoCase(_T("FTP"))
		|| !sAction.CompareNoCase(_T("SMTPFILE"))
		|| !sAction.CompareNoCase(_T("MSGFILE"))
		|| !sAction.CompareNoCase(_T("MSGFILEROOM")))
	{
		AfxExtractSubString(sSub, sData, 14, g_Separator);
		CString sContents = sSub;
		AfxExtractSubString(sSub, sData, 4, g_Separator);
		CString sTargetFilePath = sSub;
		AfxExtractSubString(sSub, sData, 3, g_Separator);
		sTargetFilePath += sSub;

		if(sContents.IsEmpty() && FileExists(sTargetFilePath))
		{
			WRITELOG(_T("[CClientManager] ExtractTextFromFile SendSimpleLog sContents Empty"));
			CString sExtData = // g_cSn3.sn3_start(sTargetFilePath).c_str();
			sData += sExtData;
			WRITELOG(_T("[CClientManager] ExtractTextFromFile sData + sExtData : %s"), sData);
			CString sFilePath = pFile->m_File.GetFilePath();
			WRITELOG(_T("[CClientManager] sFilePath : %s"), sFilePath);
			pFile->m_File.Close();
			if(pFile->m_File.Open(sFilePath, CFile::modeCreate | CFile::modeReadWrite | CFile::shareDenyNone))
			{
				WRITELOG(_T("[CClientManager] ExtractTextFromFile Open Success"));
				pFile->m_File.Write(sData, sData.GetLength());
			}
		}
	}
	WRITELOG(_T("[CClientManager] ExtractTextFromFile End"));
	return Error;
}


// 2009. 08. 06, avilon
BOOL CClientManager::SetClientDateTime(CString sDateTime, CString sTimeSyncSec)
{
	BOOL bSuccess = TRUE;

	SYSTEMTIME st;
	st.wYear			= _ttoi(sDateTime.Mid(0, 4));
	st.wMonth			= _ttoi(sDateTime.Mid(4, 2));
	st.wDay				= _ttoi(sDateTime.Mid(6, 2));
	st.wHour			= _ttoi(sDateTime.Mid(8, 2));
	st.wMinute			= _ttoi(sDateTime.Mid(10, 2));
	st.wSecond			= _ttoi(sDateTime.Mid(12, 2));
	st.wMilliseconds	= 0;

	CTime cServerTime(st);
//	CTime cClientTime = CTime::GetCurrentTime();
	SYSTEMTIME time;
	GetLocalTime(&time);
	CTime cClientTime(time);
	CTimeSpan timeGap = cClientTime - cServerTime;
	CString sClientTime = cClientTime.Format(_T("%Y%m%d%H%M%S"));
	int nTimeSyncSec = 0;

	WRITELOG(_T("[SetClientDateTime] Get Server Time : %s"), sDateTime);
	WRITELOG(_T("[SetClientDateTime] Get Client Time : %s"), sClientTime);
	WRITELOG(_T("[SetClientDateTime] Get TimeSyncSec : %s"), sTimeSyncSec);

	if (sTimeSyncSec.IsEmpty() == TRUE)
	{
		nTimeSyncSec = 600;
	}
	else
	{
		nTimeSyncSec = _ttoi(sTimeSyncSec);
		if (nTimeSyncSec < 10 || 86400 < nTimeSyncSec)
		{
			nTimeSyncSec = 600;
		}
	}

	if(timeGap.GetTotalSeconds() <= -nTimeSyncSec || nTimeSyncSec <= timeGap.GetTotalSeconds())
	{
		if(!SetLocalTime(&st))
		{
			WRITELOG(_T("[SetClientDateTime] 시간동기화 실패 = %d"), GetLastError());
			bSuccess = FALSE;
		}
		else
		{
			WRITELOG(_T("[SetClientDateTime] 시간동기화 성공!!"));
		}
	}
	else
	{
		WRITELOG(_T("[SetClientDateTime] 시간편차가 %d분 %d초 미만이다!!"), nTimeSyncSec/60, nTimeSyncSec%60);
	}

	return bSuccess;
}

void CClientManager::SetClientDateTime_new(CString sDateTime, CString sTimeZone, int nTimeSyncSec)
{
	int nTimezoneBiasGap = 0;
	if (sTimeZone != _T(""))
	{
		CString	sRegPath = _T("");
		REG_TZI_FORMAT tzi;
		DWORD dwBufSize = MAX_PATH;

		sRegPath.Format(_T("%s%s"), _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\"), sTimeZone);
		if (!RegGetBinaryValue(HKEY_LOCAL_MACHINE, sRegPath, _T("TZI"), (LPBYTE)&tzi, &dwBufSize))
		{
			WRITELOG(_T("[SetClientDateTime2] RegGetBinaryValue fail - %d"), GetLastError());
		}
		else
		{
			TIME_ZONE_INFORMATION tz;
			if (GetTimeZoneInformation(&tz) == TIME_ZONE_ID_INVALID)
			{
				WRITELOG(_T("[SetClientDateTime2] GetTimeZoneInformation fail - %d"), GetLastError());
			}
			else
			{
				nTimezoneBiasGap = tzi.Bias - tz.Bias;

			}
		}
	}

	SYSTEMTIME st;
	st.wYear			= _ttoi(sDateTime.Mid(0, 4));
	st.wMonth			= _ttoi(sDateTime.Mid(4, 2));
	st.wDay				= _ttoi(sDateTime.Mid(6, 2));
	st.wHour			= _ttoi(sDateTime.Mid(8, 2));
	st.wMinute			= _ttoi(sDateTime.Mid(10, 2));
	st.wSecond			= _ttoi(sDateTime.Mid(12, 2));
	st.wMilliseconds	= 0;

	CTime cServerTime(st);
	CTimeSpan timeGap = nTimezoneBiasGap * 60;
	cServerTime += timeGap;
	CTime cClientTime = CTime::GetCurrentTime();
	WRITELOG(_T("[SetClientDateTime2] Get Server Time : %s"), sDateTime);
	WRITELOG(_T("[SetClientDateTime2] Set Server Time : %s"), cServerTime.Format(_T("%Y%m%d%H%M%S")));
	WRITELOG(_T("[SetClientDateTime2] Get Client Time : %s"), cClientTime.Format(_T("%Y%m%d%H%M%S")));
 	WRITELOG(_T("[SetClientDateTime2] Get TimeSyncSec : %d"), nTimeSyncSec);

	if (nTimeSyncSec <= 10 || 86400 <= nTimeSyncSec)
	{
		nTimeSyncSec = 600;
	}
	timeGap = cClientTime - cServerTime;
	if(timeGap.GetTotalSeconds() <= -nTimeSyncSec || nTimeSyncSec <= timeGap.GetTotalSeconds())
	{
		cServerTime.GetAsSystemTime(st);
		if(!SetLocalTime(&st))
		{
			WRITELOG(_T("[SetClientDateTime] 시간동기화 실패 = %d"), GetLastError());
		}
		else
		{
			WRITELOG(_T("[SetClientDateTime] 시간동기화 성공!!"));
		}
	}
	else
	{
		WRITELOG(_T("[SetClientDateTime] 시간편차가 %d분 %d초 미만이다!!"), nTimeSyncSec/60, nTimeSyncSec%60);
	}

}

// 2010/09/09, avilon
// IP address만 모은다.
/*
CString CClientManager::GetAllIPaddress()
{
	CString sIPAddr;
	CString sList;
	PIP_ADAPTER_INFO pInfo = NULL;
	ULONG len = 0;
	if (pInfo != NULL)
		delete (pInfo);
	unsigned  long nError;

	pInfo= (PIP_ADAPTER_INFO)malloc(len);
	nError = GetAdaptersInfo(pInfo, &len);

	if(nError == ERROR_BUFFER_OVERFLOW)
	{
		pInfo= (PIP_ADAPTER_INFO)malloc(len);
		nError	=	GetAdaptersInfo(pInfo,&len);
	}
	else
	{
		WRITELOG(_T("[CClientManager::GetAllIPaddress] len = %d"), len);
		WRITELOG(_T("[CClientManager::GetAllIPaddress] GetAdaptersInfo1 error = %uL"), nError);
	}

	if(nError == ERROR_SUCCESS)
	{
		while(pInfo)
		{
			if (pInfo!=NULL)
			{
				CString sTemp;

				PIP_ADDR_STRING pAddressList = &(pInfo->IpAddressList);
				sIPAddr	=_T("");
				do 
				{
					sIPAddr	+=	pAddressList->IpAddress.String;
					pAddressList = pAddressList->Next;
// 					if (pAddressList != NULL)
// 						sIPAddr	+=_T("\r\n");
					if((sIPAddr.Find(_T("0.0.0.0")) == -1) && (sIPAddr.Find(_T("127.0.0.1")) == -1))
					{
						sList += sIPAddr;
						sList += _T(',');
					}
				} while (pAddressList != NULL);
			}
			pInfo = pInfo->Next;
		}
	}
	else
	{
		WRITELOG(_T("[CClientManager::GetAllIPaddress] len = %d"), len);
		WRITELOG(_T("[CClientManager::GetAllIPaddress] GetAdaptersInfo2 error = %ul"), nError);
	}

	if(sList.GetLength()>0)
		sList.SetAt(sList.GetLength()-1, _T('\0'));

	WRITELOG(_T("[CClientManager::GetAllIPaddress] IP sList = %s"), sList);

	if (pInfo)
		free(pInfo);

	return sList;
}
*/

// 2010/01/20, avilon
// PC의 모든 MAC과 IP를 받아 온다.
CString CClientManager::GetMACAndIPAddr()
{
	CString sList = _T("");
	PIP_ADAPTER_INFO pInfo = NULL;
	ULONG len = sizeof(IP_ADAPTER_INFO);
	if (pInfo != NULL)
		delete (pInfo);
	unsigned  long nError;

	pInfo= (PIP_ADAPTER_INFO)malloc(len);
	nError = GetAdaptersInfo(pInfo, &len);

	if (nError == ERROR_BUFFER_OVERFLOW)
	{
		if (pInfo != NULL) delete (pInfo);
		pInfo= (PIP_ADAPTER_INFO)malloc(len);
		nError	=	GetAdaptersInfo(pInfo,&len);
	}
	else
	{
		WRITELOG(_T("[CClientManager::GetMACAndIPAddr] GetAdaptersInfo1 error = %ul"), nError);
	}
	PIP_ADAPTER_INFO pInfoOrg = pInfo; 
	if (nError == ERROR_SUCCESS)
	{
		USES_CONVERSION_EX;
		while (pInfo)
		{
			if (pInfo!=NULL)
			{
				CString sMacAddr = _T(""), sIPAddr = _T(""), sTemp = _T("");
				sMacAddr.Format(_T("%02X:%02X:%02X:%02X:%02X:%02X"),pInfo->Address[0],pInfo->Address[1],pInfo->Address[2],pInfo->Address[3],
					pInfo->Address[4],pInfo->Address[5]);

				PIP_ADDR_STRING pAddressList = &(pInfo->IpAddressList);
				do
				{
					sIPAddr.Format(_T("%s"), A2W_EX(pAddressList->IpAddress.String));
					pAddressList = pAddressList->Next;
					if ((sIPAddr.Find(_T("0.0.0.0")) == -1) && (sIPAddr.Find(_T("127.0.0.1")) == -1))
					{
						sTemp.Format(_T("%s-%s"), sMacAddr, sIPAddr);
						sList += sTemp;
						sList += _T(',');
					}
				} while (pAddressList != NULL);
			}
			pInfo = pInfo->Next;
		}
	}
	else
	{
		WRITELOG(_T("[CClientManager::GetMACAndIPAddr] GetAdaptersInfo2 error = %ul"), nError);
	}

	if(sList.GetLength() > 0)
	{
		sList.SetAt(sList.GetLength()-1, _T('\0'));
	}
	WRITELOG(_T("[CClientManager::GetMACAndIPAddr] IP sList = %s"), sList);

	if (pInfoOrg)
		free(pInfoOrg);

	return sList;
}

// 변경 여부 판단
BOOL CClientManager::CheckNICInfoChanged(CString sMacAndIP)
{
	WRITELOG(_T("[CheckNICInfoChanged]   ----------Start"));
	CString sMacIP;
	BOOL bRet = FALSE;
	int cnt = 0;

	if(sMacAndIP.GetLength() == m_sMACIPList.GetLength())
	{
		WRITELOG(_T("[CheckNICInfoChanged]   MAC, IP가 변경 되었는지 확인."));
		while(AfxExtractSubString(sMacIP, sMacAndIP, cnt, _T(',')))
		{
			CString sMacAddress, sIPAddress;
			AfxExtractSubString(sMacAddress, sMacIP, 0, _T('-'));
			// MAC부터 비교
			if(m_sMACIPList.Find(sMacAddress) > -1)
			{
				AfxExtractSubString(sIPAddress, sMacIP, 1, _T('-'));
				// IP 비교
				if(m_sMACIPList.Find(sIPAddress) == -1)
				{
					WRITELOG(_T("[CheckNICInfoChanged]   IP 변경.."));
					bRet = TRUE;
					break;
				}
			}
			else
			{
				WRITELOG(_T("[CheckNICInfoChanged]   MAC 변경.."));
				bRet = TRUE;
				break;
			}
			cnt++;
		}
	}
	else
	{
		WRITELOG(_T("[CheckNICInfoChanged] MAC, IP List 길이가 다르기 때문에 무조건 변경 되었다."));
		bRet = TRUE;
	}
	WRITELOG(_T("[CheckNICInfoChanged]   ----------End"));

	return bRet;
}

//------------------------------------------------------------   
// IDE 시리얼 알아내기 - 관련함수   
void CClientManager::ChangeByteOrder(PCHAR szString, USHORT uscStrSize)   
{
	USHORT i;
	CHAR temp;

	for (i = 0; i < uscStrSize; i+=2)   
	{
		temp = szString[i];
		szString[i] = szString[i+1];
		szString[i+1] = temp;
	}
}
//---------------------------------------------------------------------------  
// IDE 시리얼 알아내기
CString CClientManager::GetIDESerial()
{
	CString sResult, sModelNo, sDiskSN;
	SENDCMDINPARAMS in;
	uSENDCMDOUTPARAMS out;
	HANDLE hDrive;
	DWORD i;
	BYTE j;

	PIDSECTOR phdinfo;
	CHAR szBuf[41] = {0,};

	hDrive=CreateFile(_T("\\\\.\\PhysicalDrive0"),GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);   
	if (!hDrive)	return _T("");

	//Identify the IDE drives
	ZeroMemory(&in,sizeof(in));
	ZeroMemory(&out,sizeof(out));
	in.irDriveRegs.bDriveHeadReg=0xa0;

	in.irDriveRegs.bCommandReg=0xec;
	in.bDriveNumber = 0;
	in.irDriveRegs.bSectorCountReg=1;
	in.irDriveRegs.bSectorNumberReg=1;
	in.cBufferSize=512;
	if (!DeviceIoControl(hDrive,DFP_RECEIVE_DRIVE_DATA,&in,sizeof(in),&out,sizeof(out),&i,0))
	{
		CloseHandle(hDrive);
		return _T("");
	}

	phdinfo=(PIDSECTOR)out.bBuffer;
	memcpy(szBuf,phdinfo->sModelNumber,40);
	szBuf[40]=0;
	ChangeByteOrder(szBuf,40);
	USES_CONVERSION_EX;
	sModelNo = A2W_EX(szBuf);
	ZeroMemory(szBuf, 41);

	int nLen = sModelNo.GetLength();
	int nPos = 0;
	do 
	{
		if((sModelNo.GetAt(nPos)<0) || (sModelNo.GetAt(nPos)>127))
		{
			sModelNo = _T("");
			break;
		}
		nPos++;
	} while (nLen >= nPos);

	memcpy(szBuf,phdinfo->sSerialNumber,20);
	szBuf[20]=0;
	ChangeByteOrder(szBuf,20);
	sDiskSN = A2W_EX(szBuf);
	ZeroMemory(szBuf, 41);

	nPos = 0;
	nLen = sDiskSN.GetLength();

	do 
	{
		if((sDiskSN.GetAt(nPos)<0) || (sDiskSN.GetAt(nPos)>127))
		{
			sDiskSN = _T("");
			break;
		}
		nPos++;
	} while (nLen >= nPos);

	sResult = sModelNo.Trim() + _T(":") + sDiskSN.Trim();

	WRITELOG(_T("GetIDESerial : sResult = %s"), sResult);

	CloseHandle(hDrive);

	if ( sResult.IsEmpty() )
	{
		sResult = _T("");
	}

	return sResult;
}

BOOL CClientManager::RequestRunningProcessCheckList()
{
	// 2010/10/12, avilon - agentID를 얻어 오지 못 한 경우 agentinfo에서 다시 읽어 온다.
	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager] RequestRunningProcessCheckList Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;
		CString sValue = _T("");

		TCHAR pszLogPath[MAX_PATH];
		GetAgentLogPath(pszLogPath);
		CString sAgentIDPath;
		sAgentIDPath.Format(_T("%stmp\\SID_*"), pszLogPath);
		hFind = FindFirstFile(sAgentIDPath, &FindFileData);
		if(hFind != INVALID_HANDLE_VALUE)
		{
			sValue = FindFileData.cFileName;
			sValue = NumFilter(sValue);
			if((sValue.GetLength() > 0) && (sValue != _T("0")))
			{
				m_sAgentID = sValue;
				WRITELOG(_T("[CClientManager] RequestRunningProcessCheckList Agent 아이디를 다시 읽어 왔다. 값은 %s이다., sValue = %s"), m_sAgentID, sValue);

			}
			FindClose(hFind);
		}
	}
	CString sReq;	
	sReq.Format(_T("%s%c"), m_sAgentID, FIELD_SEPARATOR);

	CHAR *pszUTF8 = NULL;
	if(m_bConnectUTF8)
	{
		int nLen = WideCharToMultiByte(CP_UTF8, 0, sReq, sReq.GetLength(), NULL, 0, NULL, NULL);
		pszUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_UTF8, 0, sReq, sReq.GetLength(), pszUTF8, nLen, NULL, NULL);
		pszUTF8[nLen] = NULL;
	}
	else
	{
		int nLen = WideCharToMultiByte(CP_ACP, 0, sReq, sReq.GetLength(), NULL, 0, NULL, NULL);
		pszUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_ACP, 0, sReq, sReq.GetLength(), pszUTF8, nLen, NULL, NULL);
		pszUTF8[nLen] = NULL;
	}

	CStringA sTempReq = pszUTF8;
	if (pszUTF8)
		delete [] pszUTF8; pszUTF8 = NULL;
	sReq = UriEncode(sTempReq.GetBuffer()).c_str();

	CString sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::RequestRunningProcessCheckList()] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);

	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18005"));

	CStringW wsCmd = sCmd, wsReq = sReq;
	CString sResp = _T("");

	int ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);

	if (ret == 0)
	{
		m_listProcess.RemoveAll();
		if (sResp.GetLength() > 0)
		{
			int iCnt = 0;
			while (iCnt < 15)
			{
				CString sTemp = _T("");
				AfxExtractSubString(sTemp, sResp, iCnt, FIELD_SEPARATOR);
				if (sTemp.GetLength() == 0)
					break;

				m_listProcess.AddTail(sTemp);
				WRITELOG(_T("[CClientManager::RequestRunningProcessCheckList()] sTemp : %s"), sTemp);
				iCnt++;
			}
		}
		return TRUE;
	}
	WRITELOG(_T("*** RequestRunningProcessCheckList Fail!!"));
	return FALSE;
}

CString CClientManager::CheckRunningProcessList()
{
	WRITELOG(_T("[CClientManager::CheckRunningProcessList] 들어옴"));
	if (m_listProcess.IsEmpty())
	{
		return _T("");
	}

	DWORD dwTime = GetTickCount();

	CString sResult = _T("");
	for (int i=0; i<m_listProcess.GetCount(); i++)
		sResult += _T("0");

	CString sInternalName;
	TCHAR szProcessPath[MAX_PATH] = _T("");
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if( hSnap == INVALID_HANDLE_VALUE )
	{
		WRITELOG(_T("*** CheckRunningProcessList - INVALID_HANDLE_VALUE!!"));
		return sResult;
	} 
	if( !Process32First(hSnap, &pe32) )
	{
		WRITELOG(_T("*** CheckRunningProcessList - Process32First!!"));
		return sResult;
	}

	do
	{
		HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE,	FALSE, pe32.th32ProcessID);
		if (hProcess)
		{
			// 2011/11/17, avilon
			// Win_x64에서 64비트 프로세스의 정보를 못 가져 오는 현상 보완.
			if(m_OSVerMajor > 5)
			{
				typedef BOOL (CALLBACK* LPFNDLLFUNC) (HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD lpdwSize); 
				HINSTANCE hInst; 
				LPFNDLLFUNC pQueryFullProcessImageName;    // Function pointer 
				hInst = LoadLibrary((_T("kernel32.dll"))); 
				pQueryFullProcessImageName = (LPFNDLLFUNC)GetProcAddress(hInst, "QueryFullProcessImageNameW"); 
				DWORD dwSize = sizeof(szProcessPath);
				if(SUCCEEDED(pQueryFullProcessImageName(hProcess, 0, szProcessPath, &dwSize)))
				{
					FreeLibrary(hInst);
				}
				// QueryFullProcessImageName(hProcess, 0, szProcessPath, &dwSize);
			}
			else
			{
				GetModuleFileNameEx(hProcess, (HMODULE)pe32.th32ModuleID, szProcessPath, sizeof(szProcessPath)); 
			}
			CString sInternalName = GetFileVersionInfo(szProcessPath, _T("InternalName"));
		}
		else
		{
			WRITELOG(_T("[CheckRunningProcessList] OpenProcess Fail (ProcessName : %s) : %d"), pe32.szExeFile, GetLastError());
			sInternalName = _T("");
		}
// 		CString sProcessName = GetFileNameOnly(szProcessPath);
		CString sProcessName = pe32.szExeFile;
		//WRITELOG(_T("### ProcessName - %s, %s"), sInternalName, sProcessName);
		int iCnt = 0;
		CString sName;
		POSITION pos = m_listProcess.GetHeadPosition();
		while (pos)
		{
			sName = m_listProcess.GetNext(pos);
			//WRITELOG(_T("### Process - %s"), sName);
			if (sName.CompareNoCase(sInternalName) == 0 || sName.CompareNoCase(sProcessName) == 0)
			{
				sResult.SetAt(iCnt, _T('1'));
			}
			iCnt++;
		}
		CloseHandle(hProcess);
		ZeroMemory(szProcessPath, MAX_PATH);
	} while ( Process32Next(hSnap, &pe32) ); 

	CloseHandle(hSnap);

	WRITELOG(_T("### ProcessRun Result - %s"), sResult);
	return sResult;
}

int CClientManager::CheckLastOfflineTime(int limit_day)
{
	WRITELOG(_T("[CClientManager] CheckLastOfflineTime start"));
	int result = 0;

	TCHAR last_offline_buf[MAX_PATH];
	RegGetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_LAST_OFFLINE_TIME, last_offline_buf, MAX_PATH);
	CString sLast_Offline = last_offline_buf;
	WRITELOG(_T("[CClientManager] CheckLastOfflineTime Last Offline(%d)"), Str2Int(sLast_Offline));

	if(sLast_Offline.IsEmpty())
	{
		CString sLastOffline = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));
		WRITELOG(_T("[CClientManager] CheckLastOfflineTime 오프라인 시간이 설정되있지 않다. %s 설정."), sLastOffline);
		RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_LAST_OFFLINE_TIME, sLastOffline);
		result = -2;
	}
	else
	{
		WRITELOG(_T("[CClientManager] CheckLastOfflineTime 오프라인 시간이 설정되어있다."));
		DWORD Y = Str2Int(sLast_Offline.Left(4));
		DWORD m = Str2Int(sLast_Offline.Mid(4,2));
		DWORD d = Str2Int(sLast_Offline.Mid(6,2));
		DWORD H = Str2Int(sLast_Offline.Mid(8,2));
		DWORD M = Str2Int(sLast_Offline.Mid(10,2));
		DWORD S = Str2Int(sLast_Offline.Mid(12,2));
		CTime cLastofflineTime(Y, m, d, H, M, S);
		CTime cCurrentTime = CTime::GetCurrentTime();
		CTimeSpan cResultTime = cCurrentTime - cLastofflineTime;

		if(Str2Int(cResultTime.Format(_T("%D"))) >= limit_day)
		{
			WRITELOG(_T("[CClientManager] CheckLastOfflineTime 오프라인 지정 시간을 넘었다. 에이전트 삭제"));
			//AgentRemove();
			goto Exit_CheckOfflineTime;
		}
		else
		{
			WRITELOG(_T("[CClientManager] CheckLastOfflineTime 오프라인 지정 시간을 넘지 않았다."));
			result = -1;
			goto Exit_CheckOfflineTime;
		}
	}


Exit_CheckOfflineTime:

	WRITELOG(_T("[CClientManager] CheckLastOfflineTime end ret(%d)"), result);

	return result;
}

int CClientManager::CheckLimitOfflinePolicy()
{
	WRITELOG(_T("[CClientManager] CheckLimitOfflinePolicy start"));
	int nResult = 0;

	TCHAR limit_offline_buf[MAX_PATH];
	RegGetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_LIMIT_OFFLINE, limit_offline_buf, MAX_PATH);

	if(_tcslen(limit_offline_buf))
	{
		nResult = Str2Int(limit_offline_buf);
	}

	WRITELOG(_T("[CClientManager] CheckLimitOfflinePolicy end ret(%d)"), nResult);
	return nResult;
}

#include "installedsoftware.h"

// CString GetFileWriteTime(CString path)
// {
// 	HANDLE h_file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, 
// 		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
// 	if(h_file != INVALID_HANDLE_VALUE){
// 		FILETIME create_time, access_time, write_time;
// 
// 		// 지정된 파일에서 파일의 생성, 최근 사용 그리고 최근 갱신된 시간을 얻는다.
// 		GetFileTime(h_file, NULL, NULL, &write_time);
// 
// 		SYSTEMTIME write_system_time, write_local_time;
// 		// FILETIME 구조체정보를 SYSTEMTIME 구조체 정보로 변환한다. FILETIME 구조체에 들어있는 정보는
// 		// 우리가 직접적으로 이용하려면 계산이 복잡해지기 때문에 사용하기 편리한 SYSTEMTIME 구조체
// 		// 형식으로 변환해서 사용한다.
// 		FileTimeToSystemTime(&write_time, &write_system_time);
// 
// 		// FILETIME 구조체에서 SYSTEMTIME 구조체로 변환되면 시간정보가 UTC(Universal Time Coordinated) 형식의 
// 		// 시간이기 때문에 탐색기나 기타 프로그램에서 보는 시간과 다르다. 따라서 정확한 시간정보를 얻기 위해서
// 		// UTC 형식의 시간을 지역시간으로 변환해야 한다. 아래의 함수는 해당 작업을 하는 함수이다.
// 		SystemTimeToTzSpecificLocalTime(NULL, &write_system_time, &write_local_time);
// 
// 		// write_local_time 을 사용하면 된다..
// 
// 		CloseHandle(h_file);
// }
// // CreateFile에 GENERIC_READ 옵션을 사용하여 ReadMe.txt파일을 연다.
// 
// }



// by realhan, 2010/12/17 argment : CAtlList<CAtlString> *
 int CClientManager::GetInstalledProductsList(LPVOID p)
 {
 	WRITELOG(_T("[GetInstalledProductsList] 시작"));
 	if(IsBadWritePtr(p, sizeof(ATL_STRING_LIST)))
 	{
 		return BMS_ERROR_BAD_PTR;
 	}
 	
 	ATL_STRING_LIST *cList = (ATL_STRING_LIST *)p;
 	DWORD ret = 0;
 	TCHAR Sep = _T('|');
 	CInstalledSoftware apps;
	POSITION pos = NULL;
	pos = apps.m_cResultMap.GetStartPosition();
	while(pos)
	{
		CString key = _T(""), s_value = _T("");
		apps.m_cResultMap.GetNextAssoc(pos, key, s_value);
		
		CString Programs = _T(""), InstallDate = _T(""), Publisher = _T("");
		AfxExtractSubString(Programs, s_value, 0, FIELD_SEPARATOR);
		if (Programs.Find(_T("|")) > -1)
		{
			Programs.Replace(_T("|"), _T(" "));
		}
		AfxExtractSubString(InstallDate, s_value, 1, FIELD_SEPARATOR);
		AfxExtractSubString(Publisher, s_value, 2, FIELD_SEPARATOR);
		if (Publisher.Find(_T("|")) > -1)
		{
			Publisher.Replace(_T("|"), _T(" "));
		}
		CString result = _T("");
		result += Programs;
		result += (CString)Sep + InstallDate;
		result += (CString)Sep + Publisher;
		result += (CString)Sep + _T("install");
		cList->AddTail(result);
		ret++;
	}



// 	for (int i = 0; i < apps.cnt; i++)
// 	{
// 		CString result = "";
// 
////  		result += apps.m_aPrograms[i];
////  		result += (CString)Sep + apps.m_aInstallDate[i];
////  		result += (CString)Sep + apps.m_aPublisher[i];
////	 		result += (CString)Sep + "install";
//		
//		apps.m_cResultMap.GetCount()
//
//
// 		cList->AddTail(result);
// 		ret++;
// 	}
 
 	WRITELOG(_T("[GetInstalledProductsList] 완료 ret(%d)"), ret);
 	return ret;
 }

 int CClientManager::ProcessHardwareSpec()
 {
	 WRITELOG(_T("[ProcessHardwareSpec] 시작"));

	 CHardwareSpec hardwareSpec;
	 hardwareSpec.CollectHardwareInfo();
	 WRITELOG(_T("[ProcessHardwareSpec] 하드웨어 스펙 구함"));

	 CString sNewHardwareSpec = _T("");
	 sNewHardwareSpec = MakeHardwareSpecPacket(hardwareSpec);

	 CString sSavedHardwareSpec = _T("");
	 sSavedHardwareSpec = GetSavedHardwareSpec();

	 BOOL bSend = FALSE;
	 CString sSendData = _T("");

	 if(sSavedHardwareSpec == _T(""))
	 {
		 WRITELOG(_T("[ProcessHardwareSpec] 저장된 로그파일이 없음"));
		 if( sNewHardwareSpec!= _T("") )
		 {
			 sSendData = sNewHardwareSpec;
			 bSend = TRUE;
		 }
	 }
	 else
	 {
		 CString sDiffData = _T("");
		 sDiffData = GetDiffHardwareSpec(sSavedHardwareSpec, sNewHardwareSpec);
		 if(sDiffData != _T(""))
		 {
			 WRITELOG(_T("[ProcessHardwareSpec] 값이 달라짐 %s"), sDiffData);
			 sSendData = sDiffData;
			 bSend = TRUE;
		 }
	 }

	 if( bSend == TRUE )
	 {
		 WRITELOG(_T("[ProcessHardwareSpec] 로그 파일이 없거나 값이 달라져서 서버에 전송"));
		 if(SendHardwareSpec(sSendData) == 0)
		 {
			 if(CreateHardwareSpecFile(sNewHardwareSpec) == FALSE)
			 {
				 WRITELOG(_T("[ProcessHardwareSpec] CreateHardwareSpecFile() 실패"));
			 }
		 }
	 }
	 hardwareSpec.CleanUp();

	 WRITELOG(_T("[ProcessHardwareSpec] 종료"));
	 return TRUE; 
 }


 CString CClientManager::GetSavedHardwareSpec()
 {
	 TCHAR sFilePath[MAX_PATH] = {0,};
	 GetAgentPath(sFilePath, MAX_PATH);
	 CString sFullPathName = _T("");
	 sFullPathName.Format(_T("%s%s"), sFilePath, _T("HardwareSpec.log"));

	 CString sFileContents = _T("");
	 sFileContents = ReadStringFromFile(sFullPathName);

	 CString sDecryptData = _T("");
	 if(sFileContents != _T(""))
	 {
		 sDecryptData = DecryptData((LPTSTR)(LPCTSTR)sFileContents, BMS_AES_ENCRYPTKEY);
	 }

	 return sDecryptData;
 }


 BOOL CClientManager::CreateHardwareSpecFile(CString sHardwareSpecPacket)
 {
	 BOOL bRet = FALSE;
	 TCHAR sFilePath[MAX_PATH] = {0,};
	 GetAgentPath(sFilePath, MAX_PATH);
	 CString sFullPathName = _T("");
	 sFullPathName.Format(_T("%s%s"), sFilePath, _T("HardwareSpec.log"));

	 CString sEncryptData = _T("");
	 sEncryptData = EncryptData((LPTSTR)(LPCTSTR)sHardwareSpecPacket, BMS_AES_ENCRYPTKEY);
	 if(sEncryptData != _T(""))
	 {
		 if(CreateFileAndWrite(sFullPathName, sEncryptData) == 0)
		 {
			 bRet = TRUE;
		 }
	 }
	 return bRet;
 }

 #include "BMSCommunicate.h"
 int CClientManager::SendHardwareSpec(CString sSendData) 
 {
	 WRITELOG(_T("[CClientManager] SendHardwareSpec() 시작"));
	CString sReq = _T("");
	CString sResp = _T("");
	CString sServerURL = _T("");
	DWORD dwResult = 0;
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHW, sServerURL);
	WRITELOG(_T("[CClientManager::SendHardwareSpec()] - m_stPageInfo.m_GSHW - %s"), m_stPageInfo.m_GSHW);
	sReq.Format(_T("%s%s?cmd=18500&agentid=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHW : sPageName, m_sAgentID);
	WRITELOG(_T("[SendHardwareSpec] HardwareSpec Packet : %s"), sSendData);

	CString sRequestHeader = _T("");
	CWinHttpUtil cWinHttpUtil;
	sRequestHeader = cWinHttpUtil.MakeWinHttpRequestHeader(_T("Content-Type"), _T("application/x-www-form-urlencoded"), TRUE);

	CBMSCommunicate Communicate;
	CString sResponse = _T("");	
// 	dwResult = Communicate.SendPostDataAndReceive(m_wshttpServerUrl, (sPageName == _T("")) ? m_stPageInfo.m_GSHW : sPageName, m_httpPort, sReq, sSendData, sResponse, sRequestHeader, m_bIsHttps);
	dwResult = SendAndReceiveAddHeader((sServerURL == _T(""))? m_wshttpServerUrl : sServerURL, sReq, sSendData, sResponse, m_bIsHttps);
	if(dwResult != 0)
	{
		WRITELOG(_T("[CClientManager::SendHardwareSpec()] 목록 전달 실패 dwResult - sResponse : %d - %s"), dwResult, sResponse);
	}
	else
	{
		WRITELOG(_T("[CClientManager::SendHardwareSpec()] 목록 전달 성공"));
	}
	return dwResult;
 }

 int CClientManager::GetKeyAndValueToMap(CString sKeyValueData, CMap<CString, LPCTSTR, CString, LPCTSTR> * map)
 {
	int nkeyStartPos = 0 , nKeyEndPos = 0, nValueStartPos = 0, nValueEndPos = 0;
	CString sKey = _T(""), sValue = _T("");
	
	nkeyStartPos = sKeyValueData.Find(_T("&"), nKeyEndPos);
	while( nkeyStartPos != -1 )
	{		
		nKeyEndPos = sKeyValueData.Find(_T("="), nkeyStartPos);
		nkeyStartPos ++;
		sKey = sKeyValueData.Mid(nkeyStartPos, nKeyEndPos - nkeyStartPos);

		nValueStartPos = nKeyEndPos + 1;
		nValueEndPos = sKeyValueData.Find(_T("&"), nValueStartPos);
		if( nValueEndPos == -1 )
		{
			nValueEndPos = sKeyValueData.GetLength();
		}

		sValue = sKeyValueData.Mid(nValueStartPos, nValueEndPos - nValueStartPos);
		map->SetAt(sKey, sValue);

		nkeyStartPos = sKeyValueData.Find(_T("&"), nKeyEndPos);
	}

	return TRUE;
 }


 int CClientManager::ExtractDifference(CMap<CString, LPCTSTR, CString, LPCTSTR> * pOldMap, CMap<CString, LPCTSTR, CString, LPCTSTR> * pNewMap, CMap<CString, LPCTSTR, CString, LPCTSTR> * pExtractMap)
 {	
	int nIsSame = 0;
	CString sKey = _T(""), sValue = _T(""), sOldValue = _T("");

	POSITION pos = pNewMap->GetStartPosition();
	while(pos)
	{
		pNewMap->GetNextAssoc(pos, sKey, sValue);
		
		if(pOldMap->Lookup(sKey, sOldValue)) // key 잇으면 value검사
		{
			nIsSame = sValue.Compare(sOldValue);
		}
		else //key값이 없으면
		{
			nIsSame = -1;
		}
		if(nIsSame != 0)
		{
			pExtractMap->SetAt(sKey, sValue);
		}
	}
	return pExtractMap->GetCount();	
 }

 CString CClientManager::MakeHardwareSpecPacket(CHardwareSpec hardwareSpec)
 {	
	//CHardwareSpec tmpHardwareSpec = hardwareSpec;
	TCHAR chSep = _T('|');
	CString sResult = _T("");
	CString sAttach = _T("");

	sResult = _T("&") + AddGetMethodParameter(_T("mainboard"), hardwareSpec.GetMainboard(), TRUE);
	sResult += AddGetMethodParameter(_T("processor"), hardwareSpec.GetProcessor(), TRUE);
	sResult += AddGetMethodParameter(_T("physicalmemory"), hardwareSpec.GetPhysicalMemory(), TRUE);


	for(int i = 0; i < hardwareSpec.GetGraphicCardCnt(); i++)
	{
		sAttach = AttachMultipleValue(sAttach, hardwareSpec.GetGraphicCard(i), chSep);
	}
	sResult += AddGetMethodParameter(_T("graphiccard"), sAttach, TRUE);
	sAttach = _T("");

	for(int i = 0; i < hardwareSpec.GetNetworkCardCnt(); i++)
	{
		sAttach = AttachMultipleValue(sAttach, hardwareSpec.GetNetworkCard(i), chSep);
	}
	sResult += AddGetMethodParameter(_T("networkcard"), sAttach, TRUE);
	sAttach = _T("");

	sAttach.Format(_T("%I64d"), hardwareSpec.GetHardDiskTotalSize());
	for(int i = 0; i < hardwareSpec.GetHarddiskCnt(); i++)
	{
		sAttach = AttachMultipleValue(sAttach, hardwareSpec.GetHarddisk(i), chSep);
	}
	sResult += AddGetMethodParameter(_T("harddisk"), sAttach, TRUE);

	sResult += AddGetMethodParameter(_T("systemmodel"), hardwareSpec.GetSystemmodel(), TRUE);
	sResult += AddGetMethodParameter(_T("os"), hardwareSpec.GetOS(), TRUE);
	sResult += AddGetMethodParameter(_T("wininstalldate"), hardwareSpec.GetWinInstalldate(), FALSE);

	return sResult;
 }

 CString CClientManager::AttachMultipleValue(CString sFirstValue, CString sAttahValue, TCHAR chSeperator)
 {
	 CString sAttachedValue = _T("");
	 if(sFirstValue != _T(""))
	 {
		 sAttachedValue.Format(_T("%s%c%s"), sFirstValue, chSeperator, sAttahValue);
	 }
	 else
	 {
		 sAttachedValue = sAttahValue;
	 }
	 return sAttachedValue;
 }

 CString CClientManager::MakeNameValueFormat(CString sName, CString sValue)
 {
	 CString sData = _T("");

	 if(sName != _T(""))
	 {
		 sData.Format(_T("&%s=%s"), sName, sValue);
	 }
	return sData;
 }

 CString CClientManager::LoadLanString(UINT uID)
 {
	 return ::LoadLanString(uID, m_LangID);
 }

void CClientManager::CheckEncryptCommini(TCHAR *dec_path )
{
	CString sDecryptedData = _T("");
	sDecryptedData = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigData(BMS_COMMINI_FILENAME);

	CString sSectionName = _T("");
	sSectionName.Format(_T("%s\r\n"), BMS_ENVIRONMENT_FILE_COMMON_SECTION);
	sDecryptedData.Replace(sSectionName, _T(""));

	GetAgentLogPath(dec_path, MAX_PATH);
	TCHAR szFileName[MAX_PATH] = {0, };
	_sntprintf(szFileName, MAX_PATH-sizeof(TCHAR), _T("%s_%06d"), BMS_COMMINI_DECRYPT_FILE_NAME, GetTickCount());
	_tcsncat(dec_path, szFileName, _tcslen(dec_path));

	WRITELOG(_T("[CheckEncryptCommini] sDecryptData : %s"), sDecryptedData);
	if (MultiLanguageWriteFile(dec_path, sDecryptedData, CHARSET_UNICODE) == TRUE)
	{
		WRITELOG(_T("[CheckEncryptCommini] 임시 DecComm 생성 성공 dec_path(%s)"), dec_path);
	}
	else
	{
		WRITELOG(_T("[CheckEncryptCommini] 임시 DecComm WriteFile 실패. dec_path(%s), err(%d)"), dec_path, GetLastError());
	}
}

int CClientManager::CreateGEDFile(CString sFilePath, CString sEncryptedData)
{
	int ret = 0;
	HANDLE hPlicy = INVALID_HANDLE_VALUE;
	
	if(sEncryptedData.IsEmpty() || sFilePath.IsEmpty())
	{
		WRITELOG(_T("[CreatePolicyGEDFile] 암호화 데이터 NULL."));
		ret = -1;
		goto CleanUp;
	}

	hPlicy = CreateFile(sFilePath, GENERIC_ALL, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, NULL, NULL);
	if(hPlicy !=INVALID_HANDLE_VALUE)
	{
		DWORD cbWrite =0;
		if(hPlicy != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hPlicy);
			hPlicy = INVALID_HANDLE_VALUE;
		}

		//if(!WriteFile(hPlicy, enc_data, enc_data.GetLength(), &cbWrite, NULL))
		if(!MultiLanguageWriteFile(sFilePath, sEncryptedData, CHARSET_UTF8))
		{
			WRITELOG(_T("[CreatePolicyGEDFile] WriteFile 실패 err(%d)"), GetLastError());
			ret = -2;
			goto CleanUp;
		}
	}
	else
	{
		WRITELOG(_T("[CreatePolicyGEDFile] CreateFile 실패 err(%d), path : %s"), GetLastError(), sFilePath);
		ret = -3;
		goto CleanUp;
	}

CleanUp:
	if(hPlicy != INVALID_HANDLE_VALUE) CloseHandle(hPlicy);
	return ret;
}

CString CClientManager::ReadStringFromFile(CString file_path)
{
	CString ret = _T("");
	HANDLE hPlicyGED = INVALID_HANDLE_VALUE;
	TCHAR *pBuf = 0;

	if(file_path.IsEmpty())
	{
		goto CleanUp;
	}

	if(FileExists(file_path))
	{
		DWORD size = GetFileSize(file_path);
		hPlicyGED = CreateFile(file_path, GENERIC_ALL, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);

		pBuf = new TCHAR[size + 1];
		ZeroMemory(pBuf, (size + 1)*sizeof(TCHAR));
		*pBuf = 0;
		DWORD cbRead =0;
		if(hPlicyGED !=INVALID_HANDLE_VALUE)
		{
			if(!ReadFile(hPlicyGED, pBuf, size, &cbRead, NULL))
			{
				WRITELOG(_T("[ReadStringFromFile] GED 정책 파일. WriteFile 실패 err(%d)"), GetLastError());
				goto CleanUp;
			}
		}
		else
		{
			WRITELOG(_T("[ReadStringFromFile] GED 정책 파일. CreateFile 실패 err(%d)"), GetLastError());
			goto CleanUp;
		}

		pBuf[cbRead] = 0;
		ret = pBuf;
	}

CleanUp:

	if(hPlicyGED != INVALID_HANDLE_VALUE) CloseHandle(hPlicyGED);
	if(pBuf)
	{
		delete [] pBuf;
		pBuf = NULL;
	}

	return ret;
}

BOOL CClientManager::SendAlarmToDiscoveryUI(BOOL bIsAgent)
{
	// TODO : 디스커버리 모듈에게 임시정책 목록이 있음을 전달.
 	HWND hWnd = ::FindWindow(_T("#32770"), BMDC_MAIN_UI_WND_TITLE);
 	if(hWnd)
 	{
		WRITELOG(_T("CClientManager::SendAlarmToDiscoveryUI - 임시정책 리스트 업데이트"));
 		::PostMessage(hWnd, BMDC_REFRESH_TEMPLIST, NULL, NULL);
		return TRUE;
 	}
	return FALSE;
}

void CClientManager::RequestDiscoveryInstantPolicy(BOOL bIsStopInstant)
{
	CString sCmd, sReceived = _T(""), policy_enc_data = _T(""), sServerURL = _T("");
	CString sReq = m_sAgentID;
	CString sPageName = GetServerInfo(m_stPageInfo.m_GDSALLOW, sServerURL);
	WRITELOG(_T("[CClientManager::RequestDiscoveryInstantPolicy()] - m_stPageInfo.m_GDSALLOW - %s"), m_stPageInfo.m_GDSALLOW);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GDSALLOW : sPageName, !bIsStopInstant?_T("19205"):_T("19207"));
	int ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, m_bIsHttps);
	if(sReceived.GetLength() == 0)
	{
		WRITELOG(_T("CClientManager::RequestDiscoveryInstantPolicy - 정책을 받지 못함"));
		return;
	}

	if(!ret && (sReceived.GetLength()>0))
	{
		// 2011/11/30, avilon - 거절 목록과 승인 목록을 나눠 저장한다.
		CString sDenyList = _T(""), sAllowList = _T(""), sTmpList;
		int nPos = 0;
		while(AfxExtractSubString(sTmpList, sReceived, nPos, _T('\n')))
		{
			if(sTmpList.GetLength() > 0)
			{
				CString sCompare = _T("");
				AfxExtractSubString(sCompare, sTmpList, 7, FIELD_SEPARATOR);

				if(sCompare.CompareNoCase(_T("DENY"))==0)
				{
					sDenyList += sTmpList;
					sDenyList += _T('\n');
				}
				else
				{
					sAllowList += sTmpList;
					sAllowList += _T('\n');
				}
			}

			nPos++;
		}

		// Deny List
		if(sDenyList.GetLength() > 0)
		{
			WRITELOG(_T("CClientManager::RequestDiscoveryInstantPolicy DENY List - %s"), sDenyList);
			TCHAR szDsPolicyPath[MAX_PATH] = {0,};
			GetDiscoveryUsePath(szDsPolicyPath, MAX_PATH);
			_tcsncat(szDsPolicyPath, BMDC_DENIED_INSTANT_POLICY_FILENAME, MAX_PATH);

			CString sPreList = MultiLanguageReadFile(szDsPolicyPath, CHARSET_UTF8);
			sPreList = DecryptData((LPTSTR)(LPCTSTR)sPreList, BMS_AES_ENCRYPTKEY);

			sPreList += sDenyList;

			policy_enc_data = EncryptData((LPTSTR)(LPCTSTR)sPreList, BMS_AES_ENCRYPTKEY);

			if(MultiLanguageWriteFile(szDsPolicyPath, policy_enc_data, CHARSET_UTF8))
			{
				HWND hWnd = ::FindWindow(_T("#32770"), BMDC_MAIN_UI_WND_TITLE);
				if(hWnd)
				{
					::PostMessage(hWnd, BMDC_DENY_TEMPPOLICY, NULL, NULL);
					WRITELOG(_T("CClientManager::RequestDiscoveryInstantPolicy - BMDC_DENY_TEMPPOLICY 메시지 전송."));
				}
			}
		}

		// Allow+Stop List
	
		WRITELOG(_T("[CClientManager::RequestDiscoveryInstantPolicy] Allow List - %s"), sAllowList);
		if (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BM_DISCOVERY_INSTANT_POLICY_FILENAME, sAllowList, _T("")) == TRUE)
		{
			HWND hWnd = ::FindWindow(_T("#32770"), BMDC_DOC_CHECKER_WND_TITLE);
			if(hWnd)
			{
				::PostMessage(hWnd, BMDC_REFRESH_TEMPLIST, NULL, NULL);
				WRITELOG(_T("CClientManager::RequestDiscoveryInstantPolicy - BMDC_REFRESH_TEMPLIST 메시지 전송."));
			}
		}
	}
}

void CClientManager::RequestDiscoveryDeleteList()
{
	CString sPageName = _T("gdsfilelist2.1.jsp"), sCmd = _T(""), sReceived = _T(""), sReq = _T("");
	sReq = m_sAgentID;
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, sPageName, _T("19403"));
	int ret = SendAndReceive(m_wshttpServerUrl, sCmd, sReq, sReceived, m_bIsHttps);

	TCHAR szAgentPath[MAX_PATH] = {0, };
	GetAgentPath(szAgentPath, MAX_PATH);

	CString sDiscoveryDeleteFileListPath = _T(""), sEncryptedData = _T("");
	sDiscoveryDeleteFileListPath.Format(_T("%s%s"), szAgentPath, BMDC_DELETE_FILE_LIST_FILENAME);
	sEncryptedData = EncryptData((LPTSTR)(LPCTSTR)sReceived, BMS_AES_ENCRYPTKEY, ENC_TYPE_KLIB);
	WRITELOG(_T("[CClientManager::RequestDiscoveryDeleteList] file : %s"), sDiscoveryDeleteFileListPath);
	if (MultiLanguageWriteFile(sDiscoveryDeleteFileListPath, sEncryptedData, CHARSET_UTF8) == TRUE)
	{
		HWND hWnd = ::FindWindow(_T("#32770"), BMDC_DOC_CHECKER_WND_TITLE);
		if(hWnd)
		{
			::PostMessage(hWnd, BMDC_DELETE_FILE, NULL, NULL);
			WRITELOG(_T("[CClientManager::RequestDiscoveryDeleteList] - BMDC_DELETE_FILE 메시지 전송."));
		}
		else
		{
			WRITELOG(_T("[CClientManager::RequestDiscoveryDeleteList] FindWindow 실패 Err : %d"), GetLastError());
		}
	}
	else
	{
		WRITELOG(_T("[CClientManager::RequestDiscoveryDeleteList] Write File Err : %d"), GetLastError());
	}
}

void CClientManager::RequestDiscoverySyncList()
{
	CString sPageName = _T("gdsfilelist2.1.jsp"), sCmd = _T(""), sReceived = _T("");
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, sPageName, _T("19401"));
	int ret = SendAndReceive(m_wshttpServerUrl, sCmd, m_sAgentID, sReceived, m_bIsHttps);

	TCHAR szAgentPath[MAX_PATH] = {0, };
	GetAgentPath(szAgentPath, MAX_PATH);

	CString sDiscoverySyncFileListPath = _T(""), sEncryptedData = _T("");
	sDiscoverySyncFileListPath.Format(_T("%s%s"), szAgentPath, BMDC_SYNC_FILE_LIST_FILENAME);
	sEncryptedData = EncryptData((LPTSTR)(LPCTSTR)sReceived, BMS_AES_ENCRYPTKEY, ENC_TYPE_KLIB);
	if (MultiLanguageWriteFile(sDiscoverySyncFileListPath, sEncryptedData, CHARSET_UTF8) == TRUE)
	{
		HWND hWnd = ::FindWindow(_T("#32770"), BMDC_DOC_CHECKER_WND_TITLE);
		if(hWnd)
		{
			::PostMessage(hWnd, BMDC_UPDATE_SYNC, NULL, NULL);
			WRITELOG(_T("[CClientManager::RequestDiscoverySyncList] - BMDC_UPDATE_SYNC 메시지 전송."));
		}
	}
}

void CClientManager::RequestDiscoveryManagerForcedRescan()
{
	HWND hWnd = ::FindWindow(_T("#32770"), BMDC_DOC_CHECKER_WND_TITLE);
	if(hWnd)
	{
		::PostMessage(hWnd, BMDC_MANGER_FORCED_RESCAN_START, NULL, NULL);
		WRITELOG(_T("[CClientManager::RequestDiscoveryManagerForcedRescan] - BMDC_MANGER_FORCED_RESCAN_START 메시지 전송."));
	}
}

void CClientManager::ProcessDefaultLook(CString sLook, CString sDT)
{
	// 2009/08/12 by avilon
	// 9번 Look을 가장 먼저 검사한 후 0이면 다음 작업을 진행한다.
	if (sLook.GetAt(9) != _T('0'))
	{
		// 서버 트래픽 관리
		WRITELOG(_T("9 번째 1"));
		if (g_pBMSCommAgentDlg->KillTimer(MONITORING_TIMER_ID) == TRUE)
		{
			g_pBMSCommAgentDlg->m_bMonitoringTimer = FALSE;
		}

		m_bTraffic = TRUE;
		return;
	}
	else
	{
		WRITELOG(_T("9 번째 0"));
		if (m_bTraffic == TRUE)
		{
			if (g_pBMSCommAgentDlg->SetTimer(MONITORING_TIMER_ID, MONITORING_TIME_INTERVAL * 1000, NULL))
			{
				g_pBMSCommAgentDlg->m_bMonitoringTimer = TRUE;
			}
			m_bTraffic = FALSE;
		}
	}

	if (sLook.GetAt(0) != _T('0'))
	{
		WRITELOG(_T("0 번째 1"));
		// 정책이 변경되었다.
		RequestPolicies();
		if (m_bUseAsset == TRUE)
		{
			RequestAssetPolicies(); // 2015.03.27, keede122 DLP 정책이 바뀔때 자산관리 정책도 같이 변경되었는지 확인한다.
		}
		return;
	}
	else if (sLook.GetAt(12) != _T('0'))
	{
		WRITELOG(_T("12 번째(임시 정책 해지) - 1"));
		GetCancelInstantList(m_sAgentID);
		SendReactTrayRefresh();
		UpdateLook(12, 0);
	}
	else if (sLook.GetAt(1) != _T('0'))
	{
		// 중지 명령이 감지되었다.
		WRITELOG(_T("1 번째 1"));
		AgentPause();
		UpdateLook(1, 0);
	}
	else if (sLook.GetAt(2) != _T('0'))
	{
		// 재시작 명령이 감지되었다.
		WRITELOG(_T("2 번째 1"));
		AgentResume();
		UpdateLook(2, 0);
	}
	else if (sLook.GetAt(3) != _T('0'))
	{
		// 제거 명령이 감지되었다.
		WRITELOG(_T("3 번째 1"));
		AgentRemove(g_sBMSRemoveParam[BMS_REMOVE_FROM_LOOK]);
		UpdateLook(3, 0);
	}
	else if (sLook.GetAt(4) != _T('0'))
	{
		// 업그레이드 명령이 감지되었다.
		WRITELOG(_T("4 번째 1"));
		if (g_pBMSCommAgentDlg->m_bUserSwitch == FALSE)
		{
			AgentUpgrade();
		}
		else
		{
			WRITELOG(_T("[ClientManager::LookServer] 사용자 전환 기능에서는 강제 업그레이드를 진행하지 않는다!!"));
		}

		UpdateLook(4, 0);
	}
	else if (sLook.GetAt(5) != _T('0'))
	{
		// 잠금 풀기 명령이 감지되었다.
		WRITELOG(_T("5 번째 1"));
		UnlockPCToReactionAgent();
		UpdateLook(5, 0);
	}
	else if (sLook.GetAt(6) != _T('0'))
	{
		WRITELOG(_T("6 번째 1"));
		g_pBMSCommAgentDlg->SetInstantLevelTimer(sDT);
		RequestPolicies();
		UpdateLook(6, 0);
		// 임시 정책이 설정되었다.
	}
	else if (sLook.GetAt(7) != _T('0'))
	{
		// 임시정책 요청자
		WRITELOG(_T("7 번째 1"));
		if (!RequestAllowList(m_sAgentID, TRUE))
		{
			if (m_bGSALLOW_62 == TRUE)
			{
				UpdateInstantLevelEndDate();
				CreateInstantLevel();
			}
			else
			{
				StartInstantPolicies();
			}
			SendReactTrayRefresh();
			g_pBMSCommAgentDlg->SetTimer(ALLOW_INSTANTLEVEL_TIMER_ID, 1000*60, NULL);
			UpdateLook(7, 0);
		}

		// UpdateLook(7, 0);
	}
	else if (sLook.GetAt(8) != _T('0'))
	{
		// 임시정책 승인자
		WRITELOG(_T("8 번째 1"));
		int nResult = 0;
		nResult = RequestAllowList(m_sAgentID, FALSE);
		if (nResult <= 0)
		{
			if (nResult == -4)
			{
				SendReactTrayRefresh(TRAY_FLAG_REQUEST_CANCEL);
			}
			else
			{
				SendReactTrayRefresh();
			}

			UpdateLook(8, 0);
		}
	}
	else if (sLook.GetAt(10) != _T('0'))
	{
		WRITELOG(_T("10 번째(ProcessList) - 1"));
		if (m_bUseAsset == TRUE)			// 2015.03.19, keede122
		{
			POSITION pos = m_prcList.GetHeadPosition();
			while (pos)
			{
				PPRCLIST prc = (PPRCLIST)m_prcList.GetAt(pos);
				if (prc->sAgentProcess == BM_ASSETMAN_PROCESS_NAME || prc->sAgentProcess == BM_ASSETMAN_x64_PROCESS_NAME)
				{
					HWND hWnd = ::FindWindow(_T("#32770"), prc->sAgentWnds);
					if(hWnd != NULL)
					{
						WRITELOG(_T("[CClientManager::LookServer] - 자산 관리 실행상황 메세제 보냄."));
						::PostMessage(hWnd, BMSM_ASSET_PROCESS_LIST, (WPARAM)NULL, (LPARAM)NULL);
						ProcMessage(); 
					}
					else
					{
						hWnd = ::FindWindow(NULL, prc->sAgentWnds);
						if(hWnd != NULL)
						{
							WRITELOG(_T("[CClientManager::LookServer] - 자산 관리 실행상황 메세지 보냄."));
							::PostMessage(hWnd, BMSM_ASSET_PROCESS_LIST, (WPARAM)NULL, (LPARAM)NULL);
							ProcMessage();
						}
					}
				}
				m_prcList.GetNext(pos);
			}
			UpdateLook(10, 0);
		}
		else
		{
			// 프로세스 리스트 업데이트 호출
			m_bRequestRunningProcess = RequestRunningProcessCheckList();
			if (m_bRequestRunningProcess == TRUE)
			{
				UpdateLook(10, 0);
			}
		}
	}
	else if (sLook.GetAt(11) != _T('0'))
	{
		WRITELOG(_T("11 번째(설치 프로그램 목록 전달) - 1"));
		SendInstalledProgramList();
		UpdateLook(11, 0);
	}
	else if (sLook.GetAt(13) != _T('0'))
	{
		WRITELOG(_T("13 사용자 기반 정책 목록 - 1"));

		if(RequestUserBaseConfig())
		{
			UpdateLook(13, 0);
		}
	}
	else if (sLook.GetAt(14) != _T('0'))
	{
		WRITELOG(_T("14 번째 1"));
		if (WindowsUpdateForce())
		{
			UpdateLook(14, 0);
		}
	}
	else if (sLook.GetAt(15) != _T('0'))
	{
		WRITELOG(_T("15 번째 1"));
		BMSSendPolicy();
		UpdateLook(15, 0);
	}
}

void CClientManager::ProcessNewLook(CString sLook)
{
	if (sLook.GetAt(36) != _T('0'))
	{
		WRITELOG(_T("36 번째 1"));
		ProcessHardwareSpec();
		UpdateLook(36, 0);
	}
	else if (sLook.GetAt(37) != _T('0'))
	{
		WRITELOG(_T("37 번째 1"));
		if (m_sHashedPolicy != _T(""))
		{
			BOOL bCompareHash = FALSE;
			bCompareHash = CompareHashData(GetHashFromPolicyIntegrityFile(), m_sHashedPolicy);
			SendPolicyIntegrityResultToServer(bCompareHash);
		}
		UpdateLook(37, 0);
		AgentRestart(); // 위치..
	}
	else if (sLook.GetAt(38) != _T('0'))
	{
		if (m_OfflinePolicy == BM_OFFLINE_POLICY_SELECT_LEVEL)
		{
			RequestSelectedOfflinePolicy();
		}
		UpdateLook(38, 0);
	}
	else if (sLook.GetAt(39) != _T('0'))
	{
		WRITELOG(_T("39 번째 1"));
		if (RequestMessageSendToUser() == TRUE)
		{
			UpdateLook(39, 0);
		}
	}
	// PC보안감사 by yoon.2019.01.08.
	else if (sLook.GetAt(40) != _T('0'))
	{
		// 정책 변경
		WRITELOG(_T("40 번째 1"));

		if (m_bUsePCSecu == TRUE)
		{
			RequestPoliciesPCSecu();
		}
		UpdateLook(40, 0);
		return;
	}
	else if (sLook.GetAt(41) != _T('0'))
	{
		// PC정보요청
		WRITELOG(_T("41 번째 1"));

		if (m_bUsePCSecu == TRUE)
		{
			RequestResultPCSecu();
		}
		UpdateLook(41, 0);
	}
	else if (sLook.GetAt(42) != _T('0'))
	{
		// 사용자 기반 공용PC 강제 로그아웃
		WRITELOG(_T("42 번째 1"));
		UserBasedForceLogout();
		UpdateLook(42, 0);
	}
	else if (sLook.GetAt(43) != _T('0'))
	{
		WRITELOG(_T("43번째 1"));
		g_pBMSCommAgentDlg->m_bReceiveInternalList = FALSE;
		// 다시 정책을 요청하도록 처리한다.
		g_pClientManager->m_bPolicyReceived = FALSE;
		g_sEncryptPolicy = _T("");
		UpdateLook(43, 0);
	}
	else if (sLook.GetAt(44) != _T('0'))
	{
		WRITELOG(_T("44번째 1"));
		if (g_pClientManager->m_bHardwareDvice == TRUE)
		{
			RequestPoliciesHardwareDevice();
		}
		UpdateLook(44, 0);
	}
	else if (sLook.GetAt(45) != _T('0'))
	{
		WRITELOG(_T("45번째 1"));
		if (ExdsList(EXDS_ALLOW_TYPE) == 0)
		{
			UpdateLook(45, 0);
		}
		
	}
	else if (sLook.GetAt(46) != _T('0'))
	{
		WRITELOG(_T("46번째 1"));
		if (ExdsList(EXDS_REQ_TYPE) == 0)
		{
			UpdateLook(46, 0);
		}
	}
}

void CClientManager::ProcessDiscoverLook(CString sLook)
{
	if (m_bUseDiscovery == FALSE)
	{
		WRITELOG(_T("디스커버리를 사용하지 않는다."));
		return;
	}

	// 16 디스커버리 정책변경
	if (sLook.GetAt(16) != _T('0'))
	{
		WRITELOG(_T("16 번째(디스커버리 정책 변경)"));
		RequestDiscoveryPolicy();
		//RequestEncryptKeyValue();
	}
	// 17 디스커버리 중지
	else if (sLook.GetAt(17) != _T('0'))
	{
		WRITELOG(_T("17 번째(디스커버리 중지)"));
		RequestPauseDiscovery();
		UpdateLook(17, 0);
	}
	// 18 디스커버리 재시작
	else if (sLook.GetAt(18) != _T('0'))
	{
		WRITELOG(_T("18 번째(디스커버리 재시작)"));
		RequestDiscoveryPolicy();
		//RequestEncryptKeyValue();
		UpdateLook(18, 0);
	}
	// 19 디스커버리 제거
	else if (sLook.GetAt(19) != _T('0'))
	{
		// TODO : 디스커버리 제거 작업 - 디스커버리만 삭제 하느냐, 그라디우스까지 삭제하느냐..중복 되는 모듈 처리..
		WRITELOG(_T("19 번째(디스커버리 제거)"));
		UpdateLook(19, 0);
	}
	// 20 디스커버리 임시정책 요청자(관리자에 의한 승인/거절 결과)
	else if (sLook.GetAt(20) != _T('0'))
	{
		WRITELOG(_T("20 번째(디스커버리 임시정책 요청자)"));
		if(SendAlarmToDiscoveryUI(TRUE))
		{
			RequestDiscoveryInstantPolicy();
			UpdateLook(20, 0);
		}
	}
	// 21 디스커버리 임시정책 승인자(요청자에 의한 임시정책 요청 목록 존재)
	else if (sLook.GetAt(21) != _T('0'))
	{
		WRITELOG(_T("21 번째(디스커버리 임시정책 승인자)"));
		if(SendAlarmToDiscoveryUI(FALSE))
			UpdateLook(21, 0);
	}
	// 22 디스커버리 임시정책 요청
	else if (sLook.GetAt(22) != _T('0'))
	{
		WRITELOG(_T("22 번째(디스커버리 임시정책 요청)"));
		RequestDiscoveryInstantPolicy();
		UpdateLook(22, 0);
	}
	// 23 디스커버리 임시정책 중지
	else if (sLook.GetAt(23) != _T('0'))
	{
		WRITELOG(_T("23 번째(디스커버리 임시정책 중지)"));
		RequestDiscoveryInstantPolicy(TRUE);
		UpdateLook(23, 0);
	}
	else if (sLook.GetAt(24) != _T('0'))
	{
		WRITELOG(_T("24 번째(디스커버리 동기화)"));
		RequestDiscoverySyncList();
		UpdateLook(24, 0);
	}
	else if (sLook.GetAt(25) != _T('0'))
	{
		WRITELOG(_T("25 번째(디스커버리 관리자 문서 삭제)"));
		RequestDiscoveryDeleteList();
	}
	else if (sLook.GetAt(26) != _T('0'))
	{
		WRITELOG(_T("26 번째(디스커버리 관리자 강제 재스캔"));
		RequestDiscoveryManagerForcedRescan();
		UpdateLook(26, 0);
	}
}

void CClientManager::ProcessTakoutLook(CString sLook)
{
	if(sLook.GetLength() < NEW_SERVER_LOOK_RESP_LENGTH)
	{
		WRITELOG(_T("[ProcessTakoutLook] 새로 생성된 Takeout Look이 아니거나 길이가 다르다. sLook(%s)"), sLook);
		return;
	}

	// 32번째 - 요청목록 - 승인자
	// 33번째 - 승인/거절 목록 - 요청자
	if(sLook.GetAt(32) != _T('0'))
	{
		WRITELOG(_T("32번째 - %c"), sLook.GetAt(32));
		SendReactTrayRefresh(TRAY_FLAG_PCTAKEOUT_MANAGER);
		// UpdateLook(32, 0);
	}
	else if(sLook.GetAt(33) != _T('0'))
	{
		WRITELOG(_T("33번째 - %c"), sLook.GetAt(33));
		SendReactTrayRefresh(TRAY_FLAG_PCTAKEOUT_CLIENT);
		UpdateLook(33, 0);
	}
	else if(sLook.GetAt(34) != _T('0'))
	{
		// UpdateLook(34, 0);
	}
	else if(sLook.GetAt(35) != _T('0'))
	{
		// UpdateLook(35, 0);
	}
}

BOOL CClientManager::RequestUserBaseConfig()
{
	BOOL bRet = FALSE;
	WRITELOG(_T("[RequestUserBaseConfig] 시작"));

	CString sReq = _T(""), sCmd = _T(""), sResp = _T(""), sSub = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSUSER, sServerURL);
	WRITELOG(_T("[CClientManager::RequestUserBaseConfig()] - m_stPageInfo.m_GSUSER - %s"), m_stPageInfo.m_GSUSER);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSUSER : sPageName, _T("18504"));
	// Send = gsuser6.3.jsp?cmd=18504&packet=5 // 1. 에이전트 아이디
	// Resp = 010 // 1. 사용유무. 2. 로그아웃시간.
	int ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, m_sAgentID, sResp, m_bIsHttps);
	WRITELOG(_T("[RequestUserBaseConfig]  ret : %d sResp : %s"), ret, sResp);

	if(!ret)
	{
		bRet = TRUE;
		CString sSubString = _T("");
		AfxExtractSubString(sSubString, sResp, 0, g_Separator);
		g_pBMSCommAgentDlg->m_bUserBase = _ttoi(sSubString);
		AfxExtractSubString(sSubString, sResp, 1, g_Separator);
		m_dwUserBaseLogoutTime = _ttoi(sSubString) * (60 * 1000);

		if(g_pBMSCommAgentDlg->m_bUserBase)
		{
			WRITELOG(_T("[RequestUserBaseConfig] 사용자 기반 ON!!! %d, %d"), g_pBMSCommAgentDlg->m_bUserBase, m_dwUserBaseLogoutTime);

			g_pBMSCommAgentDlg->SetTimer(USERBASE_MONITORING_TIMER_ID, 10 * 1000, NULL);

			CString sUserId = ReadUserInfoFile(USERINFO_LOGINID);
			if (!sUserId.IsEmpty())
			{
				WRITELOG(_T("[RequestUserBaseConfig] 이미 로그인 되어있다!! 빠져나간다!!"));
				return bRet;
			}

			DeleteUserIDFile();
			CreateUserBaseFlagFile();

			TCHAR agent_path[MAX_PATH] = {0, };
			GetAgentPath(agent_path, MAX_PATH);

			CAtlString sAgentPath = _T("");
			sAgentPath = agent_path;
			sAgentPath += BM_LOGIN_PROCESS_NAME;

			HWND hWnd = ::FindWindow(_T("#32770"), BM_LOGIN_WND_TITLE);
			if(FileExists2(sAgentPath) && !hWnd && g_pBMSCommAgentDlg->m_bUserBaseForSSO == FALSE)
			{
				WRITELOG(_T("[RequestUserBaseConfig] BMSLogin 실행!!!"));

				if (!ShellExecute(NULL, _T("open"), sAgentPath, _T(""), NULL, SW_SHOW))
				{
					WRITELOG(_T("[RequestUserBaseConfig] ShellExecute 실패 err(%d)"), GetLastError());
				}
				/*
				if(!RunIExplorer(sAgentPath))// if(!ExecuteProcess(sAgentPath))
				{
					WRITELOG(_T("[RequestUserBaseConfig] ExecuteProcess 실패 err(%d)"), GetLastError());
				}
				*/
			}
		}
		else
		{
			WRITELOG(_T("[RequestUserBaseConfig] 사용자 기반 OFF!!! %d, %d"), g_pBMSCommAgentDlg->m_bUserBase, m_dwUserBaseLogoutTime);

			g_pBMSCommAgentDlg->KillTimer(USERBASE_MONITORING_TIMER_ID);

			DeleteUserBaseFlagFile();
			DeleteUserIDFile();
			RequestPolicies();
			RequestUpdatePolicies();

			HWND hWnd = ::FindWindow(_T("#32770"), BM_LOGIN_WND_TITLE);
			if(hWnd != NULL)
			{
				::PostMessage(hWnd, BM_SHUTDOWN_AGENT, (WPARAM)NULL, (LPARAM)NULL);
			}	
		}
	}

	FindAgentANDSendPostMSG();

	WRITELOG(_T("[RequestUserBaseConfig] 완료 bRet(%d)"), bRet);
	return bRet;
}

DWORD CClientManager::GetLastInputTick()
{
	WRITELOG(_T("[CBMSGreenAgentDlg] GetLastInputTick start..."));
	LASTINPUTINFO stLastInfo;
	stLastInfo.cbSize = sizeof(LASTINPUTINFO);
	if(!GetLastInputInfo(&stLastInfo))
	{
		WRITELOG(_T("[CBMSGreenAgentDlg] GetLastInputTick error value(%d)"), GetLastError());
		return 0;
	}
	WRITELOG(_T("[CBMSGreenAgentDlg] GetLastInputTick end...%d"), stLastInfo.dwTime);
	return stLastInfo.dwTime;
}

int CClientManager::CreateUserBaseFlagFile()
{
	WRITELOG(_T("[CreateUserBaseFlagFile] 시작"));

	int iRet = 0;
	TCHAR pszLogPath[MAX_PATH] = {0, };
	GetAgentLogPath(pszLogPath);
	CAtlString sAgentIDPath = _T("");
	sAgentIDPath.Format(_T("%stmp\\USE_USERBASE*"), pszLogPath);
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA FindFileData;
	hFind = FindFirstFile(sAgentIDPath, &FindFileData);
	if(hFind == INVALID_HANDLE_VALUE)
	{
		TCHAR pszPath[MAX_PATH];
		GetAgentLogPath(pszPath);
		CString sAgentIdFileName = _T("");
		sAgentIdFileName.Format(_T("%stmp\\USE_USERBASE"), pszPath);
		HANDLE hAID = INVALID_HANDLE_VALUE;
		hAID = CreateFile(sAgentIdFileName, FILE_ALL_ACCESS, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hAID != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hAID);
		}
		else
		{
			iRet = -1; // 파일 생성 실패
		}
	}
	else
	{
		iRet = -2; // 파일 존재 함
	}

	if(hFind != INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
	}

	WRITELOG(_T("[CreateUserBaseFlagFile] 완료 iRet(%d)"), iRet);
	return iRet;
}

int CClientManager::DeleteUserBaseFlagFile()
{
	WRITELOG(_T("[DeleteUserBaseFlagFile] 시작"));

	int iRet = 0;
	TCHAR pszLogPath[MAX_PATH] = {0, };
	GetAgentLogPath(pszLogPath);
	CAtlString sAgentIDPath = _T("");
	sAgentIDPath.Format(_T("%stmp\\USE_USERBASE*"), pszLogPath);
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA FindFileData;

	hFind = FindFirstFile(sAgentIDPath, &FindFileData);
	if(hFind != INVALID_HANDLE_VALUE)
	{
		CAtlString tmp = _T("");
		tmp += pszLogPath;
		tmp += _T("tmp\\");
		tmp += FindFileData.cFileName;

		if(!::DeleteFile(tmp))
		{
			iRet = -1; // 삭제 실패
		}
	}
	else
	{
		iRet = -2; // 파일 없음
	}

	if(hFind != INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
	}

	WRITELOG(_T("[DeleteUserBaseFlagFile] 완료 iRet(%d)"), iRet);

	return iRet;
}

BOOL RunIExplorer(CString sPath, CString sCmdLine)
{
	LPTSTR strSearch;

	// 변수 선언
	BOOL bResult = TRUE;
	DWORD dwSessionId = 0; // 세션 ID
	DWORD dwWinlogonPID = 0; // Winlogon PID 
	typedef DWORD (*pfnWTSGetActiveConsoleSessionId)(void);
	HMODULE hModule = NULL;
	pfnWTSGetActiveConsoleSessionId fnWTSGetActiveConsoleSessionId = NULL; 

	hModule = ::LoadLibrary((_T("Kernel32.dll")));
	if( hModule != NULL )
	{
		fnWTSGetActiveConsoleSessionId = (pfnWTSGetActiveConsoleSessionId) GetProcAddress(hModule, "WTSGetActiveConsoleSessionId");
		if( fnWTSGetActiveConsoleSessionId == NULL ) 
		{
			WRITELOG(_T("[CGTrayDlg::RunIExplorer] WTSGetActiveConsoleSessionId 함수 얻기 실패"));
			bResult = FALSE;
			goto CleanUP;
		}
		dwSessionId = fnWTSGetActiveConsoleSessionId();
	}

	// 같은 세션으로 실행할 프로세스 찾기(Winlogon.exe)
	BOOL IsExistsWinlogon = FALSE;
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if( hSnap == INVALID_HANDLE_VALUE )
	{
		WRITELOG(_T("[CGTrayDlg::RunIExplorer] CreateToolhelp32Snapshot 실패:INVALID_HANDLE_VALUE"));
		bResult = FALSE;
		goto CleanUP;
	} 
	if( !Process32First(hSnap, &pe32) )
	{
		WRITELOG(_T("[CGTrayDlg::RunIExplorer] Process32First 실패"));
		bResult = FALSE;
		goto CleanUP;
	} 

	// Vista이지만 User mode로 실행할 것이므로 explorer를 기준으로 한다.
	strSearch = _T("explorer.exe"); 

	do
	{
		if( lstrcmpi(pe32.szExeFile, strSearch) == 0 )
		{
			DWORD dwWLSessionID = 0;
			BOOL bRet = ProcessIdToSessionId(pe32.th32ProcessID, &dwWLSessionID); 
			//			if(  bRet && (dwSessionId == dwWLSessionID) )
			{
				dwWinlogonPID = pe32.th32ProcessID;
				IsExistsWinlogon = TRUE;
				break;
			}
		}
	} while ( Process32Next(hSnap, &pe32) ); 

	if( IsExistsWinlogon == FALSE )
	{
		WRITELOG(_T("[CGTrayDlg::RunIExplorer] Winlogon.exe 검색 실패"));
		bResult = FALSE;
		goto CleanUP;
	} 

	// WINLOGON 프로세스 열고 토큰 복사하여 특정 세션 지정
	HANDLE hProcess = ::OpenProcess(MAXIMUM_ALLOWED, FALSE, dwWinlogonPID);
	HANDLE hToken = NULL;
	HANDLE hTokenDup = NULL;
	if( hProcess == NULL )
		goto CleanUP;
	if( !::OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_ASSIGN_PRIMARY|TOKEN_ADJUST_SESSIONID|TOKEN_READ|TOKEN_WRITE, &hToken))
	{
		WRITELOG(_T("[CGTrayDlg::RunIExplorer] OpenProcessToken Failed"));
		bResult = FALSE;
		goto CleanUP;
	}
	// CreateProcessAsUser 준비
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.lpDesktop = (_T("winsta0\\default"));
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	DWORD dwCreationFlags = NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE;
	LPVOID pEnv = NULL;

	TCHAR szPath[MAX_PATH];

	_sntprintf(szPath, _countof(szPath)-sizeof(TCHAR), _T("%s %s"), sPath, sCmdLine);

	bResult = ::CreateProcessAsUser(
		hToken,
		NULL,
		szPath,
		NULL,
		NULL,
		FALSE,
		dwCreationFlags,
		pEnv,
		NULL,
		&si,
		&pi); 
	if( !bResult )
		WRITELOG(_T("[CGTrayDlg::RunIExplorer] CreateProcessAsUser Failed %d"), GetLastError());

	if( (bResult) && (pi.hProcess != INVALID_HANDLE_VALUE) )
		CloseHandle(pi.hProcess);
	if( (bResult) && (pi.hThread != INVALID_HANDLE_VALUE) )
		CloseHandle(pi.hThread); 
CleanUP:
	if( hModule )
		FreeLibrary(hModule);
	if( hSnap != INVALID_HANDLE_VALUE )
		CloseHandle(hSnap);
	if( hProcess != NULL )
		CloseHandle(hProcess);
	if( hToken )
		CloseHandle(hToken);
	if( hTokenDup )
		CloseHandle(hTokenDup); 

	return bResult;
}

BOOL CClientManager::DeleteDuplicatePolicy(CString &sReceived)
{
	CXMLFile xmlfile;
	if(!xmlfile.LoadFromString(sReceived))
	{
		WRITELOG(_T("[DeleteDuplicatePolicy] xmlfile.LoadFromString 실패"));
		return FALSE;
	}

	HRESULT hr;

	// get root element
	CComPtr<IXMLDOMElement> pRootElem;
	hr = xmlfile.GetRootNode(&pRootElem);
	if(FAILED(hr))
	{
		WRITELOG(_T("[DeleteDuplicatePolicy] xmlfile.GetRootNode 실패"));
		return FALSE;
	}

	// read regulation list
	CComPtr<IXMLDOMNodeList> pRegulationList;
	hr = xmlfile.GetPathedNodeList(pRootElem, _T("RegulationList/Regulation"), &pRegulationList);
	if(FAILED(hr))
	{
		WRITELOG(_T("[DeleteDuplicatePolicy] xmlfile.GetPathedNodeList 실패"));
		return FALSE;
	}

	// 정책을 읽어 등록한다.
	long n_regulations;
	hr = pRegulationList->get_length(&n_regulations);
	if(FAILED(hr))
	{
		WRITELOG(_T("[DeleteDuplicatePolicy] xmlfile.get_length 실패"));
		return FALSE;
	}

	CMap<CString, LPCTSTR, BOOL, BOOL&> cDuplicateCheckMap;

	for(int l=0; l<n_regulations; l++) 
	{
		CString str;
		CComPtr<IXMLDOMNode> pRegulNode;
		hr = pRegulationList->get_item(l, &pRegulNode);
		if(FAILED(hr))
		{
			WRITELOG(_T("[DeleteDuplicatePolicy] xmlfile.get_item 실패"));
			return FALSE;
		}

		CComQIPtr<IXMLDOMElement> pRegulElem;
		pRegulElem = pRegulNode;

		CString sReaction;
		xmlfile.GetAttributeValue(pRegulElem, _T("reaction"), sReaction);

		CString sAction;
		xmlfile.GetAttributeValue(pRegulElem, _T("action"), sAction);

		CString sKey = sAction + g_Separator + sReaction;
		BOOL bDuplicate = FALSE;

		cDuplicateCheckMap.Lookup(sKey, bDuplicate);

		if (bDuplicate == FALSE)
		{
			bDuplicate = TRUE;
			cDuplicateCheckMap.SetAt(sKey, bDuplicate);
		}
		else
		{
			WRITELOG(_T("[DeleteDuplicatePolicy] 중복 정책 발견 Action : %s, Reaction : %s"), sAction, sReaction);
			CComPtr<IXMLDOMNode> pParentNode;
			if(xmlfile.GetParentNode(pRegulElem, &pParentNode))
			{
				CComPtr<IXMLDOMNode> pOldNode;
				hr = pParentNode->removeChild(pRegulElem, &pOldNode);
				if(FAILED(hr))
				{
					WRITELOG(_T("[DeleteDuplicatePolicy] xmlfile.removeChild 실패"));
					return FALSE;
				}
				else
				{
					WRITELOG(_T("[DeleteDuplicatePolicy] 중복 정책 제거!!"));
				}
			}
		}
 	}
	CString sInstantLevelXML = _T("");
	xmlfile.GetXML(sInstantLevelXML);
	
	sInstantLevelXML.Replace(_T("<?xml version=\"1.0\"?>"), _T("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
	sReceived = sInstantLevelXML;

	return TRUE;
}

int CClientManager::RequestCancleInstantPolicies()	// 2016.05.13 gsallow6.1에서 승인된 임시정책이 없어 정책 요청을 하지 않는 현상이 발생, 18001 통신을 하여 정책을 설정한다.
{
	WRITELOG(_T("[RequestCancleInstantPolicies] RequestCancleInstantPolicies Start"));

	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::RequestCancleInstantPolicies] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::RequestCancleInstantPolicies] - m_sAgentID = %s"), m_sAgentID);
	}

	CString sReq = _T(""), sReceived = _T(""), sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::RequestCancleInstantPolicies] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18001"));

	CStringW wsCmd = sCmd, wsReq = _T("");
	int ret = 0;
	int index = 0;

	sReq.Format(_T("%s%c%s"), _T("0"), g_Separator, m_sAgentID);

	CString sUserId = ReadUserInfoFile(USERINFO_LOGINID);
	if (sUserId.IsEmpty() == FALSE)
	{
		sReq += (CString)((TCHAR)FIELD_SEPARATOR + (CString)sUserId);
	}

	wsReq = sReq;
	ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sReceived, m_bIsHttps);
	if(ret)
	{
		WRITELOG(_T("[CClientManager::RequestCancleInstantPolicies] SendAndReceive FALSE ret : %d"), ret);
	}
	else
	{
		m_bPolicyReceived = TRUE;

		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sReceived, _T(""));
		m_sDLPPolicyTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));

		WRITELOG(_T("[CClientManager::RequestCancleInstantPolicies] 정책 파일 생성"));

		// Agent들에게 알림
		RequestUpdatePolicies();
		// 2013.08.13, kih5893 임시정책 요청 후에도 copysave 값 가져올 수 있도록 수정 
		ApplyPolicies();

	}

	return ret;	//0 이면 정상통신, 0이 아니면 ErrorCode
}

int CClientManager::CreateInstantLevel()
{
	WRITELOG(_T("[CClientManager] CreateInstantPolicy Start"));

	m_sDLPPolicyTime = _T("");

	// 2010/10/12, avilon - agentID를 얻어 오지 못 한 경우 agentinfo에서 다시 읽어 온다.
	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::CreateInstantLevel()] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::CreateInstantLevel()] - m_sAgentID = %s"), m_sAgentID);
	}

	CString sReq = _T(""), sReceived = _T(""), sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::CreateInstantLevel()] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18001"));

	CStringW wsCmd = sCmd, wsReq = _T("");
	int ret = 0;
	int index = 0;

	sReq.Format(_T("%s%c%s"), _T("0"), g_Separator, m_sAgentID);

	CString sUserId = ReadUserInfoFile(USERINFO_LOGINID);
	if (sUserId.IsEmpty() == FALSE)
	{
		sReq += (CString)((TCHAR)FIELD_SEPARATOR + (CString)sUserId);
	}

	wsReq = sReq;
	ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sReceived, m_bIsHttps);
	if(ret)
	{
		WRITELOG(_T("[CClientManager] CreateInstantPolicy::SendAndReceive FALSE ret : %d"), ret);
	}

	WRITELOG(_T("[CClientManager] CreateInstantPolicy::sReceived.GetLength %d"), sReceived.GetLength());
	if((ret==0) && (sReceived.GetLength() > 0))
	{
		//여기서 파일을 쓴다
		//
		CString sFilePath = _T(""), OriginPolicy = _T("");
		TCHAR lpLogPath[MAX_PATH] = {0, };
		GetAgentLogPath(lpLogPath, MAX_PATH);
		OriginPolicy.Format(_T("%s%s\\OriginPolicy"), lpLogPath, BM_RDATA);
		if (MultiLanguageWriteFile(OriginPolicy, sReceived, CHARSET_UTF8) == FALSE)
		{
			WRITELOG(_T("[CClientManager::RequestPolicies] MultiLanguageWriteFile Fail : %d"), GetLastError());
		}
		//
		EnterCriticalSection(&m_AllowDataMapCS);
		int nInstantLevelCount = m_cAllowDataMap.GetCount();
		POSITION stPos = m_cAllowDataMap.GetStartPosition();
		LeaveCriticalSection(&m_AllowDataMapCS);

		CMap<CString, LPCTSTR, BOOL, BOOL&> cInstantPolicyActionMap;

		CString sInstantPolicyAction = _T("");
		while(stPos)
		{
			CString sKey = _T("");	
			ALLOW_DATA stAllowData;
			EnterCriticalSection(&m_AllowDataMapCS);
			m_cAllowDataMap.GetNextAssoc(stPos, sKey, stAllowData);
			LeaveCriticalSection(&m_AllowDataMapCS);

			sInstantPolicyAction += sKey + _T(",");

			#define INSTANT_POLICY_ACTION_INDEX 1
			#define INSTANT_POLICY_REACTION_INDEX 2

			CString sAction = _T(""), sMapKey = _T(""), sReaction = _T("");
			AfxExtractSubString(sAction, stAllowData.m_sAction, INSTANT_POLICY_ACTION_INDEX, g_Separator);
			AfxExtractSubString(sReaction, stAllowData.m_sAction, INSTANT_POLICY_REACTION_INDEX, g_Separator);

			BOOL bReserved = TRUE;
			if (sReaction == _T("WATERMARK"))
			{
				sMapKey = sAction + g_Separator + _T("WATERMARK_EX");
				cInstantPolicyActionMap.SetAt(sMapKey, bReserved);
			}
			else if (sReaction == _T("WATERMARK_EX"))
			{
				sReaction = _T("WATERMARK");
			}

			sMapKey = sAction + g_Separator + sReaction;
			cInstantPolicyActionMap.SetAt(sMapKey, bReserved);
		}

		
		WRITELOG(_T("[CreateInstantPolicy] sInstantPolicyAction(%s)"), sInstantPolicyAction);
		DeleteDuplicatePolicy(sReceived);
		if(sInstantPolicyAction.IsEmpty() == FALSE)
		{
			CXMLFile xmlfile;
			if(!xmlfile.LoadFromString(sReceived))
			{
				WRITELOG(_T("[CreateInstantPolicy] xmlfile.LoadFromString 실패"));
				return -1;
			}

			HRESULT hr;

			// get root element
			CComPtr<IXMLDOMElement> pRootElem;
			hr = xmlfile.GetRootNode(&pRootElem);
			if(FAILED(hr))
			{
				WRITELOG(_T("[CreateInstantPolicy] xmlfile.GetRootNode 실패"));
				return -2;
			}

			// read regulation list
			CComPtr<IXMLDOMNodeList> pRegulationList;
			hr = xmlfile.GetPathedNodeList(pRootElem, _T("RegulationList/Regulation"), &pRegulationList);
			if(FAILED(hr))
			{
				WRITELOG(_T("[CreateInstantPolicy] xmlfile.GetPathedNodeList 실패"));
				return -3;
			}

			// 정책을 읽어 등록한다.
			long n_regulations;
			hr = pRegulationList->get_length(&n_regulations);
			if(FAILED(hr))
			{
				WRITELOG(_T("[CreateInstantPolicy] xmlfile.get_length 실패"));
				return -4;
			}

			for(int l=0; l<n_regulations; l++) 
			{
				CString str;
				CComPtr<IXMLDOMNode> pRegulNode;
				hr = pRegulationList->get_item(l, &pRegulNode);
				if(FAILED(hr))
				{
					WRITELOG(_T("[CreateInstantPolicy] xmlfile.get_item 실패"));
					return -5;
				}

				CComQIPtr<IXMLDOMElement> pRegulElem;
				pRegulElem = pRegulNode;

				CString sReaction;
				xmlfile.GetAttributeValue(pRegulElem, _T("reaction"), sReaction);
				if(!sReaction.CompareNoCase(_T("BLOCK")) || !sReaction.CompareNoCase(_T("WATERMARK")) || !sReaction.CompareNoCase(_T("WATERMARK_EX")))
				{
					CString sAction;
					xmlfile.GetAttributeValue(pRegulElem, _T("action"), sAction);

					CString sKey = sAction + g_Separator + sReaction;
					BOOL bReserved = FALSE;

					if(cInstantPolicyActionMap.Lookup(sKey, bReserved))// if(sInstantLevelAction.Find(sAction + g_Separator + sReaction) > -1)
					{
						WRITELOG(_T("[CreateInstantPolicy] xml 편집(%s)"), sAction + g_Separator + sReaction);
						
						CComPtr<IXMLDOMNode> pParentNode;
						if(xmlfile.GetParentNode(pRegulElem, &pParentNode))
						{
							CComPtr<IXMLDOMNode> pOldNode;
							hr = pParentNode->removeChild(pRegulElem, &pOldNode);
							if(FAILED(hr))
							{
								WRITELOG(_T("[CreateInstantPolicy] xmlfile.removeChild 실패"));
								return -6;
							}
							else
							{
								// 2017.09.19 hangebs 이 분기문 안에서 USBPORT와 같이 정책 내용이 글로벌과 관계 있을 경우 수정이 필요하다
								if (sAction.CompareNoCase(_T("USBPORT")) == 0)
								{
									xmlfile.SetAttributeValue(pRootElem, _T("usbportdefault"), _T("0"));
									xmlfile.SetAttributeValue(pRootElem, _T("usbportlogsave"), _T("1"));
									xmlfile.SetAttributeValue(pRootElem, _T("usbportlogblock"), _T("1"));
									WRITELOG(_T("[CreateInstantPolicy] USBPORT Global Set"));
								}
							}
						}
						else
						{
							WRITELOG(_T("[CreateInstantPolicy] xmlfile.GetParentNode 실패"));
						}
					}
				}
			}
			CString sInstantLevelXML = _T("");
			xmlfile.GetXML(sInstantLevelXML);
			sInstantLevelXML.Replace(_T("<?xml version=\"1.0\"?>"), _T("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
			sReceived = sInstantLevelXML;
		}
		else
		{
			WRITELOG(_T("[CreateInstantPolicy] 임시 정책 미적용 상태"));
		}

		m_policy_update_time = _T("");

		if(m_OSVerMajor >= 6)
		{
			TCHAR tempBuf[MAX_PATH];
			GetCSIDLDirectory(tempBuf, CSIDL_PROFILE);
			CString sUserDir = tempBuf;
			DWORD dwPos = sUserDir.ReverseFind(_T('\\'));
			sUserDir = sUserDir.Left(dwPos + 1);
			sUserDir += m_sSupportName;
			_sntprintf(tempBuf, MAX_PATH-1, _T("%s\\AppData\\Local\\VirtualStore\\Windows\\System32\\"), sUserDir);
			WRITELOG(_T("[CClientManager] Remove dir dir path : %s"), tempBuf);
			TCHAR VirbmsdatPath[MAX_PATH], VirbmsbinPath[MAX_PATH];
			_sntprintf(VirbmsbinPath, MAX_PATH-1, _T("%sbmsbin\\"),tempBuf);
			_sntprintf(VirbmsdatPath, MAX_PATH-1, _T("%sbmsdat\\"),tempBuf);
			RemoveAllDir(VirbmsbinPath);
			RemoveAllDir(VirbmsdatPath);
		}
		FILE*		fp	= NULL;
		TCHAR*		data_ptr	= NULL;

		if(sReceived.GetLength() == 0)
		{
			WRITELOG(_T("[CClientManager] CreateInstantPolicy - 정책을 받지 못함"));
			return -2;
		}
		
		m_bPolicyReceived = TRUE;

		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sReceived, _T(""));
		m_sDLPPolicyTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));

		WRITELOG(_T("[CClientManager] CreateInstantPolicy : 정책 파일 생성"));

		// Agent들에게 알림
		RequestUpdatePolicies();
		// 2013.08.13, kih5893 임시정책 요청 후에도 copysave 값 가져올 수 있도록 수정 
		ApplyPolicies();

	}
	// 2011/06/27 by realhan, 레벨 변경으로 인한 임시정책 중지.
	else if(ret == BM_ERROR_NO_ALLOWREQUEST)
	{
		WRITELOG(_T("[CClientManager] CreateInstantPolicy 임시정책 ERR-2001 #해당 agent의 임시정책이 정책 변경으로 인하여 해제됨"));

		RequestAllowList(m_sAgentID, TRUE);
		// WriteAllowData("", TRUE);
		EnterCriticalSection(&m_AllowDataMapCS);
		m_cAllowDataMap.RemoveAll();
		LeaveCriticalSection(&m_AllowDataMapCS);
		m_bIsCurrentUsingInstantLevel = FALSE;
		g_pBMSCommAgentDlg->KillTimer(ALLOW_INSTANTLEVEL_TIMER_ID);
		SendReactTrayRefresh();
		RequestPolicies(FALSE, TRUE);
	}
	else
	{
		WRITELOG(_T("[CClientManager] CreateInstantPolicy 정책 수신 실패. 오프라인 정책 설정."));
		// 정책 백업..
		BackupPolicyFile();

		// 오프라인 정책 생성
		CreateOfflinePolicy(m_OfflinePolicy);

		// 에이전트들에게 정책을 새로 적용하라고 요청한다.
		RequestUpdatePolicies(TRUE);

		// 정책(사본 저장, 오프라인 정책)을 읽어 적용.
		ApplyPolicies();
	}

	WRITELOG(_T("[CClientManager] CreateInstantPolicy End"));
	return 0;
}

int CClientManager::UpdateInstantLevelEndDate()
{
	WRITELOG(_T("[CClientManager] UpdateInstantEndDate Start"));
	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::UpdateInstantLevelEndDate()] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::UpdateInstantLevelEndDate()] - m_sAgentID = %s"), m_sAgentID);
	}

	EnterCriticalSection(&m_AllowDataMapCS);
	if(!m_cAllowDataMap.GetCount())
	{
		WRITELOG(_T("[CClientManager] UpdateInstantEndDate !m_cAllowDataList.GetCount()"));
		LeaveCriticalSection(&m_AllowDataMapCS);
		return -1;
	}
	LeaveCriticalSection(&m_AllowDataMapCS);

	CString sReq = _T(""), sReceived = _T(""), sCmd = _T(""),sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSALLOW, sServerURL);
	WRITELOG(_T("[CClientManager::UpdateInstantEndDate()] - m_stPageInfo.m_GSALLOW - %s"), m_stPageInfo.m_GSALLOW);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSALLOW : sPageName, _T("18212"));

	EnterCriticalSection(&m_AllowDataMapCS);
	POSITION stPos = m_cAllowDataMap.GetStartPosition();
	LeaveCriticalSection(&m_AllowDataMapCS);
	while(true)
	{
		CString sPacketData = _T("");
		TCHAR cSeparator = _T(';');
		if(!stPos)
		{
			WRITELOG(_T("[CClientManager] UpdateInstantEndDate::stPos 0x%x"), stPos);
			break;
		}
		CString sKey = _T("");
		ALLOW_DATA stAllowData;
		EnterCriticalSection(&m_AllowDataMapCS);
		m_cAllowDataMap.GetNextAssoc(stPos, sKey, stAllowData);
		LeaveCriticalSection(&m_AllowDataMapCS);
		sPacketData.Format(_T("%d%c%s%c%s%c%s"), stAllowData.m_dwTableid, cSeparator, m_sAgentID, cSeparator, stAllowData.m_sEndDate, cSeparator, stAllowData.sReqType);
		sReq += (sReq.IsEmpty()) ? sPacketData : g_Separator + sPacketData;
	}

	CStringW wsCmd = sCmd, wsReq = sReq;
	__int64 nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sReceived, m_bIsHttps);
	if (nRet == BM_ERROR_NO_ALLOWREQUEST)
	{
		WRITELOG(_T("[CClientManager] UpdateInstantEndDate 임시정책 ERR-2001 #해당 agent의 임시정책이 정책 변경으로 인하여 해제됨"));

		RequestAllowList(m_sAgentID, TRUE);
		// WriteAllowData("", TRUE);
		EnterCriticalSection(&m_AllowDataMapCS);
		m_cAllowDataMap.RemoveAll();
		LeaveCriticalSection(&m_AllowDataMapCS);
		m_bIsCurrentUsingInstantLevel = FALSE;
		g_pBMSCommAgentDlg->KillTimer(ALLOW_INSTANTLEVEL_TIMER_ID);
		SendReactTrayRefresh();
	}
	else if(nRet)
	{
		WRITELOG(_T("[CClientManager] UpdateInstantEndDate::SendAndReceive error(%d)"), nRet);
	}
	
	WRITELOG(_T("[CClientManager] UpdateInstantEndDate End"));
	return 0;
}

int CClientManager::UpdateInstantLevelEndState(DWORD dwTableid, CString sReqType)
{
	WRITELOG(_T("[CClientManager::UpdateInstantLevelEndState] Start"));

	// 2010/10/12, avilon - agentID를 얻어 오지 못 한 경우 agentinfo에서 다시 읽어 온다.
	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::UpdateInstantLevelEndState()] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::UpdateInstantLevelEndState()] - m_sAgentID = %s"), m_sAgentID);
	}

	int result = 0;
	CString sReq = _T(""), sReceived = _T(""), sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSALLOW, sServerURL);
	WRITELOG(_T("[CClientManager::UpdateInstantLevelEndState()] - m_stPageInfo.m_GSALLOW - %s"), m_stPageInfo.m_GSALLOW);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSALLOW : sPageName, _T("18213"));
	sReq.Format(_T("%d%c%s"), dwTableid, g_Separator, m_sAgentID);
	// 2012/06/15, avilon
	if(sReqType.CompareNoCase(_T("FILE")) == 0 || sReqType.CompareNoCase(_T("DEV")) == 0
		|| sReqType.CompareNoCase(_T("APP")) == 0 || sReqType.CompareNoCase(_T("WEB")) == 0)	//_T(""), _T("WATERMARK")는 적용되면 안됨
	{
		sReq += g_Separator;
		sReq += sReqType;
	}
	
	__int64 nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, m_bIsHttps);
	if (nRet != 0)
	{
		WRITELOG(_T("[UpdateInstantLevelEnd] SendAndReceive error(%d), GetLastError(%d)"), nRet, GetLastError());
	}

	WRITELOG(_T("[CClientManager] UpdateInstantLevelEnd End"));
	return nRet;
}

int CClientManager::ExpireReqAllowOfUserBase()
{
	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::ExpireReqAllowOfUserBase] Begin Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		WRITELOG(_T("[CClientManager::ExpireReqAllowOfUserBase] - m_sAgentID = %s"), m_sAgentID);
	}

	CString sReq = _T(""), sReceived = _T(""), sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSALLOW, sServerURL);
	CAtlString sUserBaseLoginID = ReadUserInfoFile(USERINFO_USERID);
	WRITELOG(_T("[CClientManager::ExpireReqAllowOfUserBase] - m_stPageInfo.m_GSALLOW - %s"), m_stPageInfo.m_GSALLOW);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSALLOW : sPageName, _T("18215"));
	sReq.Format(_T("%s%c%s"), m_sAgentID, g_Separator, sUserBaseLoginID);

	__int64 nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, m_bIsHttps);
	if (nRet != 0)
	{
		WRITELOG(_T("[CClientManager::ExpireReqAllowOfUserBase] SendAndReceive error(%d)"), nRet);
	}
	WRITELOG(_T("[CClientManager::ExpireReqAllowOfUserBase] End"));

	return 0;
}

int CClientManager::CreateFileAndWrite( CString sFullPathName, CString sWriteData )
{
	BOOL bRet = FALSE;
	
	HANDLE hFile = CreateFile(sFullPathName, GENERIC_WRITE, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, NULL, NULL);

	if(hFile != INVALID_HANDLE_VALUE)
	{
		DWORD dwBytesWrite = 0;
		if( WriteFile(hFile, sWriteData, sWriteData.GetLength() * sizeof(TCHAR), &dwBytesWrite, NULL) == 0 )
		{
			bRet = TRUE;	
			CloseHandle(hFile);
		}
		int a = dwBytesWrite;
	}
	return bRet;
}

int CClientManager::CreateFileAndWrite( CString sFullPathName, CStringA sWriteData )
{
	BOOL bRet = FALSE;

	HANDLE hFile = CreateFile(sFullPathName, GENERIC_WRITE, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, NULL, NULL);

	if(hFile != INVALID_HANDLE_VALUE)
	{
		DWORD dwBytesWrite = 0;
		if( WriteFile(hFile, sWriteData, sWriteData.GetLength(), &dwBytesWrite, NULL) != FALSE )
		{
			bRet = TRUE;	
		}
		else
		{
			WRITELOG(_T("[CreateFileAndWrite] failed (%d)"), GetLastError());
		}
		CloseHandle(hFile);
	}
	return bRet;
}

CStringA CClientManager::UnicodeToMultibyte( CString sOriginData )
{
	CHAR *pchUTF8 = NULL;
	if(m_bUseMultiLan)
	{
		int nLen = WideCharToMultiByte(CP_UTF8, 0, sOriginData, sOriginData.GetLength(), NULL, 0, NULL, NULL);
		pchUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_UTF8, 0, sOriginData, sOriginData.GetLength(), pchUTF8, nLen, NULL, NULL);
		pchUTF8[nLen] = NULL;
	}
	else
	{
		int nLen = WideCharToMultiByte(CP_ACP, 0, sOriginData, sOriginData.GetLength(), NULL, 0, NULL, NULL);
		pchUTF8 = new CHAR[nLen + 1];
		WideCharToMultiByte(CP_ACP, 0, sOriginData, sOriginData.GetLength(), pchUTF8, nLen, NULL, NULL);
		pchUTF8[nLen] = NULL;
	}

	CStringA sMultiBytes = _T("");
	sMultiBytes.Format("%s", pchUTF8);

	if (pchUTF8)
	{
		delete []pchUTF8; pchUTF8 = NULL;
	}
	

	return sMultiBytes;
}

CString CClientManager::GetDiffHardwareSpec( CString sNewSpec, CString sOldSpec )
{
	int nRet = 0;
	CMap<CString, LPCTSTR, CString, LPCTSTR> *pOldDatamap = new CMap<CString, LPCTSTR, CString, LPCTSTR>;
	CMap<CString, LPCTSTR, CString, LPCTSTR> *pNewDatamap = new CMap<CString, LPCTSTR, CString, LPCTSTR>;
	CMap<CString, LPCTSTR, CString, LPCTSTR> *pDiffDatamap = new CMap<CString, LPCTSTR, CString, LPCTSTR>;

	nRet = GetKeyAndValueToMap(sNewSpec, pOldDatamap);
	nRet = GetKeyAndValueToMap(sOldSpec, pNewDatamap);

	int nDiffCnt = ExtractDifference(pOldDatamap, pNewDatamap, pDiffDatamap);
	CString sDiffKey = _T(""), sDiffValue = _T(""), sDiffData = _T("");

	POSITION pos = pDiffDatamap->GetStartPosition();
	for(int i = 0; i < nDiffCnt; i ++)
	{
		pDiffDatamap->GetNextAssoc(pos, sDiffKey, sDiffValue);
		sDiffData += MakeNameValueFormat(sDiffKey, sDiffValue);
	}

	pOldDatamap->RemoveAll();
 	delete pOldDatamap; pOldDatamap = NULL;
	pNewDatamap->RemoveAll();
	delete pNewDatamap; pNewDatamap = NULL;
	pDiffDatamap->RemoveAll();
	delete pDiffDatamap; pDiffDatamap = NULL;

	return sDiffData;
}

CAtlString CClientManager::GetAgentInfoValue(__in LPCTSTR sName)
{
	return g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_AGENTINFO_FILENAME, sName, _T(""));
}

BOOL CClientManager::CheckPolicyIntegrity( CStringA &sAReceivedPolicy, CString sHashedServerPolicy )
{
	BOOL bCompareHash = FALSE;
	CAtlString sHashedAgentPolicy = _T("");
	DWORD dwHashResult = 0;

	dwHashResult = GetHashValue(sAReceivedPolicy, sHashedAgentPolicy);

	if(dwHashResult)
	{
		WRITELOG(_T("[CheckPolicyIntegrity] GetHashValue Failed (%d)"), dwHashResult);
	}

	if(sHashedAgentPolicy != _T(""))
	{
		bCompareHash = CompareHashData(sHashedServerPolicy, sHashedAgentPolicy);
	}

	return bCompareHash;
}

__int64 CClientManager::SendPolicyIntegrityResultToServer(BOOL bSuccess)
{
	CString sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::SendPolicyIntegrityResultToServer()] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18008"));

	CString sPacket = _T("");
	CString sTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));
	sPacket.Format(_T("%s%c%s%c%s"), m_sAgentID, FIELD_SEPARATOR, sTime, FIELD_SEPARATOR, bSuccess?_T("SUCCESS"):_T("FAIL"));

	WRITELOG(_T("[SendPolicyIntegrityResultToServer] sPacket - %s"), sPacket);

	//CStringA sUTF8 = ConvertWideToUTF8(sPacket);
	__int64 nRet = 0;
	CString sResp = _T("");
	nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sPacket, sResp, m_bIsHttps);

	WRITELOG(_T("[SendFormatResultToServer] nRet(%d), sResp(%s)"), nRet, sResp);

	return nRet;
}

void CClientManager::SavePolicyIntegrityFile( CStringA sAReceived )
{
	TCHAR cAgentPath[MAX_PATH] = {0, };
	CString sPolicyPath = _T("");
	GetAgentPath(cAgentPath);
	sPolicyPath.Format(_T("%s%s"), cAgentPath, POLICY_INTEGRITY_FILE_NAME);
	CreateFileAndWrite(sPolicyPath, sAReceived);
}

CAtlString CClientManager::GetHashFromPolicyIntegrityFile()
{
	TCHAR cAgentPath[MAX_PATH] = {0, };
	CString sPolicyPath = _T("");
	GetAgentPath(cAgentPath);
	sPolicyPath.Format(_T("%s%s"), cAgentPath, POLICY_INTEGRITY_FILE_NAME);

	CAtlString sSavedPolicyHash = _T("");
	GetHashFromFile(sPolicyPath, sSavedPolicyHash);
	return sSavedPolicyHash;
}

void CClientManager::SetMonitoringFileName()
{
	// 모니터링 파일 지정
	TCHAR lpLogPath[MAX_PATH] = {0, };
	GetAgentLogPath(lpLogPath);
	g_Log.SetLogFolder(lpLogPath);
	m_sMonitoringFolder = g_Log.m_sLogFolder;
	m_sPrevMonitoringFolder = m_sMonitoringFolder + PREV_DATA_PATH;

	m_sMonitoringFileName_FileLog = CString(FILE_LOG_FILENAME);
	m_sMonitoringFileName_AppLog = CString(APP_LOG_FILENAME);
	m_sMonitoringFileName_PrintLog = CString(PRINT_LOG_FILENAME);
	m_sMonitoringFileName_PrintJpgLog = CString(PRINT_JPG_LOG_FILENAME);
	m_sMonitoringFileName_PrintTxtLog = CString(PRINT_TXT_LOG_FILENAME);
	m_sMonitoringFileName_ScreenRecJpg = CString(SCREEN_JPG_FILENAME);
	m_sMonitoringFileName_AFileLog = CString(AFILE_LOG_FILENAME);
	m_sMonitoringFileName_NetworkLog = CString(NETWORK_LOG_FILENAME);
	m_sMonitoringFileName_MMLog = CString(MM_LOG_FILENAME);
	m_sMonitoringFileName_SaveCopy = CString(SAVECOPY_LOG_FILENAME);
	m_sMonitoringFileName_EXLog = CString(EX_LOG_FILENAME);
	m_sMonitoringFileName_GreenLog = CString(GREEN_LOG_FILENAME);
	m_sMonitoringFileName_DesLog = CString(DESCRIPTOR_LOG_FILENAME);
	m_sMonitoringFileName_ReqLog = CString(REQUEST_LOG_FILENAME);
	m_sMonitoringFileName_ReqFileLog = CString(REQUEST_FILELOG_FILENAME);
	m_sMonitoringFileName_AbnormalLog = CString(ABNORMALLOG_NAME);
	m_sMonitoringFileName_FileReadLog = CString(FILEREADLOG_NAME);
	m_sMonitoringFileName_ContentsLog = CString(CONTENTSLOG_NAME);
	m_sMonitoringFileName_StatusLog = CString(STATUSLOG_NAME);
	m_sMonitoringFileName_AgentHashLog = CString(AGENTHASHLOG_NAME);
	m_sMonitoringFileName_DevLog = CString(DEV_LOG_FILENAME);
	m_sMonitoringFileName_ScreenShotLog = CString(SCREEN_SHOT_NAME);

	m_sMonitoringFileName_DiscoveryLog = CString(BMDC_FILELOG_NAME);
	m_sMonitoringFileName_DsEndInstantLog = CString(BMDC_TEMPPOLICYLOG_NAME);
	m_sMonitoringFileName_DsStateLog = CString(BMDC_STATELOG_NAME);
	m_sMonitoringFileName_DsScanStateLog = CString(BMDC_SCANSTATELOG_NAME);
	m_sMonitoringFileName_DsOfflineReqLog = CString(BMDC_OFFLINE_TEMPPOLICYLOG_NAME);
	m_sMonitoringFileName_DsSeqLog = CString(BMDC_SEQLOG_NAME);

	// 2013.06.14, kih5893
	m_sMonitoringFileName_SendReceiveFile = CString(MULTIFILE_NAME);
	m_sMonitoringFileName_ACheckLog = CString(ACHECKLOG_NAME);
	m_sMonitoringFileName_CancelReqLog = CString(CANCEL_REQUEST_NAME);

	// 2015.03.17, keede122 자산관리 정책 로그
	m_sMonitoringFileName_AssetLog = CString(ASSET_LOG_FILENAME);
	m_sMonitoringFileName_ReqDevLog = CString(REQUEST_DEVICE_LOG_FILENAME);
	// PC보안감사 by yoon.2019.01.08.
	m_sMonitoringFileName_PCSecuLog = CString(PCSECU_LOG_FILENAME);
	m_sMonitoringFileName_JsonFileLog = CString(JSON_FILE_LOG_FILENAME);
	m_sMonitoringFileName_HardwareDeviceLog = CString(HARDWARE_FILE_LOG_FILENAME);
	m_sMonitoringFileName_EXDSLog = CString(EXDS_REQ_LOG_FILENAME);
	m_sMonitoringFileName_EXDSSCLog = CString(EXDS_SAVECOPY_LOG_FILENAME);
	m_sMonitoringFileName_EXDSApproveLog = CString(EXDS_APPROVE_LOG_FILENAME);
	m_sMonitoringFileName_EXDSCompleteLog = CString(EXDS_COMPLETE_LOG_FILENAME);
	m_sMonitoringFileName_ReciveMailLog = CString(SMTP_RECIEVE_NAME);
	m_sMonitoringFileName_ReciveMailSaveCopy = CString(SMTP_RECIEVE_SAVECOPY);

	// BlOCK LOG
	m_sMonitoringFileName_FileBlockLog = CString(BLOCK_LOG_NEW(FILE_LOG_FILENAME));
	m_sMonitoringFileName_AppBlockLog = CString(BLOCK_LOG_NEW(APP_LOG_FILENAME));
	m_sMonitoringFileName_PrintBlockLog = CString(BLOCK_LOG_NEW(PRINT_LOG_FILENAME));
	m_sMonitoringFileName_AFileBlockLog = CString(BLOCK_LOG_NEW(AFILE_LOG_FILENAME));
	m_sMonitoringFileName_MMBlockLog = CString(BLOCK_LOG_NEW(MM_LOG_FILENAME));
	m_sMonitoringFileName_FileAccessBlockLog = CString(BLOCK_LOG_NEW(FILEREADLOG_NAME));
	m_sMonitoringFileName_PasswordMgrLog = CString(PASSWORDMGR_LOG_FILENAME);
	m_sMonitoringFileName_WinUpdateLog = CString(WINUPDATE_LOG_FILENAME);
	m_sMonitoringFileName_NdisLog = CString(NDIS_LOG_FILENAME);
	m_sMonitoringFileName_DisFileReqLog = CString(DESFILE_REQLOG_NAME);
	m_sMonitoringFileName_AppFileBlockLog = CString(BLOCK_LOG_NEW(APP_FILE_LOG_FILENAME));
	m_sMonitoringFileName_ScreenShotBlockLog = CString(BLOCK_LOG_NEW(SCREEN_SHOT_NAME));
	m_sMonitoringFileName_ScreenShotSaveCopyLog = CString(SCREEN_SHOT_SAVECOPY_NAME);
	m_sMonitoringFileName_ScreenShotBlockSaveCopyLog = CString(BLOCK_LOG_NEW(SCREEN_SHOT_SAVECOPY_NAME));
	m_sMonitoringFileName_ExtChangeBlockLog = CString(BLOCK_LOG_NEW(CHANGE_EXT_LOG_NAME));
	m_sMonitoringFileName_JsonFileBlockLog = CString(BLOCK_LOG_NEW(JSON_FILE_LOG_FILENAME));

	m_sMonitoringFileName_ReqAppLog = CString(REQUEST_APPLOG_FILENAME);
	m_sMonitoringFileName_ReqWebLog = CString(REQUEST_WEBLOG_FILENAME);

	WRITELOG(_T("<CClientManager::Begin> MonitoringFolder : %s, Prev MonitoringFolder : %s"), (LPCTSTR)g_Log.m_sLogFolder, m_sPrevMonitoringFolder);
}

void CClientManager::SetServerPage(CServerInfo *pServerInfo, TCHAR *szIniFile, int nURLIndex )
{
	TCHAR pchBuf[BUFSIZ] = {0, };

	CAtlString sURL = _T(""), sSectionName = _T("");
	sURL = pServerInfo->m_sURL[nURLIndex];
	sSectionName = pServerInfo->m_sServerSection;

	if (m_stPageInfo.m_GSHS == DEFAULT_GSHS || m_stPageInfo.m_GSHS == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSHS"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSHS.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSHS : %s"), m_stPageInfo.m_GSHS);
		}
	}
	if (m_stPageInfo.m_GSLOOK == DEFAULT_GSLOOK || m_stPageInfo.m_GSLOOK == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSLOOK"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSLOOK.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSLOOK : %s"), m_stPageInfo.m_GSLOOK);
		}
	}
	if (m_stPageInfo.m_GSREG == DEFAULT_GSREG || m_stPageInfo.m_GSREG == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSREG"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSREG.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSREG : %s"), m_stPageInfo.m_GSREG);
		}
	}
	if (m_stPageInfo.m_GSFILE == DEFAULT_GSFILE || m_stPageInfo.m_GSFILE == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSFILE"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSFILE.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSFILE : %s"), m_stPageInfo.m_GSFILE);
		}
	}
	if (m_stPageInfo.m_GSALLOW == DEFAULT_GSALLOW || m_stPageInfo.m_GSALLOW == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSALLOW"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSALLOW.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSALLOW : %s"), m_stPageInfo.m_GSALLOW);
		}
	}
	// 2014/01/23 by realhan, 임시 정책 생성 로직 변경. Server -> Client
	if (m_stPageInfo.m_GSALLOW.Find(_T("gsallow6.2.jsp")) > -1)
	{
		m_bGSALLOW_62 = TRUE;
	}
	if (m_stPageInfo.m_GSAPP == DEFAULT_GSAPP || m_stPageInfo.m_GSAPP == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSAPP"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSAPP.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSAPP : %s"), m_stPageInfo.m_GSAPP);
		}
	}
	if (m_stPageInfo.m_GSBLOCK == DEFAULT_GSBLOCK || m_stPageInfo.m_GSBLOCK == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSBLOCK"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			// 2011/03/30, by realhan Commini 암호화 과정중 이상한 문자가 포함되어 있다.(ex. GSBLOCK=gsblock5.0.jsp"@wd?w?) 보완을 위해서 .jsp 전으로 파싱하여 사용한다.)
			CString s_gsblock = pchBuf;
			int pos = s_gsblock.Find(_T(".jsp"));
			m_stPageInfo.m_GSBLOCK.Format(_T("%s%s"), sURL, s_gsblock.Left(pos + 4));
			WRITELOG(_T("[SetServerPage] GSBLOCK : %s"), m_stPageInfo.m_GSBLOCK);
		}
	}
	if (m_stPageInfo.m_GSINTEGRITY == DEFAULT_GSINTEGRITY || m_stPageInfo.m_GSINTEGRITY == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSINTEGRITY"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSINTEGRITY.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSINTEGRITY : %s"), m_stPageInfo.m_GSINTEGRITY);
		}
	}
	if (m_stPageInfo.m_GDSFILE == DEFAULT_GDSFILE || m_stPageInfo.m_GDSFILE == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GDSFILE"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GDSFILE.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GDSFILE : %s"), m_stPageInfo.m_GDSFILE);
		}
	}
	if (m_stPageInfo.m_GDSALLOW == DEFAULT_GDSALLOW || m_stPageInfo.m_GDSALLOW == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GDSALLOW"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GDSALLOW.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GDSALLOW : %s"), m_stPageInfo.m_GDSALLOW);
		}
	}
	if (m_stPageInfo.m_GDSENCKEY == DEFAULT_GDSENCKEY || m_stPageInfo.m_GDSENCKEY == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GDSENCKEY"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GDSENCKEY.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GDSENCKEY : %s"), m_stPageInfo.m_GDSENCKEY);
		}
	}
	if (m_stPageInfo.m_GDSFILEREQ == DEFAULT_GDSFILEREQ || m_stPageInfo.m_GDSFILEREQ == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GDSFILEREQ"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GDSFILEREQ.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GDSFILE : %s"), m_stPageInfo.m_GDSFILE);
		}
	}
	if (m_stPageInfo.m_GSUSER == DEFAULT_GSUSER || m_stPageInfo.m_GSUSER == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSUSER"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSUSER.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSUSER : %s"), m_stPageInfo.m_GSUSER);
		}
	}
	if (m_stPageInfo.m_GSMULTI == DEFAULT_GSMULTI || m_stPageInfo.m_GSMULTI == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSMULTIFILE"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSMULTI.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSMULTI : %s"), m_stPageInfo.m_GSMULTI);
		}
	}
	if (m_stPageInfo.m_GSHW == DEFAULT_GSHW || m_stPageInfo.m_GSHW == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSHW"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSHW.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSHW : %s"), m_stPageInfo.m_GSHW);
		}
	}
	if (m_stPageInfo.m_GSDEV == DEFAULT_GSDEV61 || m_stPageInfo.m_GSDEV == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSDEV"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSDEV.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSDEV : %s"), m_stPageInfo.m_GSDEV);
		}
	}
	if (m_stPageInfo.m_GSALLOWUPLOAD == DEFAULT_GDSALLOWUPLOAD || m_stPageInfo.m_GSALLOWUPLOAD == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSALLOWUPLOAD"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSALLOWUPLOAD.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSALLOWUPLOAD : %s"), m_stPageInfo.m_GSALLOWUPLOAD);
		}
	}
	// PC보안감사 by yoon.2019.01.08.
	if (m_stPageInfo.m_GSPCSECU == DEFAULT_GSPCSECU || m_stPageInfo.m_GSPCSECU == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSPCSECU"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSPCSECU.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSPCSECU : %s"), m_stPageInfo.m_GSPCSECU);
		}
	}

	if (m_stPageInfo.m_sJsonFile == DEFAULT_GSJSONFILE || m_stPageInfo.m_sJsonFile == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSJSONFILE"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_sJsonFile.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSJOSNFILE : %s"), m_stPageInfo.m_sJsonFile);
		}
	}

	if (m_stPageInfo.m_sJsonBlockFile == DEFAULT_GSJSONBLOCKFILE || m_stPageInfo.m_sJsonBlockFile == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSJSONBLOCKFILE"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_sJsonBlockFile.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSJSONBLOCKFILE : %s"), m_stPageInfo.m_sJsonBlockFile);
		}
	}

	if (m_stPageInfo.m_GSHWDEVICE == DEFAULT_GSHWDEVICE || m_stPageInfo.m_GSHWDEVICE == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("GSHWDEVICE"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcscmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_GSHWDEVICE.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] GSHWDEVICE : %s"), m_stPageInfo.m_GSHWDEVICE);
		}
	}

	if (m_stPageInfo.m_sEXDS == DEFAULT_EXDS || m_stPageInfo.m_sEXDS == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("EXDS"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcsicmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_sEXDS.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] EXDS : %s"), m_stPageInfo.m_sEXDS);
		}
	}

	if (m_stPageInfo.m_sReciveMail == DEFAULT_RECIVEMAIL || m_stPageInfo.m_sReciveMail == _T(""))
	{
		ZeroMemory(pchBuf, BUFSIZ);
		GetPrivateProfileString(sSectionName, _T("RECIVEMAIL"), DEFAULT_CONFIGURATION, pchBuf, BUFSIZ, szIniFile);
		if (_tcsicmp(pchBuf, DEFAULT_CONFIGURATION))
		{
			m_stPageInfo.m_sReciveMail.Format(_T("%s%s"), sURL, pchBuf);
			WRITELOG(_T("[SetServerPage] RECIVEMAIL : %s"), m_stPageInfo.m_sReciveMail);
		}
	}

	SetObjectNPort(sURL);

}

void CClientManager::SetPrevServerPage( CString sSectionName, TCHAR *szIniFile )
{
	TCHAR pchBuf[BUFSIZ] = {0, };
	ZeroMemory(pchBuf, BUFSIZ);
	GetPrivateProfileString(sSectionName, _T("PGSFILE"), DEFAULT_GSFILE, pchBuf, BUFSIZ, szIniFile);
	m_stPageInfo.m_sPREV_GSFILE = pchBuf;

	ZeroMemory(pchBuf, BUFSIZ);
	GetPrivateProfileString(sSectionName, _T("PGSALLOW"), DEFAULT_GSALLOW, pchBuf, BUFSIZ, szIniFile);
	m_stPageInfo.m_sPREV_GSALLOW = pchBuf;

	ZeroMemory(pchBuf, BUFSIZ);
	GetPrivateProfileString(sSectionName, _T("PGSAPP"), DEFAULT_GSAPP, pchBuf, BUFSIZ, szIniFile);
	m_stPageInfo.m_sPREV_GSAPP = pchBuf;

	ZeroMemory(pchBuf, BUFSIZ);
	GetPrivateProfileString(sSectionName, _T("PGSBLOCK"), DEFAULT_GSBLOCK, pchBuf, BUFSIZ, szIniFile);
	CString s_gsblock = pchBuf;
	int pos = s_gsblock.Find(_T(".jsp"));
	m_stPageInfo.m_sPREV_GSBLOCK = s_gsblock.Left(pos + 4);

	ZeroMemory(pchBuf, BUFSIZ);
	GetPrivateProfileString(sSectionName, _T("PGSINTEGRITY"), DEFAULT_GSINTEGRITY, pchBuf, BUFSIZ, szIniFile);
	m_stPageInfo.m_sPREV_GSINTEGRITY = pchBuf;
}

CString CClientManager::GetServerInfo(CString sFullPath, CString &sServerURL)
{
	CString sPageName = _T("");
	WRITELOG(_T("[GetServerInfo] sFullPath : %s"), sFullPath);
	if (sFullPath.Find(_T("http://")) > -1 || sFullPath.Find(_T("https://")) > -1)
	{
		if (AfxExtractSubString(sServerURL, sFullPath, 2, _T('/')) == FALSE)
		{
			WRITELOG(_T("[CClientManager::GetServerInfo()] - url찾기 실패"));
		}
		if (sServerURL.Find(_T(":")) > -1)
		{
			CString sTemp = _T(""), sTemp2 = _T("");
			sTemp2 = sServerURL;
			AfxExtractSubString(sServerURL, sTemp2, 0, _T(':'));
			AfxExtractSubString(sTemp, sTemp2, 1, _T(':'));
			m_httpPort = _ttoi(sTemp);
		}
		CString sTemp = _T("");
		if (AfxExtractSubString(sTemp, sFullPath, 3, _T('/')) == FALSE)
		{
			WRITELOG(_T("[CClientManager::GetServerInfo()] - url찾기 실패2"));
		}
		m_shttpObject.Format(_T("/%s/"), sTemp);
		if (AfxExtractSubString(sTemp, sFullPath, 4, _T('/')) == FALSE)
		{
			WRITELOG(_T("[CClientManager::GetServerInfo()] - url찾기 실패3"));
		}
		m_shttpObject.AppendFormat(_T("%s/"), sTemp);
		if (AfxExtractSubString(sPageName, sFullPath, 5, _T('/')) == FALSE)
		{
			WRITELOG(_T("[CClientManager::GetServerInfo()] - pagename찾기 실패"));
		}
		if (sFullPath.Find(_T("https://")) > -1)
		{
			m_bIsHttps = TRUE;
		}
		else
		{
			m_bIsHttps = FALSE;
		}
	}

	return sPageName;
}

BOOL CClientManager::IsTLS12SupportOS()
{
	if(m_OSVerMajor >= 6)
	{
		return TRUE;
	}

	return FALSE;
}


void CClientManager::WriteLastLookServerTime()
{
	CString sCurrentTime = _T("");
	sCurrentTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));

	if (FileExistRegInfo() == TRUE)
	{
		WRITELOG(_T("[CClientManager::WriteLastLookServerTime] Write CurrentTime : %s"), sCurrentTime);
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_REGINFO_FILENAME, BMS_REGINFO_SERVERCONNECTION_LASTTIME, sCurrentTime, _T(""));
	}
	else
	{
		WRITELOG(_T("[CClientManager::WriteLastLookServerTime] reginfo 파일이 없다."));
	}
}

void CClientManager::CreateSIDFile()
{
	if (m_sAgentID != _T(""))
	{
		CString sAgentIDPath = _T("");
		sAgentIDPath.Format(_T("%stmp\\SID_*"), g_pBMSCommAgentDlg->m_sBMSDATLogPath);
		HANDLE hFind = INVALID_HANDLE_VALUE;
		WIN32_FIND_DATA FindFileData;
		hFind = FindFirstFile(sAgentIDPath, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			CString sAgentIdFileName = _T("");
			sAgentIdFileName.Format(_T("%stmp\\SID_%s"), g_pBMSCommAgentDlg->m_sBMSDATLogPath, m_sAgentID);
			HANDLE hAID = INVALID_HANDLE_VALUE;
			hAID = CreateFile(sAgentIdFileName, FILE_ALL_ACCESS, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hAID != INVALID_HANDLE_VALUE)
			{
				WRITELOG(_T("[CClientManager::CreateSIDFile] SID 파일 생성 완료 filename : %s"), sAgentIdFileName);
				CloseHandle(hAID);
			}
		}
		if (hFind != INVALID_HANDLE_VALUE)
		{
			FindClose(hFind);
		}
	}
}

void CClientManager::SetSaveTonerServerPage()
{
	CServerInfo *pNewServerInfo = new CServerInfo;

	m_sAgentID = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, _T(""), BMS_IASCOMM_SECTION);

	pNewServerInfo->m_sURL[0] = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_URL, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSHS = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSHS, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSLOOK = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSLOOK, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSREG = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSREG, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSFILE = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSFILE, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSALLOW = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSALLOW, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSAPP = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSAPP, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSBLOCK = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSBLOCK, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSINTEGRITY = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSINTEGRITY, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GDSFILE = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GDSFILE, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GDSALLOW = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GDSALLOW, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GDSENCKEY = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GDSENCKEY, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GDSFILEREQ = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GDSFILEREQ, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSUSER = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSUSER, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSMULTI = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSMULTIFILE, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSALLOWUPLOAD = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSALLOWUPLOAD, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_GSPCSECU = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSPCSECU, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_sJsonFile = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSJSONFILE, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_sJsonBlockFile = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSJSONBLOCKFILE, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_sEXDS = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_EXDS, BMS_IASCOMM_SECTION);
	m_stPageInfo.m_sReciveMail = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_RECIVEMAIL, BMS_IASCOMM_SECTION);
	m_sSiteName = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_REGINFO_FILENAME, BMS_REGINFO_SITE_NAME, _T(""));
	WRITELOG(_T("[CClientManager::SetSaveTonerServerPage] sitename : %s"), m_sSiteName);

	m_ServerInfoList.AddTail(pNewServerInfo);
}

void CClientManager::ProcessDiscoveryOfflineEndDataLog( CString sFolderPath )
{
	CString sEndLogPath = _T("");
	sEndLogPath.Format(_T("%s%s\\"), sFolderPath, BMDC_OFFLINE_DISCOVERY_END_DATA);
	BOOL bDiscoveryStateLog = TRUE, bDiscoveryTempPolicyLog = TRUE, bDsSeqLog = TRUE;
	do 
	{
		CFileIndex *pFile = NULL;
		if (bDiscoveryStateLog == TRUE)
		{
			pFile = g_Log.OpenFile(sEndLogPath + m_sMonitoringFileName_DsStateLog, TRUE);
			if (pFile != NULL)
			{
				WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineEndDataLog] DsStateLog filename:%s 발견"), pFile->m_sFileName);
				if (!ProcessActionLog(COMMAND_AGENT_DISCOVERY_STATELOG, pFile)) 
				{
					WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineLog] DSStateLog 전송 실패"));
					bDiscoveryStateLog = FALSE;
				}
			}
			if (m_bTerminate == TRUE)
			{
				break;
			}
		}
		CFileIndex *pFile2 = NULL;
		if (bDiscoveryTempPolicyLog == TRUE)
		{
			pFile2 = g_Log.OpenFile(sEndLogPath + m_sMonitoringFileName_DsEndInstantLog, TRUE);
			if (pFile2 != NULL)
			{
				WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineLog] DsEndInstantLog filename:%s 발견"), pFile2->m_sFileName);
				if (!ProcessActionLog(COMMAND_AGENT_OFFLINE_END_DSINSTANTLOG, pFile2)) 
				{
					WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineEndDataLog] DSTempPolicyLog 전송 실패"));
					bDiscoveryTempPolicyLog = FALSE;
				}
			}
			if (m_bTerminate == TRUE)
			{
				break;
			}
		}

		CFileIndex *pFile3 = NULL;
		if (bDsSeqLog == TRUE)
		{
			pFile3 = g_Log.OpenFile(sEndLogPath + m_sMonitoringFileName_DsSeqLog, TRUE);
			WRITELOG(_T("%s%s"), sEndLogPath, m_sMonitoringFileName_DsSeqLog);
			if(pFile3 != NULL)
			{
				WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineEndDataLog] DSSeq filename:%s 발견"), pFile3->m_sFileName);
				if(ProcessActionLog(COMMAND_AGENT_DISCOVERY_SEQLOG, pFile3) == FALSE) 
				{
					WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineEndDataLog] DSSeq 전송 실패."));
					bDsSeqLog = FALSE;
				}
			}
			if (m_bTerminate == TRUE)
			{
				break;
			}
		}

		if (pFile == NULL && pFile2 == NULL && pFile3 == NULL)
		{
			break;
		}

		ProcMessage();
	} while (TRUE);

	WIN32_FIND_DATA fdsFileData;
	HANDLE hFileHandle;
	hFileHandle = FindFirstFile((sFolderPath+_T("*.*")), &fdsFileData);
	if(hFileHandle != INVALID_HANDLE_VALUE)
	{
		do 
		{
			if (((fdsFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) && CString(fdsFileData.cFileName) != _T(".") && CString(fdsFileData.cFileName) != _T(".."))
			{
				CString sOfflineReqLogPath = _T("");
				sOfflineReqLogPath.Format(_T("%s%s\\"), sFolderPath, fdsFileData.cFileName);
				ProcessDiscoveryOfflineLog(sOfflineReqLogPath);

			}
		} while(FindNextFile(hFileHandle, &fdsFileData));
	}
	FindClose(hFileHandle);
}

void CClientManager::ProcessDiscoveryOfflineLog( CString sFolderPath )
{
	BOOL bDiscoveryOfflineReqLog = TRUE, bDiscoveryStateLog = TRUE, bDiscoveryTempPolicyLog = TRUE, bDsSeqLog = TRUE;
	do 
	{
		CFileIndex *pFile = NULL;
		if (bDiscoveryOfflineReqLog == TRUE)
		{
			pFile = g_Log.OpenFile(sFolderPath + m_sMonitoringFileName_DsOfflineReqLog, TRUE);
			if (pFile != NULL)
			{
				WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineLog] DsOfflineReqLog filename:%s 발견"), pFile->m_sFileName);
				if (!ProcessActionLog(COMMAND_AGENT_DISCOVERY_OFFLINEREQLOG, pFile)) 
				{
					WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineLog] DsOfflineReqLog 전송 실패"));
					bDiscoveryOfflineReqLog = FALSE;
				}
			}
			if (m_bTerminate == TRUE)
			{
				break;
			}
		}
		if (pFile == NULL)
		{
			break;
		}
		ProcMessage();
	} while (TRUE);

	do
	{
		CFileIndex *pFile2 = NULL;
		if (bDiscoveryTempPolicyLog == TRUE)
		{
			pFile2 = g_Log.OpenFile(sFolderPath + m_sMonitoringFileName_DsEndInstantLog, TRUE);
			if (pFile2 != NULL)
			{
				WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineLog] DsEndInstantLog filename:%s 발견"), pFile2->m_sFileName);
				if (!ProcessActionLog(COMMAND_AGENT_OFFLINE_END_DSINSTANTLOG, pFile2)) 
				{
					WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineLog] DSTempPolicyLog 전송 실패"));
					bDiscoveryTempPolicyLog = FALSE;
				}
			}
			if (m_bTerminate == TRUE)
			{
				break;
			}
		}
		CFileIndex *pFile3 = NULL;
		if (bDiscoveryStateLog == TRUE)
		{
			pFile3 = g_Log.OpenFile(sFolderPath + m_sMonitoringFileName_DsStateLog, TRUE);
			if (pFile3 != NULL)
			{
				WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineLog] DsStateLog filename:%s 발견"), pFile3->m_sFileName);
				if (!ProcessActionLog(COMMAND_AGENT_DISCOVERY_STATELOG, pFile3)) 
				{
					WRITELOG(_T("[CClientManager::ProcessDiscoveryOfflineLog] DSStateLog 전송 실패"));
					bDiscoveryStateLog = FALSE;
				}
			}
			if (m_bTerminate == TRUE)
			{
				break;
			}
		}

		CFileIndex *pFile4 = NULL;
		if (bDsSeqLog == TRUE)
		{
			pFile4 = g_Log.OpenFile(sFolderPath + m_sMonitoringFileName_DsSeqLog, TRUE);
			WRITELOG(_T("%s%s"), sFolderPath, m_sMonitoringFileName_DsSeqLog);
			if(pFile4 != NULL)
			{
				WRITELOG(_T("<MonitorLogFiles> DSSeq 로그 %s 발견"), pFile4->m_sFileName);

				if(ProcessActionLog(COMMAND_AGENT_DISCOVERY_SEQLOG, pFile4) == FALSE) 
				{
					WRITELOG(_T("<MonitorLogFiles::DevLog> COMMAND_AGENT_DISCOVERY_SEQLOG 전송 실패."));
					bDsSeqLog = FALSE;
				}
			}
			if (m_bTerminate == TRUE)
			{
				break;
			}
		}

		if (pFile2 == NULL && pFile3 == NULL && pFile4 == NULL)
		{
			break;
		}
		ProcMessage();
	} while (TRUE);
}

BOOL CClientManager::DeleteOfflineDiscoveryPrevFolder(__in CString sLogPath)
{
	BOOL bRet = FALSE;

	CString sDiscoveryOfflineReqLogPath = _T("");
	sDiscoveryOfflineReqLogPath = GetOfflineClearPath(sLogPath);

	bRet = DeleteDiscoveryPrevFolder(sDiscoveryOfflineReqLogPath);
	WRITELOG(_T("[CClientManager::DeleteOfflineDiscoveryPrevFolder] offline log 존재하므로 폴더 삭제 하지 않음 ret:%d"), bRet);

	return bRet;
}

BOOL CClientManager::DeleteDiscoveryPrevFolder(__in CString sLogPath)
{
	BOOL bFinal = TRUE, bRet = FALSE, bFolderExist = FALSE, bFileExist = FALSE;
	CString sOfflineReqLogPath = _T("");
	WIN32_FIND_DATA fdsFileData;
	HANDLE hFileHandle;
	hFileHandle = FindFirstFile((sLogPath+_T("*.*")), &fdsFileData);
	if (hFileHandle != INVALID_HANDLE_VALUE)
	{
		do 
		{
			if (((fdsFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) && CString(fdsFileData.cFileName) != _T(".") && CString(fdsFileData.cFileName) != _T(".."))
			{
				CTime cCurrentTime = CTime::GetCurrentTime();
				CString sCurrentTime = _T("");
				sCurrentTime.Format(_T("%04d%02d%02d"), cCurrentTime.GetYear(), cCurrentTime.GetMonth(), cCurrentTime.GetDay());

				if (sCurrentTime.CompareNoCase(fdsFileData.cFileName) != 0)
				{
					bFolderExist = TRUE;
					sOfflineReqLogPath.Format(_T("%s%s\\"), sLogPath, fdsFileData.cFileName);

					bRet = DeleteDiscoveryPrevFolder(sOfflineReqLogPath);
					if (bRet == FALSE)
					{
						bFinal = FALSE;
					}
				}
				else
				{
					return bRet;
				}
			}
			else if(CString(fdsFileData.cFileName) != _T(".") && CString(fdsFileData.cFileName) != _T(".."))
			{
				bFileExist = TRUE;
				break;
			}
		} while(FindNextFile(hFileHandle, &fdsFileData));
	}
	FindClose(hFileHandle);

	if ((bFolderExist == FALSE && bFileExist == FALSE) || (bFinal == TRUE && bFileExist == FALSE))
	{
		bRet = RemoveDirectory(sLogPath);
		WRITELOG(_T("[CClientManager::DeleteDiscoveryPrevFolder] path:%s, error:%d"), sLogPath, GetLastError());
	}
 	else
	{
		bRet = FALSE;
	}

	return bRet;
}

void CClientManager::DeletePrevPolicyFile()
{
	DeleteDLPPrevPolicyFile();
	DeleteDiscoveryPrevPolicyFile();
	DeleteAssetPolicyFile();		// 2015.03.17, keede122
	DeletePCSecuPrevPolicyFile();
	DeleteHardwareDevicePrevPolicyFile();
}

void CClientManager::DeleteDLPPrevPolicyFile()
{
	TCHAR xml_prev[MAX_PATH + 1] = {0, };
	GetPrevPolicyFilePath(xml_prev);
	if (FileExists(xml_prev) == TRUE)
	{
		::DeleteFile(xml_prev);
	}

	TCHAR ged_xml_prev[MAX_PATH + 1] = {0, };
	GetPrevPolicyGEDFilePath(ged_xml_prev, MAX_PATH);
	if (FileExists(ged_xml_prev) == TRUE)
	{
		::DeleteFile(ged_xml_prev);
	}

	TCHAR cfg_xml_prev[MAX_PATH + 1] = {0, };
	GetPrevPolicyCFGFilePath(cfg_xml_prev, MAX_PATH);
	if (FileExists(cfg_xml_prev) == TRUE)
	{
		::DeleteFile(cfg_xml_prev);
	}
}

void CClientManager::DeleteDiscoveryPrevPolicyFile()
{
	TCHAR szDsBufPrev[MAX_PATH + 1] = {0, };
	GetDsPrevPolicyFilePath(szDsBufPrev);

	if(FileExists(szDsBufPrev))
	{
		::DeleteFile(szDsBufPrev);
		WRITELOG(_T("[CClientManager::DeleteDiscoveryPrevPolicyFile] dat filename : %s"), szDsBufPrev);
	}

	TCHAR szDsGEDBufPrev[MAX_PATH + 1] = {0, };
	GetDsPrevPolicyGEDFilePath(szDsGEDBufPrev, MAX_PATH);

	if(FileExists(szDsGEDBufPrev))
	{
		::DeleteFile(szDsGEDBufPrev);
		WRITELOG(_T("[CClientManager::DeleteDiscoveryPrevPolicyFile] ged filename : %s"), szDsGEDBufPrev);
	}

	TCHAR szDisPolicy[MAX_PATH+sizeof(TCHAR)] = {0, };
	GetAgentPath(szDisPolicy);
	_tcsncat(szDisPolicy, BM_DISCOVERY_POLICY_CFG_FILENAME, MAX_PATH);
	TCHAR szDisPolicyPrev[MAX_PATH+sizeof(TCHAR)] = {0, };
	_tcsncpy(szDisPolicyPrev, szDisPolicy, MAX_PATH);
	_tcsncat(szDisPolicyPrev, _T(".prev"), MAX_PATH);

	if(FileExists(szDisPolicyPrev))
	{
		::DeleteFile(szDisPolicyPrev);
		WRITELOG(_T("[CClientManager::DeleteDiscoveryPrevPolicyFile] cfg filename : %s"), szDisPolicyPrev);
	}
}

void CClientManager::DeleteAssetPolicyFile()
{
	TCHAR xml_prev[MAX_PATH + 1] = {0, };
	GetAsPrevPolicyFilePath(xml_prev, MAX_PATH);
	if (FileExists(xml_prev) == TRUE)
	{
		::DeleteFile(xml_prev);
		WRITELOG(_T("[CClientManager::DeleteAssetPolicyFile] dat filename : %s"), xml_prev);
	}

	TCHAR ged_xml_prev[MAX_PATH + 1] = {0, };
	GetAsPrevPolicyGEDFilePath(ged_xml_prev, MAX_PATH);
	if (FileExists(ged_xml_prev) == TRUE)
	{
		::DeleteFile(ged_xml_prev);
		WRITELOG(_T("[CClientManager::DeleteAssetPolicyFile] ged filename : %s"), ged_xml_prev);
	}

	TCHAR cfg_xml_prev[MAX_PATH + 1] = {0, };
	GetAsPrevPolicyCFGFilePath(cfg_xml_prev, MAX_PATH);
	if (FileExists(cfg_xml_prev) == TRUE)
	{
		::DeleteFile(cfg_xml_prev);
		WRITELOG(_T("[CClientManager::DeleteAssetPolicyFile] cfg filename : %s"), cfg_xml_prev);
	}
}

void CClientManager::DeletePCSecuPrevPolicyFile()
{
	TCHAR xml_prev[MAX_PATH + 1] = {0, };
	GetPsPrevPolicyFilePath(xml_prev, MAX_PATH);
	if (FileExists(xml_prev) == TRUE)
	{
		::DeleteFile(xml_prev);
		WRITELOG(_T("[CClientManager::DeletePCSecuPolicyFile] dat filename : %s"), xml_prev);
	}

	TCHAR ged_xml_prev[MAX_PATH + 1] = {0, };
	GetPsPrevPolicyGEDFilePath(ged_xml_prev, MAX_PATH);
	if (FileExists(ged_xml_prev) == TRUE)
	{
		::DeleteFile(ged_xml_prev);
		WRITELOG(_T("[CClientManager::DeletePCSecuPolicyFile] ged filename : %s"), ged_xml_prev);
	}

	TCHAR cfg_xml_prev[MAX_PATH + 1] = {0, };
	GetPsPrevPolicyCFGFilePath(cfg_xml_prev, MAX_PATH);
	if (FileExists(cfg_xml_prev) == TRUE)
	{
		::DeleteFile(cfg_xml_prev);
		WRITELOG(_T("[CClientManager::DeletePCSecuPolicyFile] cfg filename : %s"), cfg_xml_prev);
	}
}

void CClientManager::DeleteHardwareDevicePrevPolicyFile()
{
	TCHAR xml_prev[MAX_PATH + 1] = {0, };
	GetHdPrevPolicyFilePath(xml_prev, MAX_PATH);
	if (FileExists(xml_prev) == TRUE)
	{
		::DeleteFile(xml_prev);
		WRITELOG(_T("[CClientManager::DeletePCSecuPolicyFile] dat filename : %s"), xml_prev);
	}

	TCHAR ged_xml_prev[MAX_PATH + 1] = {0, };
	GetHdPrevPolicyGEDFilePath(ged_xml_prev, MAX_PATH);
	if (FileExists(ged_xml_prev) == TRUE)
	{
		::DeleteFile(ged_xml_prev);
		WRITELOG(_T("[CClientManager::DeletePCSecuPolicyFile] ged filename : %s"), ged_xml_prev);
	}

	TCHAR cfg_xml_prev[MAX_PATH + 1] = {0, };
	GetHdPrevPolicyCFGFilePath(cfg_xml_prev, MAX_PATH);
	if (FileExists(cfg_xml_prev) == TRUE)
	{
		::DeleteFile(cfg_xml_prev);
		WRITELOG(_T("[CClientManager::DeletePCSecuPolicyFile] cfg filename : %s"), cfg_xml_prev);
	}
}

void CClientManager::InitAgentID(CAtlString sAgentID)
{
	m_sAgentID = sAgentID;
}

void CClientManager::RequestSelectedOfflinePolicy()
{
	CString sReq = _T(""), sReceived = _T(""), sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::RequestSelectedOfflinePolicy()] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18006"));

	CStringW wsCmd = sCmd, wsReq = _T("");
	int ret = 0;

	sReq.Format(_T("%s%c%d"), m_sAgentID, g_Separator, m_nOfflineLevelId);
	wsReq = sReq;
	ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sReceived, m_bIsHttps);
	if (ret == 0 && sReceived.GetLength() > 0 && sReceived.Find(_T("Level")) != -1 && sReceived.Find(_T("RegulationList")) != -1)
	{
		TCHAR szAgentPath[MAX_PATH] = {_T(""), };
		GetAgentPath(szAgentPath);
		CString sOfflinePolicyFilePath = _T("");
		sOfflinePolicyFilePath.Format(_T("%s\%s"), szAgentPath, BMS_DLP_OFFLINE_POLICY_FILENAME);
		MultiLanguageWriteFile(sOfflinePolicyFilePath, sReceived, m_bUseMultiLan?CHARSET_UTF8:CHARSET_MULTIBYTE);
	}
	WRITELOG(_T("[CClientManager::RequestSelectedOfflinePolicy()] SendAndReceive ret : %d"), ret);
}

void CClientManager::UserBaseOfflinePolicy()
{
	CString sUserBaseOfflinePolicy = _T(""), sContents = _T("");
	TCHAR lpLogPath[MAX_PATH] = {0, };
	GetAgentLogPath(lpLogPath, MAX_PATH);
	sUserBaseOfflinePolicy.Format(_T("%s%s\\UserBaseOfflinePolicy"), lpLogPath, BM_RDATA);

	sContents = MultiLanguageReadFile(sUserBaseOfflinePolicy, CHARSET_UTF8);
	g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_DLP_POLICY_FILENAME, sContents, _T(""));
	BackupPolicyFile();	
}

void CClientManager::SaveLookThreadExitTime()
{
	CString sCurrentTime = _T("");
	sCurrentTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));
	WRITELOG(_T("[SaveLookThreadExitTime] Time : %s"), sCurrentTime);
	m_sLastLookTime = sCurrentTime;
}

BOOL CClientManager::IsValidResponse(CString &sResp, CString sCmd)
{
	WRITELOG(_T("[CClientManager::IsValidResponse] Start"));
	BOOL bRet = FALSE;
	int nSUCCESSPOS = -1;
	nSUCCESSPOS = sResp.Find(_T("SUCCESS"));
	if (nSUCCESSPOS == 0)
	{
		WRITELOG(_T("[CClientManager::IsValidResponse] Valid!!!"));
		sResp.Delete(0, 8);
		bRet = TRUE;
	}
	else
	{
		WRITELOG(_T("[CClientManager::IsValidResponse] InValid!!!"));
	}

	return bRet;
}

void CClientManager::RequireReboot()
{
	WRITELOG(_T("[RequireReboot] Start"));
	CRebootPopupDlg cRebootDlg;
	cRebootDlg.DoModal();
}

CString CClientManager::GetReqData()
{
	TCHAR szLogPath[MAX_PATH] = {0, };
	CString sReqDataPath = _T(""), sRet = _T("");
	GetAgentLogPath(szLogPath, MAX_PATH);
	sReqDataPath.Format(_T("%s%s\\%s"), szLogPath, BM_RDATA, BM_REQ_DATA);

	if (m_bUseMultiLan == TRUE)	
	{
		sRet = MultiLanguageReadFile(sReqDataPath, CHARSET_UTF8);
	}
	else
	{
		sRet = MultiLanguageReadFile(sReqDataPath, CHARSET_MULTIBYTE);
	}

	return sRet;
}

void CClientManager::ModifyReqData(CString &sReqData, DWORD dwTableID)
{
	WRITELOG(_T("[ModifyReqData] Start!!"));

	int nSPos = 0;
	int nCnt = 0;
	CString sRow = _T(""), sTableID = _T(""), sRemoveRow = _T("");;

	while (AfxExtractSubString(sRow, sReqData, nCnt++, _T('|')))
	{
		AfxExtractSubString(sTableID, sRow, 0, g_Separator);
		if (_ttoi(sTableID) == dwTableID)
		{
			nSPos = sReqData.Find(sRow);
			sRemoveRow = sReqData.Mid(nSPos, sRow.GetLength()+1);
			WRITELOG(_T("[ModifyReqData] sRemoveRow : %s"), sRemoveRow);
			sReqData.Replace(sRow, _T(""));
			break;
		}
		else
		{
			continue;
		}
	}
}

void CClientManager::CheckOption()
{
	// 2010/10/12, avilon - agentID를 얻어 오지 못 한 경우 agentinfo에서 다시 읽어 온다.
	CString sTempAgentID = NumFilter(m_sAgentID);
	if(sTempAgentID == _T("0"))
	{
		WRITELOG(_T("[CClientManager::Begin] Agent 아이디가 이상하다. 값은 %s이다."), m_sAgentID);
		m_sAgentID = g_pBMSCommAgentDlg->GetAgentIdToSInfoFile();
		m_sAgentID = NumFilter(m_sAgentID);
		m_sAgentID = (m_sAgentID == _T("0")) ? _T("") : m_sAgentID;
		if (((m_sAgentID.GetLength() > 0) && (m_sAgentID != _T("0"))) && (m_sAgentID != _T("0")))
		{
			WRITELOG(_T("[CClientManager::Begin] 에이전트 아이디를 다시 쓴다."));
			g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTID, m_sAgentID, _T(""));
			WRITELOG(_T("[CClientManager::Begin] Agent 아이디. 값은 %s이다"), m_sAgentID);
		}
	}

	// 2011/12/28, avilon -  에이전트 아이디 이상.
	if(m_sAgentID.GetLength() > 10)
	{
		CString sSIDPath = _T("");
		sSIDPath.Format(_T("%stmp\\SID_*"), g_pBMSCommAgentDlg->m_sBMSDATLogPath);
		HANDLE hFind = INVALID_HANDLE_VALUE;
		WIN32_FIND_DATA FindFileData;
		hFind = FindFirstFile(sSIDPath, &FindFileData);
		if(hFind != INVALID_HANDLE_VALUE)
		{
			sSIDPath.Format(_T("%stmp\\%s"), g_pBMSCommAgentDlg->m_sBMSDATLogPath, FindFileData.cFileName);
			::DeleteFile(sSIDPath);
			FindClose(hFind);
		}

		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.SetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTID, _T(""), _T(""));

		if (m_bIsLogEncoded == FALSE)
		{
			RegSetStringValue(HKEY_LOCAL_MACHINE, REG_PATH_IAS, REG_STRING_AGENTID, _T(""));
		}
		m_sAgentID = _T("");
	}

	if (m_OSVerMajor >= 6)
	{
		m_sSupportName = GetLoginName();
	}
	else
	{
		// 2010/10/26, avilon
		// XP이더라도 시스템으로 실행 하기 때문에 실제 로그인 아이디를 얻으려면 유저 권한으로 실행 시켜야 된다.
		TCHAR lpUserName[MAX_PATH] = {0, };
		_tcsncpy_s(lpUserName, MAX_PATH, GetLoginName(), _TRUNCATE);
		if (!_tcsicmp(lpUserName, _T("system")))
		{
			CBMSSupport Support;
			Support.GetFromSupport(lpUserName, 10000, m_bUseMultiLan);
			WRITELOG(_T("## 시스템으로 돌고 있어서 서포트 에이전트를 통해 다시 계정이름을 받아 왔다. %s"), lpUserName);
		}
		m_sSupportName = GetLoginName();

	}
	WRITELOG(_T("[CClientManager::Begin] - m_sAgentID : %s, Length : %d"), m_sAgentID, m_sAgentID.GetLength());
	if (m_sAgentID.GetLength() == 0)
	{
		// 에이전트 아이디 설정
		m_bNeedAgentID = TRUE;
		m_bFirstInstall = TRUE;
		WRITELOG(_T("[CClientManager::Begin] * NeedAgentID *"));
	}

	if (m_bIsSetCommini == FALSE)
	{
		InitComm();
	}

}

void CClientManager::InitManageList()
{
	m_sAgentID = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_AGENTID, _T(""));
	m_OfflinePolicy = _ttoi(g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_OFFLINEPOLICY, _T("")));
	m_bIsLogEncoded = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_LOGENCODE, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;
	m_bUseMultiLan = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_CHARACTERSET, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;
	m_bUseDiscovery = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_DISCOVERY_LIST_USEMULTILANG, BMS_MNG_DISCOVERY) == _T("true")) ? TRUE : FALSE;
	// 2015.03.16, keede122
	m_bUseAsset = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_USE_ASSET_MANAGER, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;
	m_sProjectName = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_AGENTINFO_FILENAME, BMS_AGENTINFO_PROJECTNAME, _T(""));
	m_bADUnLock = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_AD_UNLOCK, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;
	m_bFileAgent_boot_run = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_FILEAGENT_BOOT_RUN, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;
	m_bHardwareSpec = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_HARDWARESPEC, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;
	m_bNeedRebootAfterMapping = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_NEED_REBOOT_AFTER_MAPPING, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;
	m_bUsePublicEncryptKeyForDiscovery = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_DISCOVERY_LIST_USE_PUBLIC_ENCRYPTKEY , BMS_MNG_DISCOVERY) == _T("true")) ? TRUE : FALSE;
	m_dwLookConnectionLimitCount = _ttoi(g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_LOOK_CONNECTION_LIMIT_COUNT, BMS_MNG_CUSTOMLIST));
	m_bUpgradePass = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_UPGRADE_PASS, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;
	m_bUsePCSecu = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_USE_PCSECU, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;
	m_bHardwareDvice = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_HARDWAREDEVICE, BMS_MNG_CUSTOMLIST) == _T("true")) ? TRUE : FALSE;

	CString sOtherLang = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_FIXED_OTHERLANGUAGE, BMS_MNG_CUSTOMLIST);
	if (sOtherLang.GetLength() > 0)
	{
		LANGID otherLangID = GetFixedOtherlanguageID(sOtherLang);
		m_LangID = ReplaceDefaultLangID(m_LangID, otherLangID);
		WRITELOG(_T("[CClientManager::InitManageList] 기본 언어 설정을 변경 - %s(%d)"), sOtherLang, m_LangID);
	}
// 2017.11.06 kjh4789 tls1_2 기본 TRUE 설정
// 	if (g_bSetupTLS1_2 == FALSE)
// 	{
		// 2019.02.07 kjh4789 Main Thread에서는 tls1_2가 실패, LookThread에서는 tls1_2가 성공하는 케이스가 확인됨. 옵션에 따라서 tls1_2 기능을 끌 수 있도록 처리하도록 처리
		m_bTLS1_2 = (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_TLS1_2, BMS_MNG_CUSTOMLIST) == _T("false")) ? FALSE : TRUE;
// 		g_bSetupTLS1_2 = TRUE;
// 	}
	WRITELOG(_T("[CClientManager::InitManageList] agentid : %s, offlinepolicy : %d, AD_unlock : %d, m_bTLS1_2 : %d"), m_sAgentID, m_OfflinePolicy, m_bADUnLock, m_bTLS1_2);
}

void CClientManager::SetObjectNPort(CAtlString sURL)
{
	if (sURL.Find(_T("http://")) != -1)
	{
		m_bIsHttps = FALSE;
		m_httpPort = 80;
		sURL = sURL.Right(sURL.GetLength() - 7);
		WRITELOG(_T("[ClientManager -> Begin] HTTP Connection!!"));

		m_bTLS1_2 = FALSE;
	}
	else if (sURL.Find(_T("https://")) != -1)
	{
		m_bIsHttps = TRUE;
		m_httpPort = 443;
		sURL = sURL.Right(sURL.GetLength() - 8);
		WRITELOG(_T("[ClientManager -> Begin] HTTPS Connection!!"));
	}

	int nPos = 0, nTempPos = 0;	
	nPos = sURL.Find(_T("/"), nPos);
	m_wshttpServerUrl = sURL.Left(nPos);
	m_shttpObject = sURL.Right(sURL.GetLength() - nPos);
	if((nTempPos = m_wshttpServerUrl.Find(_T(":"))) != -1)
	{
		CString sTemp = _T(""), sTemp2 = m_wshttpServerUrl;
		AfxExtractSubString(sTemp, sTemp2, 0, _T(':'));
		m_wshttpServerUrl = sTemp;
		AfxExtractSubString(sTemp, sTemp2, 1, _T(':'));
		m_httpPort = _ttoi(sTemp.GetBuffer());
	}
	WRITELOG(_T("Url = %s Object = %s port = %d") , m_wshttpServerUrl, m_shttpObject, m_httpPort);
}

void CClientManager::CheckNChangeServerInfo()
{
	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);

	CString sChkCmd = _T(""), sResp = _T("");
	sChkCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18009"));
	int nFirstConnectErr = 0;
	nFirstConnectErr = CheckServerConnectInfo((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sChkCmd, _T(""), sResp, m_bIsHttps);

	if (nFirstConnectErr == 0 || nFirstConnectErr == BM_SERVER_ERR_NOT_SUPPORT_CMD) // 성공하거나 지원하지 않는 cmd
	{
		WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 현재 설정 그대로 적용!!"));
	}
	else if (nFirstConnectErr == ERROR_WINHTTP_SECURE_FAILURE)
	{
		BOOL bBackupTLS1_2 = m_bTLS1_2;
		if (m_bTLS1_2 == TRUE)
		{
			WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 첫통신 실패 tls1_2 플래그 있어서 제거하고 통신 시도"));
			m_bTLS1_2 = FALSE;
		}
		else
		{
			WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 첫통신 실패 tls1_2 플래그 넣고 통신 시도"));
			m_bTLS1_2 = TRUE;
		}
		nFirstConnectErr = CheckServerConnectInfo((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sChkCmd, _T(""), sResp, m_bIsHttps);

		if (nFirstConnectErr != NULL) // 성공이 아니면
		{
			WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 통신 실패로 판단하고 tls1_2 플래그 백업"));
			m_bTLS1_2 = bBackupTLS1_2;
		}
		else 
		{
			WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 재 통신 시도 성공 TLS1_2 : %d"), m_bTLS1_2);
		}
	}
	else
	{
		WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 서버접속 실패 정보 변경 필요!! - %d"), nFirstConnectErr);

		BOOL bSuccess = FALSE;

		CString sSubServerURL = _T(""), sGSHSPageName = _T("");
		sGSHSPageName = DEFAULT_GSHS;

		CServerInfo *pServerInfo = NULL;
		POSITION pos = m_ServerInfoList.GetHeadPosition();
		while (pos)
		{
			pServerInfo = (CServerInfo*)m_ServerInfoList.GetNext(pos);
			for (int nCnt = 1; nCnt <= SERVER_URL_COUNT; nCnt++)
			{
				if (pServerInfo->m_sURL[nCnt] == _T(""))
					break;

				CString sGSHSPage = _T("");
				sGSHSPage.Format(_T("%s%s"), pServerInfo->m_sURL[nCnt], sGSHSPageName);

				CString sPageName = GetServerInfo(sGSHSPage, sSubServerURL);
				CString sChkCmd = _T(""), sResp = _T("");
				sChkCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18009"));
				nFirstConnectErr = CheckServerConnectInfo((sSubServerURL == _T("")) ? m_wshttpServerUrl : sSubServerURL, sChkCmd, _T(""), sResp, m_bIsHttps);
				if (nFirstConnectErr == 0 || nFirstConnectErr == BM_SERVER_ERR_NOT_SUPPORT_CMD)
				{
					ChangeServerInfo(pServerInfo, nCnt);
					bSuccess = TRUE;
					WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 서버접속 정보 변경 완료!!"));
				}
				else if (nFirstConnectErr == ERROR_WINHTTP_SECURE_FAILURE)
				{
					BOOL bBackupTLS1_2 = m_bTLS1_2;
					if (m_bTLS1_2 == TRUE)
					{
						WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 첫통신 실패 tls1_2 플래그 있어서 제거하고 통신 시도"));
						m_bTLS1_2 = FALSE;
					}
					else
					{
						WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 첫통신 실패 tls1_2 플래그 넣고 통신 시도"));
						m_bTLS1_2 = TRUE;
					}
					nFirstConnectErr = CheckServerConnectInfo((sSubServerURL == _T("")) ? m_wshttpServerUrl : sSubServerURL, sChkCmd, _T(""), sResp, m_bIsHttps);

					if (nFirstConnectErr != NULL) // 성공이 아니면
					{
						WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 통신 실패로 판단하고 tls1_2 플래그 백업"));
						m_bTLS1_2 = bBackupTLS1_2;
					}
					else 
					{
						WRITELOG(_T("[CClientManager::CheckNChangeServerInfo] 재 통신 시도 성공 TLS1_2 : %d"), m_bTLS1_2);
						ChangeServerInfo(pServerInfo, nCnt);
						bSuccess = TRUE;
					}
				}
			}

			if (!bSuccess)
			{
				ChangeServerInfo(pServerInfo, 0);
			}
		}
	}

}

void CClientManager::ChangeServerInfo(CServerInfo *pServerInfo, int nURLIndex )
{

	CAtlString sURL = _T("");
	sURL = pServerInfo->m_sURL[nURLIndex];

	CString sTemp = _T("");
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSHS, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSHS.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSHS : %s"), m_stPageInfo.m_GSHS);
	}

	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSLOOK, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSLOOK.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSLOOK : %s"), m_stPageInfo.m_GSLOOK);
	}

	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSREG, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSREG.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSREG : %s"), m_stPageInfo.m_GSREG);
	}

	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSFILE, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSFILE.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSFILE : %s"), m_stPageInfo.m_GSFILE);
	}

	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSALLOW, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSALLOW.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSALLOW : %s"), m_stPageInfo.m_GSALLOW);
	}

	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSAPP, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSAPP.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSAPP : %s"), m_stPageInfo.m_GSAPP);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSBLOCK, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSBLOCK.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSBLOCK : %s"), m_stPageInfo.m_GSBLOCK);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSINTEGRITY, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSINTEGRITY.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSINTEGRITY : %s"), m_stPageInfo.m_GSINTEGRITY);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GDSFILE, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GDSFILE.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GDSFILE : %s"), m_stPageInfo.m_GDSFILE);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GDSALLOW, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GDSALLOW.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GDSALLOW : %s"), m_stPageInfo.m_GDSALLOW);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GDSENCKEY, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GDSENCKEY.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GDSENCKEY : %s"), m_stPageInfo.m_GDSENCKEY);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GDSFILEREQ, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GDSFILEREQ.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GDSFILEREQ : %s"), m_stPageInfo.m_GDSFILEREQ);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSUSER, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSUSER.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSUSER : %s"), m_stPageInfo.m_GSUSER);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSMULTIFILE, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSMULTI.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSMULTI : %s"), m_stPageInfo.m_GSMULTI);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSHW, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSHW.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSHW : %s"), m_stPageInfo.m_GSHW);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSDEV, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSDEV.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSDEV : %s"), m_stPageInfo.m_GSDEV);
	}
	
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSALLOWUPLOAD, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSALLOWUPLOAD.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSALLOWUPLOAD : %s"), m_stPageInfo.m_GSALLOWUPLOAD);
	}

	// PC보안감사 by yoon.2019.01.08.
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSPCSECU, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSPCSECU.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSPCSECU : %s"), m_stPageInfo.m_GSPCSECU);
	}

	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSJSONFILE, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_sJsonFile.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_sJsonFile : %s"), m_stPageInfo.m_sJsonFile);
	}

	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSJSONBLOCKFILE, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_sJsonBlockFile.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_sJsonBlockFile : %s"), m_stPageInfo.m_sJsonBlockFile);
	}

	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_GSHWDEVICE, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_GSHWDEVICE.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_GSHWDEVICE : %s"), m_stPageInfo.m_GSHWDEVICE);
	}
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_EXDS, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_sEXDS.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_sEXDS : %s"), m_stPageInfo.m_sEXDS);
	}
	sTemp = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_COMMINI_FILENAME, BMS_IASCOMM_RECIVEMAIL, pServerInfo->m_sServerSection);
	if (sTemp != _T(""))
	{
		m_stPageInfo.m_sReciveMail.Format(_T("%s%s"), sURL, sTemp);
		WRITELOG(_T("[ChangeServerInfo] m_stPageInfo.m_sReciveMail : %s"), m_stPageInfo.m_sReciveMail);
	}
	
	// 2014/01/23 by realhan, 임시 정책 생성 로직 변경. Server -> Client
	if (m_stPageInfo.m_GSALLOW.Find(_T("gsallow6.2.jsp")) > -1)
	{
		m_bGSALLOW_62 = TRUE;
	}

	SetObjectNPort(sURL);
}

//2017.11.29 kjh4789 Discovery Encrypt Key 구조 변경
void CClientManager::RequestEncryptKeyValue()
{
	int nRet = FALSE;
	CString sAgentEncryptKey = _T("");

	if (m_bUsePublicEncryptKeyForDiscovery == TRUE)
	{
		if (CheckEncryptKey(BM_DISCOVERY_PUBLICKEYVALUE_FILENAME) == TRUE)
		{
			m_bReceivedDiscoveryKey = TRUE;
		}
		else
		{
			nRet = RequestPublicKeyValue();
			if (nRet == BM_SERVER_ERR_NOT_SUPPORT_CMD)
			{
				WRITELOG(_T("[RequestEncryptKeyValue] 지원하지 않는 cmd다!! 개인 키를 요청한다!!"));
				if (CheckEncryptKey(BM_DISCOVERY_ENCKEYVALUE_FILENAME) == TRUE)
				{
					m_bReceivedDiscoveryKey = TRUE;
				}
				else
				{
					nRet = RequestPrivateKeyValue();
				}
			}
		}
	}
	else
	{
		if (CheckEncryptKey(BM_DISCOVERY_ENCKEYVALUE_FILENAME) == TRUE)
		{
			m_bReceivedDiscoveryKey = TRUE;
		}
		else
		{
			nRet = RequestPrivateKeyValue();
		}
	}

	WRITELOG(_T("[RequestEncryptKeyValue] End - %d"), nRet);

	return;
}

BOOL CClientManager::CheckEncryptKey(CString sFileName)
{
	BOOL bRet = FALSE;
	TCHAR szPath[MAX_PATH] = {0, };
	GetAgentPath(szPath, MAX_PATH);

	CString sFilePath = _T(""), sData = _T("");
	sFilePath.Format(_T("%s%s"), szPath, sFileName);

	if (IsExistEncryptKey(sFilePath) == TRUE)
	{
		WRITELOG(_T("[CheckEncryptKey] 파일 존재 - %s"), sFilePath);
		sData = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigData(sFileName);
		WRITELOG(_T("[CheckEncryptKey] 복호화 데이타 - %s"), sData);
		int nLength = sData.GetLength();
		if (nLength == 16 || nLength == 64)
		{
			WRITELOG(_T("[CheckEncryptKey] Data 길이가 유효!!"));
			CString sCustomer = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, BMS_MNG_CUSTOMER_LIST_CUSTOMER, BMS_MNG_CUSTOMLIST);
			if (sCustomer.Compare(_T("AMORE")) == 0 && sFileName.Compare(BM_DISCOVERY_ENCKEYVALUE_FILENAME) == 0 &&sData.Compare(_T("rPf2FZGkUxpVocI6")) && m_sAgentID != 2382)
			{
				WRITELOG(_T("[CheckEncryptKey] 아모레 커스터마이징!! 키가 유효하지 않다.!!"));
			}
			else
			{
				bRet = TRUE;
			}
		}
		else
		{
			WRITELOG(_T("[CheckEncryptKey] Data 길이가 유효하지 않음 - %d"), nLength);
		}
	}
	else
	{
		WRITELOG(_T("[CheckEncryptKey] 파일 찾기 실패 - %s, %d"), sFilePath, GetLastError());
	}
	
	return bRet;
}

int CClientManager::RequestPrivateKeyValue()
{
	int nRet = 0;
	CString sReceived = _T(""), sCmd = _T(""), sPacketNum = _T(""), sServerURL = _T(""), sReq = m_sAgentID;
	CString sPageName = GetServerInfo(m_stPageInfo.m_GDSENCKEY, sServerURL);
	WRITELOG(_T("[ClientManager::RequestPrivateKeyValue] - m_stPageInfo.m_GDSENCKEY - %s"), m_stPageInfo.m_GDSENCKEY);
	if (m_bIsLogEncoded == TRUE)
	{
		CString sDiscoveryKey = _T("");
		sPacketNum = _T("19302");
		sDiscoveryKey = MakeRandomHashData();
		sReq += g_Separator;					
		sReq += sDiscoveryKey;
	}
	else
	{
		sPacketNum = _T("19300");
	}
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GDSENCKEY : sPageName, sPacketNum);
	nRet = SendAndReceive((sServerURL == _T(""))? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, m_bIsHttps);
	if (sReceived.GetLength() == 0)
	{
		WRITELOG(_T("[CClientManager::RequestPrivateKeyValue] - ENCRYPT KEY 받지 못함"));
		return -2;
	}

	if ((!nRet && (sReceived.GetLength() == 16)) || sReceived.GetLength() == 64)
	{
		if (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BM_DISCOVERY_ENCKEYVALUE_FILENAME, sReceived, _T("")))
		{
			m_bReceivedDiscoveryKey = TRUE;
			HWND hWnd = ::FindWindow(_T("#32770"), BMDC_DOC_CHECKER_WND_TITLE);
			if (hWnd)
			{
				WRITELOG(_T("[CClientManager::RequestPrivateKeyValue] - 디스커버리 ENCRYPT KEY 알림."));
				::PostMessage(hWnd, BMDC_UPDATE_ENCKEY, NULL, NULL);
			}
		}
	}

	return nRet;
}

int CClientManager::RequestPublicKeyValue()
{
	int nRet = 0;
	CString sReceived = _T(""), sCmd = _T(""), sServerURL = _T(""), sReq = m_sAgentID;
	CString sPageName = GetServerInfo(m_stPageInfo.m_GDSENCKEY, sServerURL);
	WRITELOG(_T("[ClientManager::RequestPublicKeyValue] - m_stPageInfo.m_GDSENCKEY - %s"), m_stPageInfo.m_GDSENCKEY);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GDSENCKEY : sPageName, _T("19303"));
	nRet = SendAndReceive((sServerURL == _T(""))? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, m_bIsHttps);

	if (sReceived.GetLength() == 0)
	{
		WRITELOG(_T("[CClientManager::RequestPrivateKeyValue] - ENCRYPT KEY 받지 못함"));
		return -2;
	}

	if ((!nRet && (sReceived.GetLength() == 16)) || sReceived.GetLength() == 64)
	{
		if (g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BM_DISCOVERY_PUBLICKEYVALUE_FILENAME, sReceived, _T("")))
		{
			m_bReceivedDiscoveryKey = TRUE;
			HWND hWnd = ::FindWindow(_T("#32770"), BMDC_DOC_CHECKER_WND_TITLE);
			if (hWnd)
			{
				WRITELOG(_T("[CClientManager::RequestPublicKeyValue] - 디스커버리 ENCRYPT KEY 알림."));
				::PostMessage(hWnd, BMDC_UPDATE_ENCKEY, NULL, NULL);
			}
		}
	}

	return nRet;
}

BOOL CClientManager::IsExistEncryptKey(CString sFilePath)
{
	BOOL bRet = FALSE;
	CString sExt[3] = {_T(".cfg"), _T(".ged"), _T(".ini")};
	for (int nCnt = 0; nCnt < 3; nCnt++)
	{
		CString sFileFullPath = _T("");
		sFileFullPath.Format(_T("%s%s"), sFilePath, sExt[nCnt]);
		if (FileExists(sFileFullPath))
		{
			WRITELOG(_T("[IsExistEncryptKey] File Find!! - %s"), sFileFullPath);
			bRet = TRUE;
			break;
		}
	}

	return bRet;
}

BOOL CClientManager::ChangeLastLoginInfoToUserIDFile()
{
	BOOL bRet = FALSE;
	CString sTempFolder = _T(""), sLastLoginInfoPath = _T(""), sSrcFilePath = _T(""), sDestFilePath = _T(""), sFileName = _T(""), sUserKey = _T(""), sAgentID = _T(""), sUserID = _T("");
	sTempFolder.Format(_T("%s\\tmp\\"), g_pBMSCommAgentDlg->m_sBMSDATLogPath);
	sLastLoginInfoPath.Format(_T("%sUSERBASEForLastLogin_*.*"), sTempFolder);

	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA FindFileData;

	hFind = FindFirstFile(sLastLoginInfoPath, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		WRITELOG(_T("[ChangeLastLoginInfoToUserIDFile] 로그 확인 불가!! 빠져나간다."));
		return bRet;
	}
	else
	{
		sFileName = FindFileData.cFileName;
		AfxExtractSubString(sUserKey, sFileName, 1, _T('_'));
		AfxExtractSubString(sAgentID, sFileName, 2, _T('_'));
		AfxExtractSubString(sUserID, sFileName, 3, _T('_'));
		sSrcFilePath.Format(_T("%s%s"), sTempFolder, FindFileData.cFileName);
		sDestFilePath.Format(_T("%sUSERBASE_%s_%s_%s"), sTempFolder, sUserKey, sAgentID, sUserID);
		WRITELOG(_T("[ChangeLastLoginInfoToUserIDFile] sSrcFilePath : %s, sDestFilePath : %s"), sSrcFilePath, sDestFilePath);
		for (int nTry = 0; nTry < 5; nTry++)
		{
			bRet = CopyFile(sSrcFilePath, sDestFilePath, FALSE);
			if (bRet == TRUE)
			{
				WRITELOG(_T("[ChangeLastLoginInfoToUserIDFile] CopyFile Success!!"));
				break;
			}
			else
			{
				WRITELOG(_T("[ChangeLastLoginInfoToUserIDFile] CopyFile Fail!! : %d, Try : %d"), GetLastError(), nTry);
			}
		}
		
	}

	return bRet;
}

BOOL CClientManager::RequestMessageSendToUser()
{
	BOOL bRet = FALSE;
	int ret = 0;
	CString sReq = _T(""), sReceived = _T(""), sCmd = _T(""), sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);
	WRITELOG(_T("[CClientManager::RequestMessageSendToUser] - m_stPageInfo.m_GSHS - %s"), m_stPageInfo.m_GSHS);
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18600"));
	sReq.Format(_T("%s%c%s"), m_sAgentID, g_Separator, GetDateTime().c_str());

	ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, m_bIsHttps);
	WRITELOG(_T("[CClientManager::RequestSelectedOfflinePolicy()] SendAndReceive ret : %d, sReceived : %s"), ret, sReceived);
	
	if (ret == 0 && sReceived != _T(""))
	{
		bRet = TRUE;
		CString sFilePath = _T(""), sMessageSendToUser = _T("");
		TCHAR lpLogPath[MAX_PATH] = {0, };
		GetAgentLogPath(lpLogPath, MAX_PATH);
		sMessageSendToUser.Format(_T("%s%s\\MessageSendToUser"), lpLogPath, BM_RDATA);
		if (MultiLanguageWriteFile(sMessageSendToUser, sReceived, CHARSET_UTF8) == TRUE)
		{
			WRITELOG(_T("[RequestMessageSendToUser] Success!!"));
			HWND hWnd = ::FindWindow(_T("#32770"), BM_GTRAYICON_WND_TITLE);
			if (hWnd)
			{
				WRITELOG(_T("[RequestMessageSendToUser] BMSM_GTRAY_REFRESH_LIST 전송!!"));
				PostMessage(hWnd, BMSM_GTRAY_REFRESH_LIST, 9, NULL);
			}
		}
	}
	
	return bRet;
}

// PC보안감사 by yoon.2019.01.08.
void CClientManager::RequestPoliciesPCSecu()
{
	m_sPCSecuPolicyTime = _T("");

	CString sCmd, sReceived = _T(""), policy_enc_data=_T("");
	CString sReq = m_sAgentID;

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSPCSECU, sServerURL);
	WRITELOG(_T("[CClientManager::RequestPoliciesPCSecu()] - m_stPageInfo.m_GSPCSECU - %s"), m_stPageInfo.m_GSPCSECU);

	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSPCSECU : sPageName, _T("21001"));
	int nRet = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, sReq, sReceived, m_bIsHttps);

	if (sReceived.GetLength() == 0)
	{
		WRITELOG(_T("CBMSCommAgentDlg::RequestPoliciesPCSecu - 정책을 받지 못함"));
		return;
	}

	if (sReceived.Find(_T("Level")) == -1 && sReceived.Find(_T("CheckList")) == -1)
	{
		WRITELOG(_T("CBMSCommAgentDlg::RequestPoliciesPCSecu - 정상적인 정책이 아니다 sReceived : %s"), sReceived);
		return;
	}

	if (!nRet)
	{
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_PCSECU_POLICY_FILENAME, sReceived, _T(""));
		m_sPCSecuPolicyTime = CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S"));
	}
	WRITELOG(_T("[RequestPoliciesPCSecu] 정책 파일 생성, GetLastError(%d)"), GetLastError());

	HWND hWnd = ::FindWindow(_T("#32770"), BM_PCSECURITY_WND_TITLE);
	if (hWnd)
	{
		WRITELOG(_T("[CClientManager::RequestPCSecuPolicy] - PCSecu 정책 업데이트 알림."));
		::PostMessage(hWnd, WM_MSG_PCSECU_UPDATEPOLICY, NULL, NULL);
	}
}

//	조회 페이지에서 PC정보요청으로 즉시 검사
void CClientManager::RequestResultPCSecu()
{
	HWND hWnd = ::FindWindow(_T("#32770"), BM_PCSECURITY_WND_TITLE);
	if (hWnd)
	{
		WRITELOG(_T("[CClientManager::RequestPCSecuPolicy] ."));
		::PostMessage(hWnd, WM_MSG_PCSECU_REQUESTRESULT, NULL, NULL);
	}
}

void CClientManager::UpgradeFailProc()
{
	TCHAR szLogPath[MAX_PATH] = {0, };
	GetAgentPath(szLogPath, MAX_PATH);
	_tcsncat(szLogPath, _T("UpgradeFail"), MAX_PATH);

	if (FileExists(szLogPath))
	{
		WRITELOG(_T("[UpgradeFailProc] Upgrade가 실패했다. Upgrade Flag를 제거한다."));
		m_bUpgrade = FALSE;
		::DeleteFile(szLogPath);
	}
}

void CClientManager::UserBasedForceLogout()
{
	WRITELOG(_T("[CClientManager::UserBasedForceLogout] 시작"));
	if (g_pBMSCommAgentDlg->m_bUserBaseForSSO == FALSE)
	{
		g_pBMSCommAgentDlg->UserBasedLogout(_T("forcelogout"));
	}
	WRITELOG(_T("[CClientManager::UserBasedForceLogout] 완료"));
}

BOOL CClientManager::RequestInternalList()
{
	BOOL bRet = FALSE;

	TCHAR szPath[MAX_PATH] = {0, };
	GetAgentPath(szPath, MAX_PATH);
	CString sInternalIPListFilePath = _T("");
	sInternalIPListFilePath.Format(_T("%sInternalIPList.json"), szPath);

	CString sServerURL = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHS, sServerURL);

	CString sCmd = _T(""), sResp = _T("");
	sCmd.Format(_T("%s%s?cmd=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHS : sPageName, _T("18010"));
	int nFirstConnectErr = 0;
	nFirstConnectErr = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, sCmd, _T(""), sResp, m_bIsHttps);
	if (nFirstConnectErr == BM_SERVER_ERR_LIST_NOT_EXIST) // ERR-1008 #리스트 없음
	{
		::DeleteFile(sInternalIPListFilePath);
		WRITELOG(_T("[RequestInternalList] 내부 IP가 설정되어 있지 않으므로 삭제"));
		bRet = TRUE;
	}
	else if (nFirstConnectErr == 0 && sResp.GetLength() > 0)
	{
		if (MultiLanguageWriteFile(sInternalIPListFilePath, sResp, CHARSET_UTF8))
		{
			WRITELOG(_T("[RequestInternalList] Success!! data - %s"), sResp);
			bRet = TRUE;
		}
		else
		{
			WRITELOG(_T("[RequestInternalList] Write Fail - 0x%x"), GetLastError());
		}
	}
	else
	{
		WRITELOG(_T("[RequestInternalList] Connection Faiil - %d, %s"), nFirstConnectErr, sResp);
	}

	return bRet;

}

void CClientManager::RequestPoliciesHardwareDevice()
{
	WRITELOG(_T("[CClientManager::RequestPoliciesHardwareDevice] 시작 %d"), m_bFirstInstall);


	CString sReceived = _T("");
	DWORD dwResult = 0;

	if (m_bFirstInstall == TRUE)
	{
		sReceived = _T("{\"device\":[]}");
	}
	else
	{
		dwResult = SendAndReceiveHardwareDevice(_T("18700"), _T(""), sReceived);
		if (dwResult != 0)
		{
			WRITELOG(_T("[CClientManager::RequestPoliciesHardwareDevice] SendAndReceiveHardwareDevice fail! dwResult :%d, sResponse : %s"), dwResult, sReceived);
			return;
		}

		if (sReceived.GetLength() == 0)
		{
			WRITELOG(_T("[CClientManager::RequestPoliciesHardwareDevice] - 정책을 받지 못함"));
			return;
		}

		if (sReceived.Find(_T("block_time")) == -1 && sReceived.Find(_T("block_message")) == -1 && sReceived.Find(_T("device")) == -1)
		{
			WRITELOG(_T("[CClientManager::RequestPoliciesHardwareDevice] - 정상적인 정책이 아니다 sReceived : %s"), sReceived);
			return;
		}
	}

	if (!dwResult)
	{
		g_pBMSCommAgentDlg->m_cBMSConfigurationManager.WriteNewConfigData(BMS_HARDWAREDEVICE_POLICY_FILENAME, sReceived, _T(""));
	}
	WRITELOG(_T("[CClientManager::RequestPoliciesHardwareDevice] 정책 파일 생성, GetLastError(%d)"), GetLastError());
	m_bFirstInstall = FALSE;


	BOOL bWOW64 = FALSE;
	IsWow64Process(GetCurrentProcess(), &bWOW64);

	HWND hWnd = ::FindWindow(_T("#32770"), bWOW64==TRUE?BM_APPAGENT_x64_WND_TITLE:BM_APPAGENT_WND_TITLE);
	if (hWnd)
	{
		if (!::PostMessage(hWnd, BM_HARDWAREDEVICE_START_SCAN, NULL, NULL))
		{
			WRITELOG(_T("[CClientManager::RequestPoliciesHardwareDevice] PostMessage fail %d "), GetLastError());
		}
	}

	WRITELOG(_T("[CClientManager::RequestPoliciesHardwareDevice] 종료"));
}

DWORD CClientManager::SendAndReceiveHardwareDevice(CString sCmd, CString sSendData, CString &sResponseData)
{
	WRITELOG(_T("[CClientManager::SendAndReceiveHardwareDevice] 시작"));
	CString sReq = _T("");
	CString sResp = _T("");
	CString sServerURL = _T("");
	DWORD dwResult = 0;
	CString sPageName = GetServerInfo(m_stPageInfo.m_GSHWDEVICE, sServerURL);
	WRITELOG(_T("[CClientManager::SendAndReceiveHardwareDevice] - m_stPageInfo.m_GSHWDEVICE - %s"), m_stPageInfo.m_GSHWDEVICE);
	sReq.Format(_T("%s%s?cmd=%s&agentid=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSHWDEVICE : sPageName, sCmd, m_sAgentID);
	WRITELOG(_T("[CClientManager::SendAndReceiveHardwareDevice] HardwareDevice Packet : %s"), sSendData);

	CString sRequestHeader = _T("");
	CWinHttpUtil cWinHttpUtil;
	sRequestHeader = cWinHttpUtil.MakeWinHttpRequestHeader(_T("Content-Type"), _T("application/x-www-form-urlencoded"), TRUE);

	CBMSCommunicate Communicate;
	// 	dwResult = Communicate.SendPostDataAndReceive(m_wshttpServerUrl, (sPageName == _T("")) ? m_stPageInfo.m_GSHW : sPageName, m_httpPort, sReq, sSendData, sResponse, sRequestHeader, m_bIsHttps);
	dwResult = SendAndReceiveAddHeader((sServerURL == _T(""))? m_wshttpServerUrl : sServerURL, sReq, sSendData, sResponseData, m_bIsHttps);
	if(dwResult != 0)
	{
		dwResult = SendAndReceiveAddHeader((sServerURL == _T(""))? m_wshttpServerUrl : sServerURL, sReq, sSendData, sResponseData, m_bIsHttps);
	}
	return dwResult;
}


BOOL CClientManager::WriteExdsData(CString sData, EXDS_LIST_TYPE enumType)
{
	BOOL bRet = FALSE;
	WRITELOG(_T("[WriteExdsData] sData : %s"), sData);
	if (sData.IsEmpty() || sData.Find(_T("ERR-")) > -1)
	{
		return bRet;
	}

	CString sFileName = _T(""), sFilePath = _T("");
	switch(enumType)
	{
		case EXDS_REQ_TYPE:
			sFileName = _T("exdsReqInfo");
			break;
		case EXDS_ALLOW_TYPE:
			sFileName = _T("exdsAllowInfo");
			break;
		default:
			break;
	}

	TCHAR szExdsInfoPath[MAX_PATH] = {0, };
	GetAgentLogPath(szExdsInfoPath, MAX_PATH);

	sFilePath.Format(_T("%sexdsData\\%s"), szExdsInfoPath, sFileName);

	CString sExistData = MultiLanguageReadFile(sFilePath, CHARSET_UTF8);
	if (sExistData.Compare(sData) != 0)
	{
		MultiLanguageWriteFile(sFilePath, sData, CHARSET_UTF8);
		bRet = TRUE;
	}

	return bRet;
}

int CClientManager::ExdsList(EXDS_LIST_TYPE enumType)
{
	WRITELOG(_T("[CClientManager::ExdsList] Start enumType : %d"), enumType);
	CString sResp = _T(""), sCmd = _T(""), sServerURL = _T(""), sCmdNumber = _T("");
	CString sPageName = GetServerInfo(m_stPageInfo.m_sEXDS, sServerURL);
	
	WRITELOG(_T("[CClientManager::ExdsList] - m_stPageInfo.m_sEXDS - %s"), m_stPageInfo.m_sEXDS);

	DWORD dwReactParam = 0;

	switch(enumType)
	{
		case EXDS_REQ_TYPE:
			sCmdNumber = _T("18800");
			dwReactParam = TRAY_EXDS_REQUEST_COMPLETE;
			break;
		case EXDS_ALLOW_TYPE:
			sCmdNumber = _T("18803");
			dwReactParam = TRAY_ALLOW_EXDS_LIST_MESSAGE;
			break;
		default:
			break;
	}

	sCmd.Format(_T("%s%s?cmd=%s&agentid=%s"), m_shttpObject, (sPageName == _T("")) ? m_stPageInfo.m_GSALLOW : sPageName, sCmdNumber, m_sAgentID);

	WRITELOG(_T("[CClientManager::ExdsList] sCmd : %s, m_sAgentID : %s"), sCmd, m_sAgentID);
	CStringW wsCmd = sCmd, wsReq = _T("");
	int ret = SendAndReceive((sServerURL == _T("")) ? m_wshttpServerUrl : sServerURL, wsCmd, wsReq, sResp, m_bIsHttps);
	if (ret == 0 && sResp.Find(_T("[")) > -1 && sResp.Find(_T("]")) > -1)
	{
		if (WriteExdsData(sResp, enumType) == TRUE)
		{
			SendReactTrayRefresh(dwReactParam);
		}
	}

	return ret;
}

CString CClientManager::GetExternalData()
{
	CString sRet = _T("");
	WRITELOG(_T("[CClientManager::GetExternalData] Start"));
	CString sData = g_pBMSCommAgentDlg->m_cBMSConfigurationManager.GetConfigValue(BMS_MANAGELIST_FILENAME, _T("externaldata_mapping"), BMS_MNG_CUSTOMLIST);
	if (sData.IsEmpty() == FALSE)
	{
		CString sType = _T(""), sPath = _T("");;
		AfxExtractSubString(sType, sData, 0, _T('^'));
		AfxExtractSubString(sPath, sData, 1, _T('^'));

		if (sType.CompareNoCase(_T("file")) == 0)
		{
			sRet = MultiLanguageReadFile(sPath, CHARSET_UTF8);
		}

		if (sType.CompareNoCase(_T("reg")) == 0)
		{
			int nSData = sPath.ReverseFind(_T('\\')) + 1;
			CString sValueName = sPath.Mid(nSData);
			sPath = sPath.Left(nSData);

			HKEY hKeyRoot = HKEY_LOCAL_MACHINE;
			if (sPath.Find(_T("HKLM")) == 0 || sPath.Find(_T("HKEY_LOCAL_MACHINE")) == 0)
			{
				hKeyRoot = HKEY_LOCAL_MACHINE;
				sPath.Replace(_T("HKLM\\"), _T(""));
				sPath.Replace(_T("HKEY_LOCAL_MACHINE\\"), _T(""));
			}

			if (sPath.Find(_T("HKCU")) == 0 || sPath.Find(_T("HKEY_CURRENT_USER")) == 0)
			{
				sPath.Replace(_T("HKCU\\"), _T(""));
				sPath.Replace(_T("HKEY_CURRENT_USER\\"), _T(""));
				CString sTemp = sPath, sSID = _T("");

				hKeyRoot = HKEY_CURRENT_USER;
				sSID = GetCurrentUserSID();
				if (!sSID.IsEmpty())
				{
					hKeyRoot = HKEY_USERS;
					sPath.Format(_T("%s\\%s"), sSID, sTemp);	
				}
			}

			TCHAR lpBuf[MAX_PATH] = {0, };
			ZeroMemory(lpBuf, MAX_PATH*sizeof(TCHAR));
			RegGetStringValue(hKeyRoot, sPath, sValueName, lpBuf, MAX_PATH*sizeof(TCHAR));
			sRet.Format(_T("%s"), lpBuf);
		}

	}

	if (sRet.IsEmpty())
	{
		sRet = _T("{null}");
	}

	WRITELOG(_T("[CClientManager::GetExternalData] Ret : %s"), sRet);

	return sRet;
}

CString CClientManager::ChangeStatusText(CString sState)
{
	CString sRet = _T("");
	if (sState.CompareNoCase(_T("U")) == 0)
	{
		sRet = LoadLanString(216);
	}
	else if (sState.CompareNoCase(_T("A")) == 0)
	{
		sRet = LoadLanString(217);
	}
	else if (sState.CompareNoCase(_T("D")) == 0)
	{
		sRet = LoadLanString(218);
	}

	return sRet;

}

DWORD CClientManager::GetCSOwningThreadID()
{
	DWORD dwOwningThreadID = (DWORD)m_RequestPoliciesCS.OwningThread;
	return dwOwningThreadID;
}

void CClientManager::LeaveCriticalSectionRequestPoliciesCS()
{
	LeaveCriticalSection(&m_RequestPoliciesCS);
}
