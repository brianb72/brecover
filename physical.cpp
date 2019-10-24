#include "stdafx.h"
#include "..\resource.h"

#include <commctrl.h>
#include <wbemidl.h>
#include <objbase.h>
#include <wbemcli.h>
#include <comutil.h>  // _b_str_

#include <stdio.h>

#include "..\global.h"
#include "physical.h"

struct par_types_data {
   unsigned char index;
   char *name;
};

struct par_types_data par_types[] = {
	{0x00, "Empty"},
	{0x01, "DOS 12-bit FAT"},
	{0x02, "XENIX root"},
	{0x03, "XENIX usr"},
	{0x04, "DOS 16-bit <32M"},
	{0x05, "Extended"},
	{0x06, "DOS 16-bit >=32M"},
	{0x07, "HPFS/NTFS"},		/* or QNX? */
	{0x08, "AIX"},
	{0x09, "AIX bootable"},
	{0x0a, "OS/2 Boot Manager"},
	{0x0b, "Win95 FAT32"},
	{0x0c, "Win95 FAT32 (LBA)"},
	{0x0e, "Win95 FAT16 (LBA)"},
	{0x0f, "Win95 Extended (LBA)"},
	{0x11, "Hidden DOS FAT12"},
	{0x14, "Hidden DOS FAT16"},
	{0x16, "Hidden DOS FAT16 (big)"},
	{0x17, "Hidden OS/2 HP/NTFS"},
	{0x40, "Venix 80286"},
	{0x51, "Novell?"},
	{0x52, "Microport"},		/* or CPM? */
	{0x63, "GNU HURD"},		/* or System V/386? */
	{0x64, "Novell Netware 286"},
	{0x65, "Novell Netware 386"},
	{0x75, "PC/IX"},
	{0x80, "Old MINIX"},		/* Minix 1.4a and earlier */

	{0x81, "Linux/MINIX"}, /* Minix 1.4b and later */
	{0x82, "Linux swap"},
	{0x83, "Linux native"},
	{0x85, "Linux extended"},

	{0x86, "NTFS volume set"},
	{0x87, "NTFS volume set"},


	{0x93, "Amoeba"},
	{0x94, "Amoeba BBT"},		/* (bad block table) */
	{0xa5, "BSD/386"},
	{0xa6, "OpenBSD"},
	{0xa7, "NEXTSTEP"},
	{0xb7, "BSDI fs"},
	{0xb8, "BSDI swap"},
	{0xc7, "Syrinx"},
	{0xdb, "CP/M"},			/* or Concurrent DOS? */
	{0xe1, "DOS access"},
	{0xe3, "DOS R/O"},
	{0xf2, "DOS secondary"},
	{0xff, "BBT"},			/* (bad track table) */
	{ 0, NULL }
};


cPhysical::cPhysical() {
	memset(&drive, 0, sizeof(struct phydrive_data) * NUM_PHYDRIVE);
}

cPhysical::~cPhysical() {
	
}

bool cPhysical::Init() {
	int iDx;
	char szDevice[100];
	HANDLE hDev;
	

	for (iDx = 0; iDx < NUM_PHYDRIVE; ++iDx) {
		sprintf(szDevice, "\\\\.\\PHYSICALDRIVE%i", iDx);
		hDev = CreateFile(szDevice, GENERIC_READ, FILE_SHARE_READ,
					NULL, OPEN_EXISTING, NULL, NULL);
		if (hDev == INVALID_HANDLE_VALUE) break;
		drive[iDx].bExist = true;
		CloseHandle(hDev);
	}

	return true;
}


#include <atlbase.h>

void ReportErrorInfo(IUnknown * punk){
	CComBSTR bstrError;
	CComPtr<ISupportErrorInfo> pSEI;
	HRESULT hr = punk->QueryInterface(IID_ISupportErrorInfo,(void **) &pSEI);
	if(SUCCEEDED(hr)){
		CComPtr<IErrorInfo> pEO;
		if(S_OK == GetErrorInfo(NULL, &pEO)){
			char buf[1000];
			CComBSTR bstrDesc;
			pEO->GetDescription(&bstrDesc);
			sprintf(buf, "%S", bstrDesc);
			MessageBox(0, buf, "COM Error", MB_OK);
		}
	}
}

/* Open the specified drive and read its MBR */
bool cPhysical::ReadMBR(int iDrive) {
	HANDLE hDev;
	unsigned long iBytes;
	char bufmbr[512+10];
	char buferr[1001];
	
	struct mbr_data *mbr = (struct mbr_data *)bufmbr;
	int i;

	if (!drive[iDrive].bExist) {
		MessageBox(0, "cPhysical::ReadMBR Nonexistant Drive", "Error", MB_OK);
		return false;
	}
	hDev = CreateFile(drive[iDrive].deviceid, GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_EXISTING, NULL, NULL);
	if (hDev == INVALID_HANDLE_VALUE) {
		sprintf(buferr, "Error Opening %s", drive[iDrive].deviceid);
		MessageBox(0, buferr, "cPhysical::ReadMBR", MB_OK);
		return false;
	}

	SetFilePointer(hDev, 0, NULL, FILE_BEGIN);
	if (!ReadFile(hDev, bufmbr, 512L, &iBytes, NULL)) {
		sprintf(buferr, "Error Reading MBR %s", drive[iDrive].deviceid);
		MessageBox(0, buferr, "cPhysical::ReadMBR", MB_OK);
		CloseHandle(hDev);
		return false;
	}
	CloseHandle(hDev);

	drive[iDrive].bPar = true;
	if (mbr->signature != (short)0xAA55) {	// Copy it anyway
		sprintf(buferr, "Warning %s : MBR signature wrong! %4.4hXh", drive[iDrive].deviceid,
			mbr->signature);
		MessageBox(0, buferr, "cPhysical::ReadMBR", MB_OK);
	}
	for (i = 0; i < 4; ++i) {
		memcpy(&drive[iDrive].par[i], &mbr->par[i], sizeof(struct mbr_partition));
	}
	return true;
}


bool cPhysical::InitWMI() {
	IEnumWbemClassObject * pEnum = NULL;
	IWbemClassObject * pClass = NULL;
	HRESULT hres, hresclass;
	int iDx = 0;
	ULONG uReturned;
	VARIANT v;
	_bstr_t bstr;
	ULONG uCount = 1;


	BSTR strQuery = (L"Select * from Win32_diskdrive");
	BSTR strQL = (L"WQL");
	hres = pSvc->ExecQuery(strQL, strQuery,
		WBEM_FLAG_RETURN_IMMEDIATELY,NULL,&pEnum);

	if (FAILED(hres)) {
		DisplayLastError("pSvc->ExeQuery");
		return false;
	}
	
	pEnum->Reset();

	
	hres = WBEM_S_NO_ERROR;

	while ( (hres == WBEM_S_NO_ERROR) && (iDx < NUM_PHYDRIVE) ) {
		hres = pEnum->Next(WBEM_INFINITE, uCount, &pClass, &uReturned);
		if (hres == WBEM_S_NO_ERROR) {
			drive[iDx].bExist = true;
			hresclass = pClass->Get(_bstr_t(L"Model"), 0, &v, 0, 0);
			if (FAILED(hresclass)) {
				MessageBox(0, "pClass->Get() failed", "Error", MB_OK);
				VariantClear( &v );
				break;
			}
			bstr = &v;
			strncpy(drive[iDx].model, (char*)bstr, 100);
			hresclass = pClass->Get(_bstr_t(L"DeviceID"), 0, &v, 0, 0);
			if (FAILED(hresclass)) {
				MessageBox(0, "pClass->Get() failed", "Error", MB_OK);
				VariantClear( &v );
				break;
			}
			bstr = &v;
			strncpy(drive[iDx].deviceid, (char*)bstr, 200);
			VariantClear( &v );
			if (!ReadMBR(iDx)) break;
		}

		++iDx;
	}

	if (hres != WBEM_S_FALSE) {
		ReportErrorInfo(pEnum);
		MessageBox(0, "cPhysical::Init()", "Error", MB_OK);
		return false;
	}
	if (pEnum != NULL) pEnum->Release();
	if (pClass != NULL) pClass->Release();
	return true;
}



void cPhysical::DispMBR(int iDrive) {
	LVITEM lvI;
	int i;
	char buf[100];

	ListView_DeleteAllItems(hPar);
	if (!drive[iDrive].bExist || !drive[iDrive].bPar) return;

	lvI.mask = LVIF_TEXT | LVIF_PARAM; 

	for (i = 0; i < 4; ++i) {
		if (drive[iDrive].par[i].type == 0) break;
		lvI.iSubItem = 0;
	    lvI.lParam = i;
		lvI.iItem = i;
		sprintf(buf, "%i", i);
		lvI.pszText = buf;
		ListView_InsertItem(hPar, &lvI);
		sprintf(buf, "%s", GetPartitionName(drive[iDrive].par[i].type));
		ListView_SetItemText(hPar, i, 1, buf);
		sprintf(buf, "%lu mb", (drive[iDrive].par[i].length / 2L) / 1024L);
		ListView_SetItemText(hPar, i, 2, buf);

	}
}



char *cPhysical::GetPartitionName(unsigned char type) {
	int i;

	for (i=0; par_types[i].name; i++)
		if (par_types[i].index == type)
			return par_types[i].name;

	return "Unknown";
}
