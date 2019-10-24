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

extern cPhysical *oPhy;
extern cNTFS *oNTFS;



void SetNTFSFileSystem() {
	char buf[501];
	int iRet;
	MapFileMFT mapFile;
	MapFileMFT::iterator itr;
	LVITEM lvI;
	TVINSERTSTRUCT tv;
	int iFile = 0;
	struct file_data fi;

	if (!oNTFS->OpenDevice(oPhy->drive[giDrv].deviceid)) {
		sprintf(buf, "Failed to open %s", oPhy->drive[giDrv].deviceid);
		MessageBox(0, buf, "SetNTFSFileSystem()", MB_OK);
		return;
	}
	oNTFS->SetPartition(oPhy->drive[giDrv].par[giPar].offset,
						oPhy->drive[giDrv].par[giPar].length);
	if (!oNTFS->ParseMFTBoot()) {
		sprintf(buf, "Error reading MFT from %s", oPhy->drive[giDrv].deviceid);
		MessageBox(0, buf, "SetNTFSFileSystem()", MB_OK);
		oNTFS->CloseDevice();
		return;
	}

	oNTFS->mftCurDir = NTFS_ROOTDIR;
	oNTFS->vecDir.clear();
	if ( (iRet = oNTFS->GetDirectoryFiles(NTFS_ROOTDIR, mapFile)) != NTFS_OK ) {
		sprintf(buf, "Error %i GetDirectoryFiles");
		MessageBox(0, buf, "Error", MB_OK);
		return;
	}

	TreeView_DeleteAllItems(hTVDir);
	ListView_DeleteAllItems(hLVFile);

	lvI.mask = LVIF_TEXT | LVIF_PARAM; 
	lvI.iSubItem = 0;
	
	tv.hParent		= TVI_ROOT;
	tv.hInsertAfter = TVI_SORT;

	for (itr = mapFile.begin(); itr != mapFile.end(); ++itr) {
		if ( (*itr).first < 24 ) continue; // Dont show "." or metafiles
		if ( (iRet = oNTFS->GetMFTFiledata((*itr).first, &fi)) != NTFS_OK) {
			lvI.iItem = iFile; lvI.lParam = (*itr).first;
			sprintf(buf, "%8.8lu", (long)(*itr).first);
			lvI.pszText = buf; ListView_InsertItem(hLVFile, &lvI);
			ListView_SetItemText(hLVFile, iFile, 2, "*Error Reading*");
			++iFile;			// An error occured
		} 
		if (!(fi.mftFlags & 0x01) && !bDel) continue; // no deleted items shown

		if (fi.mftFlags & 0x02) {	// A directory
			tv.item.mask = TVIF_TEXT | TVIF_PARAM;
			tv.item.pszText = fi.szName;
			tv.item.lParam = fi.mft;
			if (!(fi.mftFlags & 0x01)) {		// BOLD if deleted
				tv.item.state = tv.item.stateMask = TVIS_BOLD;
				tv.item.mask |= TVIF_STATE;
			}
			TreeView_InsertItem(hTVDir, &tv);
		} else {					// A file
			lvI.iItem = iFile;
			lvI.lParam = (*itr).first;
			sprintf(buf, "%8.8lu", (long)(*itr).first);
			lvI.pszText = buf;
			ListView_InsertItem(hLVFile, &lvI);
			if (fi.mftFlags & 0x01) { 
				ListView_SetItemText(hLVFile, iFile, 1, fi.szName);
			} else {
				sprintf(buf, "[DEL] %s", fi.szName);
				ListView_SetItemText(hLVFile, iFile, 1, buf);
			}
			NumToComma(fi.size, buf);
			ListView_SetItemText(hLVFile, iFile, 2, buf);
			sprintf(buf, "%lu", fi.timeModified);
			ListView_SetItemText(hLVFile, iFile, 3, buf);
			++iFile;
		}
	}

}


// Change to the directory specified by tvI. lParam is the MFT#
void NTFSChangeDir(TVITEM tvI) {
	int iRet;

	TreeView_EnsureVisible(hTVDir, tvI.hItem);
	TreeView_SelectItem(hTVDir, tvI.hItem);  

	if ( (oNTFS->ReadMFT(tvI.lParam)
}