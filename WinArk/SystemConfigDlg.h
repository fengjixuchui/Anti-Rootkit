#pragma once
#include "resource.h"
#include "SystemInformation.h"
#include "DriverHelper.h"

class CSystemConfigDlg :
	public CDialogImpl<CSystemConfigDlg>,
	public CDialogResize<CSystemConfigDlg> {
public:
	enum { IDD = IDD_CONFIG };

	BEGIN_MSG_MAP(CSystemConfigDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY,OnDestroy)
		COMMAND_ID_HANDLER(IDC_SET_CALLBACK,OnSetCallback)
		COMMAND_ID_HANDLER(IDC_REMOVE_CALLBACK,OnRemoveCallback)
		COMMAND_ID_HANDLER(IDC_ENABLE_DBGSYS,OnEnableDbgSys)
		CHAIN_MSG_MAP(CDialogResize<CSystemConfigDlg>)
	END_MSG_MAP()

	BEGIN_DLGRESIZE_MAP(CSystemConfigDlg)
		/*DLGRESIZE_CONTROL(IDC_GROUP_DBGSYS,DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_ENABLE_DBGSYS,DLSZ_MOVE_Y)

		DLGRESIZE_CONTROL(IDC_GROUP_CALLBACK,DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_SET_CALLBACK, DLSZ_MOVE_Y )
		DLGRESIZE_CONTROL(IDC_REMOVE_CALLBACK, DLSZ_MOVE_Y)

		DLGRESIZE_CONTROL(IDC_GROUP_SYSINFO,DLSZ_SIZE_X)*/
	END_DLGRESIZE_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetCallback(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRemoveCallback(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEnableDbgSys(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);


private:
	bool InitDbgSymbols(DbgSysCoreInfo* pInfo);

private:
	CButton m_CheckImageLoad;
	WinSys::BasicSystemInfo m_BasicSysInfo;
	bool m_enableDbgSys = false;
};