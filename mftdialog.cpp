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

cNTFS *o = new cNTFS;
extern cPhysical *oPhy;

char out[50000];

#define ADD strcat(out,buf);
void DoLook(HWND hDlg, int iNum) {
	char buf[10000];
	char tmp[10000];
	int iRet;
	struct ntfs_mft_header *mhead = (struct ntfs_mft_header *)o->aMFT;
	struct ntfs_attribute *attr = NULL;
	struct ntfs_attr_file_name *fn;
	struct ntfs_attr_index_root *ir = NULL;
	struct ntfs_attr_index_root *ia = NULL;
	struct ntfs_attribute *attrir = NULL;
	struct ntfs_attribute *attria = NULL;
	qword *vcn;

	map<long, int> mapFiles;
	map<long, int>::iterator itr;
	struct ntfs_attr_index_entry *ind;
	int i, isan;
	int iFlags;

	out[0] = 0;
	if ((iRet = o->ReadMFT(iNum)) != NTFS_OK) {
		sprintf(out, "Error %i reading MFT", iRet);
		goto done;
	}

	sprintf(buf, "MFT Flags: %s %s\r\n", (mhead->flags & 1)?"INUSE":"", (mhead->flags&2)?"DIR":"");
	ADD


	while ( (iRet = o->GetNextAttribute(0, &attr)) == NTFS_OK ) {
		sprintf(buf, "\r\n--- ATTRIBUTE\r\n"); ADD

		sprintf(buf, "  %s - %s\r\n", o->AttributeName(attr->type), attr->resident?"NonResident":"Resident");
		ADD
		if (attr->namelen != 0) {
			char *pName =  (char*)attr + attr->name_ofs;
			wstrncpy(tmp, pName, 500);
			sprintf(buf, "Name: %.*s\r\n", attr->namelen, tmp);
			ADD
		}
		if (attr->type == NTATTR_FILE_NAME) {
			if (attr->resident != 0) {
				sprintf(buf, "ERROR nonresident filename\r\n");
				ADD
			} else {
				fn = (struct ntfs_attr_file_name*)(((char*)attr) + attr->res.attr_ofs);
				wstrncpy(tmp, (char*)&fn->name, 500);
				tmp[fn->len] = 0;
				sprintf(buf, "File Name: %s   Size: %llu\r\n", tmp, fn->realsize);
				ADD
			}
			sprintf(buf, "Parent MFT: %llu\r\n", fn->parent & 0x00FFFFFFFFFFFFFF);
			ADD
		}
		if (attr->type == NTATTR_INDEX_ROOT) {
			ir = (struct ntfs_attr_index_root *)(((char*)attr) + attr->res.attr_ofs);
			sprintf(buf, "ROOT: Type %lXh  Collation %lu  Size of Index Alloc in bytes: %lu  Clusters per Index Record: %i\r\n",
					ir->type, ir->collation, ir->index_size, ir->clustersperindex & 0xFF);
			ADD
			sprintf(buf, "HEADER: indx_off: %lu   indx_len: %lu   alloc_len: %lu   flags: %s\r\n",
				ir->ih.indx_ofs, ir->ih.indx_len, ir->ih.alloc_len, ((ir->ih.flags & 1)?"IN ALLOC":"IN ROOT"));
			ADD
/*			sprintf(buf, "INDEX_ROOT DUMP: ");
			ADD
			for (i = 0; i < sizeof(ntfs_attr_index_root) - sizeof(ntfs_attr_index_header); ++i) {
				sprintf(buf, "%2.2hhX ", ((char*)ir)[i] & 0xFF);
				ADD
			}
			sprintf(buf, "\r\nINDEX_HEADER DUMP (%i) : ", sizeof(ntfs_attr_index_header));
			ADD
			for (i = 0; i < sizeof(ntfs_attr_index_header); ++i) {
				sprintf(buf, "%2.2hhX ", ((char*)(&ir->ih))[i] & 0xFF);
				ADD
			}
			sprintf(buf, "\r\nENTRY DUMP: ");
			ADD
			char *p;
			p = (char*)(&ir->ih) + sizeof(ntfs_attr_index_header);
			for (i = 0; i < 500; ++i) {
				sprintf(buf, "%2.2hhX ", p[i]  & 0xFF);
				ADD
			}
*/


			ind = (struct ntfs_attr_index_entry *)(((char*)(&ir->ih) + ir->ih.indx_ofs));
//			for (i = 0; i < 1000; ++i) {
//				sprintf(buf, "%2.2hhX ", ((char*)ind)[i]  & 0xFF);
//				ADD
//			}
			isan = 0;
//			while (BOUNDS(ind, o->aMFT, sizeof(o->aMFT)) && !(ind->flags & 2) && (isan++ < 10)) {
			while (BOUNDS(ind, o->aMFT, sizeof(o->aMFT)) && (isan++ < 10)) {
		char c;
		sprintf(buf, "fileref: %8.8lu  indlen: %2i  keylen: %2i  flags: %hhXh\r\n", 
			(long)ind->fileref, ind->index_len, ind->key_len, ind->flags);
		strcat(out, buf);
		for (i = 0; i < ind->key_len; ++i) {
			sprintf(buf, "%2.2hhX ", (&ind->stream)[i] & 0xFF);
			strcat(out, buf);
			if (i > 5 && !( (i+1) % 32 )) { strcat(out, "\r\n"); }
		}
		strcat(out, "\r\n---\r\n");
		for (i = 0; i < ind->key_len; ++i) {
			c = (&ind->stream)[i] & 0xFF;
			if (c < 32 || c > 127) c = ' ';
			sprintf(buf, "%c ", c);
			strcat(out, buf);
			if (i > 5 && !( (i+1) % 96 )) { strcat(out, "\r\n"); }
		}
		if ( (ind->index_len - ind->key_len) != 16 ) {
			sprintf(buf, "\r\n--- Trailing %i ---\r\n", ind->index_len - ind->key_len - 16); ADD
			for (ind->key_len; i < ind->index_len - ind->key_len - 16; ++i) {
				sprintf(buf, "%2.2hhX ", (&ind->stream)[i] & 0xFF);
				strcat(out, buf);
				if (i > 5 && !( (i+1) % 32 )) { strcat(out, "\r\n"); }
			}
		}
		strcat(out, "\r\n\r\n");

				if (ind->flags & 2) break; // break the while
				

				
				itr = mapFiles.find((long)ind->fileref);
				if (itr != mapFiles.end()) { iFlags = 0; }  // Already in our map
				else if ( (iRet = o->GetMFTFilename((long)ind->fileref, tmp, &iFlags)) != NTFS_OK) {
					sprintf(buf, "   ERROR %i fetching MTF %i\r\n", iRet, (long)ind->fileref);
					ADD
				} else {
					sprintf(buf, "   %8.8lu [%s] %s %s\r\n", (long)ind->fileref,
						(iFlags & 2)?"DIR ":"FILE", tmp, (iFlags & 1)?" ":"DELETED");
					ADD
					mapFiles[(long)ind->fileref] = iFlags;
				}
				if (ind->flags & 1) {
					// The VCN to an INDX is the last 8 bytes of the entry.
					vcn = (qword *)(((char*)ind) + ind->index_len - 8);
					sprintf(buf, "Leaf Node VCN: %lu\r\n", *vcn);
					ADD
/*
					for (i = 0; i < ind->index_len; ++i) {
						sprintf(buf, "%2.2hhX ", ((char*)ind)[i] & 0xFF);
						ADD
					}
*/
					sprintf(buf, "\r\n");
					ADD
				}

				ind = (struct ntfs_attr_index_entry *)((char*)ind + ind->index_len);
			} // while

			sprintf(buf, "Last Entry Dump Size: %i\r\n", ind->index_len); ADD

			for (i = 0; i < ind->index_len; ++i) {
				sprintf(buf, "%2.2hhX ", ((char*)ind)[i] & 0xFF);
				ADD
			}

			if (ind->flags & 1) {
				char *pv = ( ((char*)ind) + (ind->index_len) - 8L);
				vcn = (qword*) ( ((char*)ind) + 16L);
				sprintf(buf, "END RECORD Leaf Node VCN: %8.8Xh  %8.8Xh\r\n", ((unsigned long)(*vcn)), (unsigned long)(*pv)); 
				ADD
				sprintf(buf, "ind %lu  len %i  vcn %lu\r\n", ind, ind->index_len, vcn);
				ADD
				for (i = 0; i < 8; ++i) {
					sprintf(buf, "%2.2hhX ", ((char*)vcn)[i] & 0xFF);
					ADD
				}

					sprintf(buf, "\r\n"); ADD
			}

		} // if (attr->type == NTATTR_INDEX_ROOT)
		if (attr->type == NTATTR_INDEX_ALLOC) {
			struct ntfs_runlist aRunList[21];
			char aIndex[20001];
			char *pRunData = (char*)(((char*)attr) + attr->non.data_ofs);
			memset(aIndex, 0, 20000);
			sprintf(buf, "RunList: ");
			ADD
			for (i = 0; i < 20; ++i) {
				sprintf(buf, "%2.2hhX ", pRunData[i]  & 0xFF);
				ADD
			}
			sprintf(buf, "\r\n");
			ADD
			iRet = o->DecodeRunlist(pRunData, 1024, aRunList, 20);
			if (iRet < 0) {
				sprintf(buf, "ERROR %i decoding run lists\r\n", iRet);
				ADD
			} else {
				sprintf(buf, "Index Runs\r\n");
				ADD
				for (i = 0; i < 20 && aRunList[i].len != -1; ++i) {
					sprintf(buf, "%lu @ %lu\r\n", aRunList[i].len, (long)(aRunList[i].lcn));	
					ADD
				}
				// In order to decode the index block we need a handle to $INDEX_ROOT
				attria = attr;   // the current attribute is ALLOC
				if ( (iRet = o->GetAttribute(NTATTR_INDEX_ROOT, &attrir)) != NTFS_OK) {
					sprintf(buf, "Error %i trying to get handle to $INDEX_ROOT. Cannot decode $INDEX_ALLOC.\r\n");
					ADD
					continue;
				} 
				if ( (iRet = o->DecodeIndexAlloc(attrir, attria, 2000, mapFiles)) != NTFS_OK) {
					sprintf(buf, "Error %i trying to DecodeIndexAlloc. \r\n", iRet);
					ADD
					continue;
				}
				//return;
				itr = mapFiles.begin();
				isan = 0;
				sprintf(buf, "Size of mapfiles: %i\r\n", mapFiles.size());
				ADD
				for (isan = 0, itr = mapFiles.begin(); isan < 500 && itr != mapFiles.end(); ++isan, ++itr) {
					if ( (iRet = o->GetMFTFilename((*itr).first, tmp, &iFlags)) != NTFS_OK) {
						sprintf(buf, "Error %i getting MFT# %lu filename.\r\n", iRet, (*itr).first);
						ADD
					}
					sprintf(buf, "   %8.8lu [%s] %s %s\r\n", (*itr).first,
						(iFlags & 2)?"DIR ":"FILE", tmp, (iFlags & 1)?" ":"DELETED");
					ADD
				}


			}

		} // if (attr->type == NTATTR_INDEX_ALLOC)
	}



done:
	SetDlgItemText(hDlg, IDC_TEXT, out);
	isan = 0;
}

void DoFindIndx(HWND hDlg) {
	vector<int> vecIndex;
	vector<int>::iterator itr;
	int iRet;

	bool bCancel = false;
	char buf[1000];
	char out[30000];

	out[0] = buf[0] = 0;
	
	SetDlgItemText(hDlg, IDC_TEXT, "Searching for INDX blocks....");
	if ( (iRet = o->FindAllIndexBlocks(vecIndex, true, &bCancel)) != NTFS_OK) {
		sprintf(buf, "Error %i while scanning.\r\n", buf);
		ADD
	}
	
	for (itr = vecIndex.begin(); itr != vecIndex.end(); ++itr) {
		sprintf(buf, "%8.8lu\r\n", *itr);
		ADD
	}

	SetDlgItemText(hDlg, IDC_TEXT, out);

}


void DoReadCluster(HWND hDlg, int iNum) {
	char out[30000];
	char buf[10000];
	char clu[30000];
	int iRet;
	int i;
	char c;

	out[0] = buf[0] = clu[0] = 0;

	if ( (iRet = o->ReadClusters(clu, iNum, 1)) != NTFS_OK) {
		sprintf(buf, "Error %i reading cluster %i\r\n", iRet, iNum);
		ADD
		SetDlgItemText(hDlg, IDC_TEXT, out);
		return;
	}

	for (i = 0; i < (o->ulBytesPerSector * o->ulSectorsPerCluster); ++i) {
		c = clu[i] & 0xFF;
		if (c >= 32 && c <= 126) sprintf(buf, "%c", c);
		else sprintf(buf, " ");
		ADD
		if (i > 5 && !((i+1) % 96)) strcat(out, " |\r\n");
	}

	strcat(out, "\r\n\r\n-------------------------------------------------\r\n\r\n");

	for (i = 0; i < (o->ulBytesPerSector * o->ulSectorsPerCluster); ++i) {
		sprintf(buf, "%2.2X ", clu[i] & 0xFF); ADD
		if (i > 5 && !((i+1) % 16)) strcat(out, "\r\n");
	}
	
	SetDlgItemText(hDlg, IDC_TEXT, out);
}

	// Mesage handler for about box.
LRESULT CALLBACK MFTLookup(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	int iNum;
	int bSuc;

	switch (message)
	{
		case WM_INITDIALOG:
			hMFTDlg = hDlg;
			if (!o->OpenDevice("\\\\.\\PHYSICALDRIVE1")) {
				MessageBox(0, "Couldn't open drive", "Error", MB_OK);
				return true;
			}
			o->SetPartition(oPhy->drive[1].par[0].offset,
						oPhy->drive[1].par[0].length);
			if (!o->ParseMFTBoot()) {
				MessageBox(0, "Couldn't parse boot", "Error", MB_OK);
				return true;
			}
			
				return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:	
					o->CloseDevice();
					EndDialog(hDlg, LOWORD(wParam));
					return TRUE;
				case IDC_LOOK:
					iNum = GetDlgItemInt(hDlg, IDC_NUM, &bSuc, false);
					DoLook(hDlg, iNum);
					return TRUE;
				case IDC_FIND_INDX:
					DoFindIndx(hDlg);
					return TRUE;
				case IDC_READCLUSTER:
					iNum = GetDlgItemInt(hDlg, IDC_CLUSTER, &bSuc, false);
					DoReadCluster(hDlg, iNum);
					return TRUE;
				default: return false;
			}

	}
    return FALSE;
}
