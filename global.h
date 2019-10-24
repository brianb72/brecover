#ifndef __GLOBAL_H__
#define __GLOBAL_H__





/* PACK RULES
   Put #pragma pack(1) before every block of structures that needs no packing
   Put #pragma pack(DEFPAK)  after the block
*/

#define DEFPAK	8		// Default Pack size
#pragma pack(DEFPAK)

#include <commctrl.h>
#include <wbemidl.h>
#include <objbase.h>
#include <wbemcli.h>
#include <comutil.h>  // _b_str_


// Types
typedef short 		word;
typedef long  		dword;
typedef __int64	    qword;

extern HINSTANCE	gInst;
extern HWND			gWnd;
extern HWND			hToolbar;
extern HWND			hCBPhysical;
extern HWND			hSTPhysical;
extern HWND			hPar;
extern HWND			hTVDir;	
extern HWND			hLVFile;
extern HWND hMFTDlg;

extern int giDrv;
extern int giPar;

extern void DisplayLastError(char *head);
extern void DisplayErrorString(UINT msg, char *location);
extern void NumToComma(int iNum, char *szBuf);
extern bool InitControls(HWND hWnd);
extern IWbemLocator  *pLoc;
extern IWbemServices *pSvc;

extern int wstrncpy(char *dest, const char *src, int len);

/*	ptest: pointer to test   pstruct: pointer to structure   type: type of structure
	Returns true if ptest is within pstruct. */

#define BOUNDS(ptest, pstruct, size) ( \
                   ( ((char*)ptest) >= ((char*)pstruct) ) && \
				   ( ((char*)ptest) <= ( ((char*)pstruct) + (size)) ) \
				   )

extern bool bDel;


// NTFSFUNC.CPP
extern void SetNTFSFileSystem();
extern void NTFSChangeDir(TVITEM tvI);

#endif
