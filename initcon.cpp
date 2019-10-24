#include "stdafx.h"
#include "resource.h"

#include <windows.h>

#include <commctrl.h>
#include <shlwapi.h>

#include "global.h"


#define ADDBMPTOIMGLIST(name) { \
	hBmp = LoadBitmap(gInst, MAKEINTRESOURCE(name)); \
	ImageList_AddMasked(hImageList, hBmp, clrTrans); \
	DeleteObject(hBmp); }
	




bool InitToolbar(HWND hWnd) {
	int ip = 0, ap = 0;
	HIMAGELIST hImageList;
	HBITMAP hBmp;
	COLORREF clrTrans = RGB(212,208,200);	
	const short num_buttons = 3;
    TBBUTTON t_but[num_buttons] = {0};


	hImageList = ImageList_Create(20, 20, ILC_COLOR24 | ILC_MASK, 0, 4);

	if (hImageList == NULL) {
		DisplayErrorString(ERS_CONTROL, "InitToolbar:hImageList");
		return false;
	}

	// Load in the order they will appear on the bar
	ADDBMPTOIMGLIST(IDB_RED);
	ADDBMPTOIMGLIST(IDB_GREEN);
	ADDBMPTOIMGLIST(IDB_BLUE);

	hToolbar = CreateWindow(TOOLBARCLASSNAME, "Tbar", 
				WS_CHILD | WS_VISIBLE | CCS_NORESIZE | TBSTYLE_TOOLTIPS  ,
				0, 0, 0, 0, 
				hWnd, (HMENU)IDC_TOOLBAR, gInst, NULL);

	if (hToolbar == NULL) { DisplayErrorString(ERS_CONTROL, "Toolbar:hToolBar"); return false; }

	SendMessage(hToolbar, TB_SETIMAGELIST, 0,
		reinterpret_cast<LPARAM>(hImageList));


	t_but[ap].iBitmap   = ip++;
	t_but[ap].idCommand = IDC_TOOLBUTTON;
	t_but[ap].fsState   = TBSTATE_ENABLED;
	t_but[ap++].fsStyle = TBSTYLE_AUTOSIZE | TBSTYLE_BUTTON;

	t_but[ap].iBitmap   = ip++;
	t_but[ap].idCommand = IDC_TOOLBUTTON;
	t_but[ap].fsState   = TBSTATE_ENABLED;
	t_but[ap++].fsStyle = TBSTYLE_AUTOSIZE | TBSTYLE_BUTTON;

	t_but[ap].iBitmap   = ip++;
	t_but[ap].idCommand = IDC_TOOLBUTTON;
	t_but[ap].fsState   = TBSTATE_ENABLED;
	t_but[ap++].fsStyle = TBSTYLE_AUTOSIZE | TBSTYLE_BUTTON;


	SendMessage(hToolbar, TB_ADDBUTTONS, num_buttons, 
		reinterpret_cast<LPARAM>(t_but));

	
	SetWindowPos(hToolbar, NULL, 0, 0, 500, 34, SWP_FRAMECHANGED); 

	return true;
}



bool InitControls(HWND hWnd) {
	LVCOLUMN lvc; 

	if (!InitToolbar(hWnd)) return false;


	hSTPhysical = CreateWindow(WC_STATIC, "Physical Drive", 
		WS_CHILD | WS_VISIBLE | SS_CENTER | WS_BORDER,
		0, 0, 0, 0, hWnd, (HMENU)IDC_STPHYSICAL, gInst, NULL);
	hCBPhysical = CreateWindow(WC_COMBOBOX, "CBPhysical", 
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
		0, 0, 0, 0, hWnd, (HMENU)IDC_CBPHYSICAL, gInst, NULL);
	if (hCBPhysical == NULL) { DisplayErrorString(ERS_CONTROL, "Toolbar:hCBPhysical"); return false; }

	hPar = CreateWindow(WC_LISTVIEW, "", WS_BORDER | LVS_SINGLESEL | 
				LBS_NOTIFY | WS_CHILD | LVS_REPORT | WS_VISIBLE | LVS_SHOWSELALWAYS,
				350, 50, 300, 100, hWnd, (HMENU)IDC_PAR, gInst, NULL);
	ListView_SetExtendedListViewStyle(hPar, LVS_EX_FULLROWSELECT);

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
	lvc.fmt = LVCFMT_LEFT; 
	
	lvc.iSubItem = 0;	lvc.cx = 30;
	lvc.pszText = "#";
	ListView_InsertColumn(hPar, lvc.iSubItem, &lvc);

	lvc.iSubItem = 1;	lvc.cx = 100;
	lvc.pszText = "Type";
	ListView_InsertColumn(hPar, lvc.iSubItem, &lvc);
	
	lvc.iSubItem = 2;	lvc.cx = 100;
	lvc.pszText = "Size";
	ListView_InsertColumn(hPar, lvc.iSubItem, &lvc);



	hTVDir = CreateWindow(WC_TREEVIEW, "TVDir", 
		WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | 
		TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
		0, 0, 0, 0, hWnd, (HMENU)IDC_TVDIR, gInst, NULL);

	hLVFile = CreateWindow(WC_LISTVIEW, "LVFile", WS_BORDER | LVS_SINGLESEL | 
				LBS_NOTIFY | WS_CHILD | LVS_REPORT | WS_VISIBLE | LVS_SHOWSELALWAYS,
				0, 0, 0, 0, hWnd, (HMENU)IDC_LVFILE, gInst, NULL);
	ListView_SetExtendedListViewStyle(hLVFile, LVS_EX_FULLROWSELECT);

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
	lvc.fmt = LVCFMT_LEFT; 
	

	lvc.iSubItem = 0;	lvc.cx = 100;
	lvc.pszText = "#";
	ListView_InsertColumn(hLVFile, lvc.iSubItem, &lvc);

	lvc.iSubItem = 1;	lvc.cx = 200;
	lvc.pszText = "File";
	ListView_InsertColumn(hLVFile, lvc.iSubItem, &lvc);

	lvc.iSubItem = 2;	lvc.cx = 100;
	lvc.pszText = "Size";
	ListView_InsertColumn(hLVFile, lvc.iSubItem, &lvc);

	lvc.iSubItem = 3;	lvc.cx = 100;
	lvc.pszText = "Date";
	ListView_InsertColumn(hLVFile, lvc.iSubItem, &lvc);


	SetWindowPos(hSTPhysical, NULL,  20, 50, 100, 20, SWP_FRAMECHANGED);
	SetWindowPos(hCBPhysical, NULL, 140, 50, 200, 200, SWP_FRAMECHANGED);

	

	return true;
}