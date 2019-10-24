// bRecover.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"

#include <commctrl.h>
#include <wbemidl.h>
#include <objbase.h>
#include <wbemcli.h>
#include <comutil.h>  // _b_str_

#include <stdio.h>

#include "global.h"
#include "physical/physical.h"
#include "ntfs/cntfs.h"




cNTFS *oNTFS = new cNTFS;

HINSTANCE	gInst;
HWND		gWnd;
HWND		hToolbar;
HWND		hCBPhysical;
HWND		hSTPhysical;
HWND		hPar;			// Partition table
HWND		hTVDir;			// Directory list
HWND		hLVFile;		// File list
		

HWND hMFTDlg;

#define FS_NONE		0
#define FS_NTFS		1

int curFS = FS_NONE;	// Current file system type

int giDrv = 0;			// Currently selected drive
int giPar = 0;			// Currently selected partition

bool bDel = true;		// Should deleted items appear?

HTREEITEM hTVSelected;	// Currently selected item in Treeview

// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	MFTLookup(HWND, UINT, WPARAM, LPARAM);

void SetCurrentFileSystem();

void DoResize();
bool InitCOM();
bool InitWMI();
void COMCleanup();

void ChangeDir(TVITEM tvI);

/* COM HANDLES */
IWbemLocator  *pLoc = NULL;
IWbemServices *pSvc = NULL;

cPhysical *oPhy = new cPhysical;



void Testit()  {

	CoInitialize(NULL);
IWbemLocator * pIWbemLocator = NULL;
IWbemServices * pWbemServices = NULL;
IEnumWbemClassObject * pEnumObject = NULL;
BSTR bstrNamespace = (L"//./root\\cimv2");
HRESULT hRes = CoCreateInstance (
  CLSID_WbemAdministrativeLocator,
  NULL ,
  CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER , 
  IID_IUnknown ,
  ( void ** ) & pIWbemLocator
  ) ;
if (SUCCEEDED(hRes))
{
  hRes = pIWbemLocator->ConnectServer(
  bstrNamespace, // Namespace
  NULL, // Userid
  NULL, // PW
  NULL, // Locale
  0, // flags
  NULL, // Authority
  NULL, // Context
  &pWbemServices
  );
}
BSTR strQuery = (L"Select * from win32_Processor");
BSTR strQL = (L"WQL");
hRes = pWbemServices->ExecQuery(strQL, strQuery,
  WBEM_FLAG_RETURN_IMMEDIATELY,NULL,&pEnumObject);

ULONG uCount = 1, uReturned;
IWbemClassObject * pClassObject = NULL;
hRes = pEnumObject->Reset();
hRes = pEnumObject->Next(WBEM_INFINITE,uCount, &pClassObject, &uReturned);
VARIANT v;
BSTR strClassProp = SysAllocString(L"LoadPercentage");
hRes = pClassObject->Get(strClassProp, 0, &v, 0, 0);
SysFreeString(strClassProp);

_bstr_t bstrPath = &v; //Just to convert BSTR to ANSI
char* strPath=(char*)bstrPath;
if (SUCCEEDED(hRes))
MessageBox(0, strPath, "Msg", MB_OK);
else
MessageBox(0, "Error in getting object", "Error", MB_OK);
VariantClear( &v );
pIWbemLocator->Release();
pWbemServices->Release();
pEnumObject->Release();
pClassObject->Release();
CoUninitialize();

}



int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	InitCommonControls();
	if (!InitCOM()) { COMCleanup(); return false; }
	if (!InitWMI()) { COMCleanup(); return false; }

	// Initialize global strings
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_BRECOVER);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	COMCleanup();

	return msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_BRECOVER);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCSTR)IDC_BRECOVER;
	wcex.lpszClassName	= "bRecover";
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{

   gInst = hInstance; // Store instance handle in our global variable

   gWnd = CreateWindow("bRecover", "bRecover", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!gWnd)
   {
      return FALSE;
   }

   ShowWindow(gWnd, nCmdShow);
   UpdateWindow(gWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	LVITEM lvI;
	int i;

	switch (message) 
	{
		case WM_CREATE:
			if (!InitControls(hWnd)) {
				DestroyWindow(hWnd);	
				break;
			}
			if (!oPhy->InitWMI()) {
				MessageBox(0, "oPhy->Init failure", "Error", MB_OK);
			}
			for (i = 0; i < NUM_PHYDRIVE; ++i) {
				if (!(oPhy->drive[i].bExist))	break;
				SendMessage(hCBPhysical, CB_ADDSTRING, 0, (LPARAM)(oPhy->drive[i].model));
			}
			
			// TODO change back to 0 when done testing
			SendMessage(hCBPhysical, CB_SETCURSEL, 1, 0);
			oPhy->DispMBR(1);
			break;
		case WM_NOTIFY:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			switch (wmId) 
			{
				case IDC_PAR:
					if ( ((NM_LISTVIEW*)lParam)->hdr.code == NM_DBLCLK) {
						if (((NMITEMACTIVATE*)lParam)->iItem == -1) break;
						lvI.mask = LVIF_PARAM;
						lvI.iItem = ((NMITEMACTIVATE*)lParam)->iItem;
						lvI.iSubItem = 0;
						ListView_GetItem(hPar, (LPARAM)&lvI);
						giPar = lvI.lParam;
						SetCurrentFileSystem();
					}
					break;
				case IDC_LVFILE:
					if ( ((NM_LISTVIEW*)lParam)->hdr.code == NM_DBLCLK) {
						if (((NMITEMACTIVATE*)lParam)->iItem == -1) break;
						lvI.mask = LVIF_PARAM;
						lvI.iItem = ((NMITEMACTIVATE*)lParam)->iItem;
						lvI.iSubItem = 0;
						ListView_GetItem(hLVFile, (LPARAM)&lvI);
						//ChangeDir(lvI.lParam);
					}
					break;
				case IDC_TVDIR:
					if ( ((NM_TREEVIEW*)lParam)->hdr.code == NM_DBLCLK) {
						TVITEM tvI;
						HTREEITEM hSel = NULL;
						hSel = TreeView_GetNextItem(hTVDir, hSel, TVGN_CARET);
						if (hSel == NULL || hSel == hTVSelected) break;  // no item or change
						tvI.mask = TVIF_PARAM;
						tvI.hItem = hSel;
						TreeView_GetItem(hTVDir, &tvI);
						hTVSelected = hSel;
						ChangeDir(tvI);					
					}
					break;
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{
				case IDC_CBPHYSICAL:
					if (wmEvent == CBN_SELCHANGE) {
						i = SendMessage(hCBPhysical, CB_GETCURSEL, 0, 0);
						if (i == CB_ERR) {
							giDrv = -1;
							giPar = -1;
							ListView_DeleteAllItems(hPar);
							break;
						}
						giDrv = i;
						giPar = 0;
						oPhy->DispMBR(i);
					}
					break;
				case IDM_ABOUT:
				   DialogBox(gInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
				   break;
				case IDC_LOOKUP:
				   DialogBox(gInst, (LPCTSTR)IDD_MFT, hWnd, (DLGPROC)MFTLookup);
				   break;

				case IDM_EXIT:
				   DestroyWindow(hWnd);
				   break;
				default:
				   return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_SIZE:
			DoResize();
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

// Mesage handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
				return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}


void DoResize() {
	RECT rMain;
	int iWidth; 
	GetClientRect(gWnd, &rMain);
	iWidth = rMain.right - rMain.left;
	SetWindowPos(hToolbar, NULL, 0, 0, rMain.right - rMain.left, 34, SWP_FRAMECHANGED); 

	
	SetWindowPos(hTVDir,  NULL, 20, 170, 
								(iWidth / 3) - 10, 200, SWP_FRAMECHANGED);
	SetWindowPos(hLVFile, NULL, 20 + (iWidth / 3) + 10,  170, 
								((iWidth / 3) * 2) - 20 - 30, 200, SWP_FRAMECHANGED);



}


/* As described in 
   http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wmisdk/wmi/creating_a_wmi_application_using_c_.asp
   Do all of the COM init stuff
*/
bool InitCOM() {
	HRESULT hres;
//	hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	hres = CoInitialize(NULL);

	if (FAILED(hres)) { 
		MessageBox(0, "CoInitializeEx", "Error", MB_OK);
		return false;
	}
	
	hres =  CoInitializeSecurity(
				NULL,                        // Security descriptor
				-1,                          // COM negotiates authentication service
				NULL,                        // Authentication services
				NULL,                        // Reserved
				RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication level for proxies
				RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation level for proxies
				NULL,                        // Authentication info
				EOAC_NONE,                   // Additional capabilities of the client or server
				NULL);                       // Reserved

	if (FAILED(hres)) {
		MessageBox(0, "CoInitializeSecurity", "Error", MB_OK);
		return false;
	}

	return true;

}

/* Setup a connection to the WMI services */
bool InitWMI() {
	HRESULT hres;

/*	hres = CoCreateInstance(CLSID_WbemLocator, 0, 
        CLSCTX_INPROC_SERVER , IID_IWbemLocator, (LPVOID *) &pLoc);
*/
  
 hres = CoCreateInstance (
  CLSID_WbemAdministrativeLocator,
  NULL ,
  CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER , 
  IID_IUnknown ,
  ( void ** ) &pLoc
  ) ;

	if (FAILED(hres)) {
		MessageBox(0, "WbemLocator", "Error", MB_OK);
		return false;
	}

	// Connect to the root\default namespace with the current user.
    hres = pLoc->ConnectServer(
			_bstr_t(L"//./root\\cimv2"), 
//          _bstr_t(L"//./ROOT\\DEFAULT"), 
            NULL, NULL, 0, NULL, 0, 0, &pSvc);
	if (FAILED(hres)) {
		MessageBox(0, "pLoc->ConnectServer", "Error", MB_OK);
		return false;
	}
	// Set the proxy so that impersonation of the client occurs.
/*    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
				NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
				NULL, EOAC_NONE);
	if (FAILED(hres)) {
		MessageBox(0, "CoSetProxyBlanket", "Error", MB_OK);
		return false;
	}
*/
	return true;	
}


void COMCleanup() {
	if (pSvc != NULL) pSvc->Release();
	pSvc = NULL;
    if (pLoc != NULL) pLoc->Release();     
	pLoc = NULL;
	CoUninitialize();
}





/* Set up controls for the current file system and populate the boxes with default info 
   based on giDrv and giPar 
*/
void SetCurrentFileSystem() {
	byte parType;

	if (giDrv > (unsigned)NUM_PHYDRIVE || giPar > (unsigned)4) {
//		MessageBox(0, "SetCurrentFileSystem out of bounds", "Error", MB_OK);
		return;
	}

	parType = oPhy->drive[giDrv].par[giPar].type;

	if (parType == (byte)0x07) {
		curFS = FS_NTFS;
		SetNTFSFileSystem();
		return;
	}

	curFS = FS_NONE;
	MessageBox(0, "Unsupported Filesystem", "Error", MB_OK);
}



void ChangeDir(TVITEM tvI) {

	if (curFS == FS_NTFS) {
		NTFSChangeDir(tvI);
		return;
	}
	MessageBox(0, "No Filesystem Set!", "Error", MB_OK);
	return;
}