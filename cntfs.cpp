#include "stdafx.h"
#include "..\resource.h"

#include "..\global.h"

#include <stdio.h>

#include "cntfs.h"


	// SetFilePointer, ReadFile


cNTFS::cNTFS() {
	hDev = NULL;
	ulParOffset = -1;
	ulParSize = 0;
	ulBytesPerSector = 512L;		// Should always be 512 unless some unusual media
									// like a USB memory stick? TODO - check this out
	mftCurDir = 5;
}

/* Open the specified device with READONLY access */
bool cNTFS::OpenDevice(char *szWhich) {
	if (hDev != NULL) CloseDevice();
	hDev = CreateFile(szWhich, GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_EXISTING, NULL, NULL);
	if (hDev == INVALID_HANDLE_VALUE) return false;
	return true;
}

void cNTFS::CloseDevice() {
	CloseHandle(hDev);
	hDev = NULL;
	ulParOffset = -1;
	ulParSize = 0;
}


/* Sets the partition paramaters to use. All sector reads must occur between
   ulParOffset and ulParOffset+ulParSize */
void cNTFS::SetPartition(unsigned long ulOffset, unsigned long ulSize) {
	ulParOffset = ulOffset;
	ulParSize = ulSize;
}

/* Read the specified sector into paTarget
   NOTE: I _think_ if paTarget is not word or dword aligned, ReadFile will fail
   with "Invalid Param. When defining class member variables use padding if needed
   to ensure they are aligned. */
bool cNTFS::ReadSector(char *paTarget, qword ulSector, int iCnt) {
	unsigned long ulRet;
	long ulLow, ulHigh;
	ulSector *= ulBytesPerSector;

	ulLow  = (unsigned long)(ulSector & 0x00000000FFFFFFFF);
	ulHigh = (unsigned long)(ulSector >> 32);
	ulRet = SetFilePointer(hDev, ulLow, &ulHigh, FILE_BEGIN);
	if (!ReadFile(hDev, paTarget, ulBytesPerSector * iCnt, &ulRet, NULL)) { 
		DisplayLastError("ReadSector ReadFile");
		return false;
	}
	if (ulRet != (ulBytesPerSector * iCnt)) return false;
	return true;
}

bool cNTFS::ReadClusters(char *paTarget, qword ulCluster, int iCnt) {
	return ReadSector(paTarget, (ulCluster * ulSectorsPerCluster) + ulParOffset, 
		iCnt * ulSectorsPerCluster);

}

/* MUST BE CALLED FIRST BEFORE ANY OTHER NTFS FUNCTIONS
   Reads the first sector of the NTFS partition and parses the boot sector.
*/
bool cNTFS::ParseMFTBoot() {
	struct ntfs_bootsector *boot = (struct ntfs_bootsector *)aSector;
	signed long lTmp;

	mftCurDir = 5;

	if (!ReadSector(aSector, ulParOffset)) return false;
	
	if (strncmp((const char*)boot->oem_id, "NTFS    ", 8) != 0) {
		MessageBox(0, "OEM ID Failure", "cNTFS::ParseMFTBoot", MB_OK);
		return false;
	}

	ulSectorsPerCluster = boot->SectorsPerCluster;
	MediaDescriptor = boot->MediaDescriptor;
	lcnMFT = boot->lcnMFT;
	lcnMFTMirr = boot->lcnMFTMirr;
	lcnCurrentMFT = lcnMFT;			// Use the first MFT
	lTmp = boot->ClustersPerMFT;
	/* NOTE - ClustersPerMFT is a dword, but it seems as if only the first byte is used.
	   So F6h is negative, and is 1024 bytes. TODO Make sure this is always correct 
	   Assume that if an MFT is more than 5 clusters, something is wrong. 
	*/
	if ((char)lTmp < (char)0) {	
		ulSectorsPerMFT = (1 << ( (long)(((char)-1) * (char)lTmp)) ) / 512L;
		if ( ulSectorsPerMFT > (ulSectorsPerCluster * 5)) {
			MessageBox(0, "ulSectorsPerMFT > ulSectorsPerCluster", "cNTFS::ParseMFTBoot", MB_OK);
			return false;
		}
	}
	else ulSectorsPerMFT = lTmp * ulSectorsPerCluster;
	/* TODO Verify what normal ClustersPerIndex values are */
	lTmp = boot->ClustersPerIndex;
	if ((char)lTmp < (char)0) {	
		ulSectorsPerIndex = (1 << ( (long)(((char)-1) * (char)lTmp)) ) / 512L;
		if (ulSectorsPerIndex > (ulSectorsPerCluster * 5)) {
			MessageBox(0, "ulSectorsPerIndex > ulSectorsPerCluster", "cNTFS::ParseMFTBoot", MB_OK);
			return false;
		}
	}
	else ulSectorsPerIndex = lTmp * ulSectorsPerCluster;

	return true;
}

/* Read the specified MFT and perform the fixups.
   pBuf specifies the buffer to receive the MFT. If NULL or omitted, use aMFT.
   Buffer must be as large as aMFT.
 */
int cNTFS::ReadMFT(unsigned long mftNum, char *pBuf) {
	unsigned long iDx;
	struct ntfs_mft_header *m;
	word *seqnum;  			   // seq number
	word *seqone, *seqtwo;  // original values in the seq array
	word *endone, *endtwo;  // end of sector, should == seqnum

	if (pBuf == NULL) pBuf = aMFT;
	m = (struct ntfs_mft_header *)pBuf;
	for (iDx = 0; iDx < ulSectorsPerMFT; ++iDx) {
		if (!ReadSector(&pBuf[ulBytesPerSector * iDx], 
						(ulParOffset + (lcnCurrentMFT * ulSectorsPerCluster)) + // Start of MFT
						(mftNum * ulSectorsPerMFT)	+			// MFT# offset
						iDx )) {							// Sector offset
			char buf[100]; // DEBUG
			sprintf(buf, "cNTFS::ReadMFT(%lu)", mftNum);
			DisplayLastError(buf);
			return NTERR_READ;
		}
	}

	if (m->magic != 0x454c4946) return NTERR_MAGIC;
	seqnum = (word*)&pBuf[m->fixup_ofs]; 
	seqone = (word*)&pBuf[m->fixup_ofs + 2];	
	seqtwo = (word*)&pBuf[m->fixup_ofs + 4];	
	endone = (word*)&pBuf[510]; endtwo = (word*)&pBuf[1022];

	if (!BOUNDS(seqnum, pBuf, sizeof(aMFT)) || 
		!BOUNDS(seqone, pBuf, sizeof(aMFT)) || 
		!BOUNDS(seqtwo, pBuf, sizeof(aMFT)) ) {
		return NTERR_FIXUP;	
	}

	if ( (*seqnum != *endone) || (*seqnum != *endtwo) ) return NTERR_FIXUP;
	*endone = *seqone; *endtwo = *seqtwo;

	return NTFS_OK;
}


/* Find the next attribute after *pattr. If NULL, start from the first attribute.
   If iVal == 0 then return the next attribute.
   On success a pointer returned by this function will always be within bounds.
   On failure the pointer must be considered invalid.
*/
int cNTFS::GetNextAttribute(int iVal, struct ntfs_attribute **pattr, char *pBuf) {
	struct ntfs_mft_header *m;
	struct ntfs_attribute *attr;
	byte *a;

	if (pBuf == NULL) pBuf = aMFT;
	m = (struct ntfs_mft_header *)pBuf;

	if (*pattr == NULL) {
		a = (byte *)&pBuf[m->attr_ofs];
	} else {
		a = (byte*)(((char*)*pattr) + (*pattr)->len);
	}

	if (!BOUNDS(a, pBuf, sizeof(aMFT))) return NTERR_BOUNDS;
	while ( (a >= (byte*)pBuf) && (a <= (byte*)pBuf + m->rec_size) ) {
		attr = (struct ntfs_attribute *)a;
		if (attr->type == 0xFFFFFFFF) return NTERR_NOTFOUND;
		if (attr->type == iVal || iVal == 0) {
			*pattr = attr;
			return NTFS_OK;
		}
		a += attr->len;
		if (!BOUNDS(a, pBuf, sizeof(aMFT))) return NTERR_BOUNDS;
	}
	return NTERR_NOTFOUND;
}


/* Find the first iVal attribute from the MFT entry */
int cNTFS::GetAttribute(int iVal, struct ntfs_attribute **pattr, char *pBuf) {
	*pattr = NULL;
	return GetNextAttribute(iVal, pattr, pBuf);
}


/* Start reading the files from the specified directory. Do not update mftCurDir.
   
*/
int cNTFS::StartReadingDirFile(unsigned long mftNum) {
	int iRet;
	struct ntfs_mft_header *m = (struct ntfs_mft_header *)aMFT;
	struct ntfs_attribute *irattr;		// $INDEX_ROOT attribute header
	struct ntfs_attr_index_root *ir;    // Actual data
	char buf[501];
	char *pRunData;						// Pointer to raw run data
	struct ntfs_runlist aRun[11];			// TODO Pick a sane value for this
	struct ntfs_attr_file_name *fn;

	iRet = ReadMFT(199);

	if ( (iRet = GetAttribute(NTATTR_FILE_NAME, &irattr)) != NTFS_OK) {
		sprintf(buf, "Error %i getting $INDEX_ALLOC for MFT# %i", iRet, mftNum);
		MessageBox(0, buf, "cNTFS::StartReadingDirFile", MB_OK);
		return NTERR_ERROR;
	}

	fn = (struct ntfs_attr_file_name*)(((char*)irattr) + irattr->res.attr_ofs);




//	iRet = ReadMFT(mftNum);
	
	
	if (iRet != NTFS_OK) return iRet;

	if (!(m->flags & 0x02)) return NTERR_NOTADIR;
	
	// Read $INDEX_ROOT
	if ( (iRet = GetAttribute(NTATTR_INDEX_ROOT, &irattr)) != NTFS_OK) {
		sprintf(buf, "Error %i getting $INDEX_ROOT for MFT# %i", iRet, mftNum);
		MessageBox(0, buf, "cNTFS::StartReadingDirFile", MB_OK);
		return NTERR_ERROR;
	}
	
	ir = (struct ntfs_attr_index_root *)(((char*)irattr) + irattr->res.attr_ofs);
	if (!BOUNDS(ir, aMFT, sizeof(aMFT))) return NTERR_BOUNDS;
	
	if (ir->ih.flags != 1) {
		sprintf(buf, "Directory #%i is small index!", mftNum);
		MessageBox(0, buf, "StartReadingDirFile()", MB_OK);
		return NTERR_ERROR;
	}

	// Read $INDEX_ROOT, always nonresident
	if ( (iRet = GetAttribute(NTATTR_INDEX_ALLOC, &irattr)) != NTFS_OK) {
		sprintf(buf, "Error %i getting $INDEX_ALLOC for MFT# %i", iRet, mftNum);
		MessageBox(0, buf, "cNTFS::StartReadingDirFile", MB_OK);
		return NTERR_ERROR;
	}

	if (irattr->resident != 0x1) {
		sprintf(buf, "MFT# %i INDEX_ALLOC is resident!", mftNum);
		MessageBox(0, buf, "cNTFS::StartReadingDirFile", MB_OK);
		return NTERR_ERROR;
	}

	pRunData = (char*)(((char*)irattr) + irattr->non.data_ofs);

	char outbuf[1000];
	sprintf(outbuf, "Data: ");

	for (int i = 0; i < 20; ++i) {
		sprintf(buf, "%2.2hhX ", pRunData[i] & 0xFF);
		strcat(outbuf, buf);
	}
	MessageBox(0, outbuf, "Runs", MB_OK);

	if (!BOUNDS(pRunData, aMFT, sizeof(aMFT))) return NTERR_BOUNDS;

	iRet = DecodeRunlist(pRunData, (int)(pRunData - (char*)aMFT), aRun, 10);

	return NTFS_OK;
}

int cNTFS::GetNextDirFile() {
	return NTFS_OK;

}


/* Decodes a run list pointed to by pSource, which is at most iSourceSize long.
   Place decoded runs into pList, which should be defined as pList[iListSize+1].
   The run list will be terminated with lcn == -1, EVEN IF AN ERROR OCCURS.
   The caller can use this partial run list if desired. If the first entry is -1,
   no run list was decoded.
   RETURNS
   NTERR_BOUNDS  - pSource[iSourceSize] exceeded with no end of runlist found
   NTERR_BUFFER  - pList[iListSize] exceeded
   NTERR_INVALID - A decoded LCN falls outside of the volume. pList will contain
                   a truncated list ending at BUT NOT INCLUDING the invalid LCN.

	
*/
int cNTFS::DecodeRunlist(char *pSource, int iSourceSize, 
						 ntfs_runlist *pList, int iListSize) {
	int iLenSize, iOffSize, iDx = 0, iRun;
	int iShift;
	int iRet = NTFS_OK;

	for (iRun = 0; iRun < iListSize; ++iRun) {
		pList[iRun].lcn = pList[iRun].len = 0;		// Zero run info

		if (iDx >= iSourceSize) { iRet = NTERR_BOUNDS; break; }
		if (iRun >= iListSize)  { iRet = NTERR_BUFFER; break; }
		if (pSource[iDx] == 0) break;
		iLenSize = pSource[iDx] & 0x0000000F;
		iOffSize = (pSource[iDx] & 0x000000F0) >> 4; 
		++iDx;
		for (iShift = 0; iShift < iLenSize; ++iShift,++iDx)
			pList[iRun].len |= (((int)pSource[iDx]) & 0x000000FFL) << (iShift * 8L);
		for (iShift = 0; iShift < iOffSize; ++iShift,++iDx)
			pList[iRun].lcn |= (((int)pSource[iDx]) & 0x000000FFL) << (iShift * 8L);
		if (iRun > 0) {
			if (pList[iRun].lcn & ( 1 << ((iOffSize * 8)-1) ) ) {
				pList[iRun].lcn = pList[iRun-1].lcn + ( ((qword)pList[iRun].lcn) | 
					0xFFFFFFFFFFFFFFFF << (qword)(iLenSize * 8L));
			}
			else 
				pList[iRun].lcn += pList[iRun-1].lcn;
		}
		// TODO Add NTERR_INVALID check make sure lcn is within vol
	}

	pList[iRun].lcn = pList[iRun].len = -1;     // Mark the last run
	if (iRun == iListSize) return NTERR_BUFFER;

	return iRet;
}


char *cNTFS::AttributeName(int iVal) {
	switch (iVal) {
		case 0x10:	return "$STANDARD_INFORMATION";
		case 0x20:	return "$ATTRIBUTE_LIST";
		case 0x30: 	return "$FILE_NAME";
		case 0x40:  return "$OBJECT_ID";
		case 0x50:	return "$SECURITY_DESCRIPTOR";
		case 0x60:	return "$VOLUME_NAME";
		case 0x70:	return "$VOLUME_INFORMATION";
		case 0x80:	return "$DATA";
		case 0x90:	return "$INDEX_ROOT";
		case 0xA0:	return "$INDEX_ALLOCATION";
		case 0xB0:	return "$BITMAP";
		case 0xC0:	return "$REPARSE_POINT";
		case 0xD0:	return "$EA_INFORMATION";
		case 0xE0:	return "$EA";
		case 0xF0:	return "$PROPERTY_SET";
		case 0x100:	return "$LOGGED_UTILITY_STREAM";
		default: break;
	}
	return "!UNKNOWN!";
}



int cNTFS::ExtractMFTFilename(char *szTarget, int *iFlags, char *pMFT, int iSize) {
	char szNames[4][1001];
	struct ntfs_mft_header *m;
	struct ntfs_attribute *attr = NULL;
	struct ntfs_attr_file_name *fn;
	int iSan = 0;

	if (pMFT == NULL) pMFT = aMFT;
	if (iSize == 0) iSize = sizeof(aMFT);

	m = (struct ntfs_mft_header *)pMFT;


	szNames[0][0] = szNames[1][0] = szNames[2][0] = szNames[3][0] = 0;
	szTarget[0] = 0;		

	*iFlags = m->flags;

	while ( iSan++ < 10 && (GetNextAttribute(NTATTR_FILE_NAME, &attr, pMFT) == NTFS_OK) ) {
		fn = (struct ntfs_attr_file_name*)(((char*)attr) + attr->res.attr_ofs);
		if (!BOUNDS(fn, pMFT, iSize)) return NTERR_BOUNDS;
		if ( !( (fn->ns >= 0) && (fn->ns <= 3) ) ) continue;  // unknown namespace, ignore	
		wstrncpy(szNames[fn->ns], &fn->name, fn->len);
		szNames[fn->ns][fn->len] = 0;
	}
	
	// Order to return  WIN32 -> BOTH -> DOS -> POSIX    not sure about POSIX
		 if (szNames[NTFS_FILENS_WIN32][0] != 0) strncpy(szTarget, szNames[NTFS_FILENS_WIN32], 255);
	else if (szNames[NTFS_FILENS_BOTH][0] != 0) strncpy(szTarget, szNames[NTFS_FILENS_BOTH], 255);
	else if (szNames[NTFS_FILENS_DOS][0] != 0) strncpy(szTarget, szNames[NTFS_FILENS_DOS], 255);
	else if (szNames[NTFS_FILENS_POSIX][0] != 0) strncpy(szTarget, szNames[NTFS_FILENS_POSIX], 255);
	else return NTERR_NOTFOUND;

	return NTFS_OK;
}



/* Reads the filename of record mftNUM and places it in szTarget 
   without altering aMFT. szTarget must be at least 255 bytes.
   iFlags receives the MFT flag byte, indicating if file is inuse or a directory,
   and can be NULL if not desired.
   TODO verify max limit of unicode filename

*/
int cNTFS::GetMFTFilename(unsigned long mftNum, char *szTarget, int *iFlags) {
	char aBuf[(512*NTFS_MAX_CLUSTERSIZE)+10];
	struct ntfs_mft_header *m = (struct ntfs_mft_header *)aBuf;
	int iRet;
	struct ntfs_attribute *attr = NULL;
	struct ntfs_attr_file_name *fn;

	int iSan = 0;
	char szNames[4][1001];

	szNames[0][0] = szNames[1][0] = szNames[2][0] = szNames[3][0] = 0;

	szTarget[0] = 0;		


	if ( (iRet = ReadMFT(mftNum, aBuf)) != NTFS_OK) return iRet;

	if (iFlags != NULL) {
		*iFlags = m->flags;
	}

	while ( iSan++ < 10 && (GetNextAttribute(NTATTR_FILE_NAME, &attr, aBuf) == NTFS_OK) ) {
		fn = (struct ntfs_attr_file_name*)(((char*)attr) + attr->res.attr_ofs);
		if (!BOUNDS(fn, aBuf, sizeof(aMFT))) return NTERR_BOUNDS;
		if ( !( (fn->ns >= 0) && (fn->ns <= 3) ) ) continue;  // unknown namespace, ignore	
		wstrncpy(szNames[fn->ns], &fn->name, fn->len);
		szNames[fn->ns][fn->len] = 0;
	}
	
	// Order to return  WIN32 -> BOTH -> DOS -> POSIX    not sure about POSIX
		 if (szNames[NTFS_FILENS_WIN32][0] != 0) strncpy(szTarget, szNames[NTFS_FILENS_WIN32], 255);
	else if (szNames[NTFS_FILENS_BOTH][0] != 0) strncpy(szTarget, szNames[NTFS_FILENS_BOTH], 255);
	else if (szNames[NTFS_FILENS_DOS][0] != 0) strncpy(szTarget, szNames[NTFS_FILENS_DOS], 255);
	else if (szNames[NTFS_FILENS_POSIX][0] != 0) strncpy(szTarget, szNames[NTFS_FILENS_POSIX], 255);
	else return NTERR_NOTFOUND;

	

	return NTFS_OK;
}



/* Decodes the index block contained in aBlock[iBlock]. Add MFT#'s and flags
   to mapFile. 
   DO NOT CLEAR mapFile so a caller can use this function multiple times to
   decode several linked index blocks.
   NOTE : I think leaf nodes (flag & 0x01) can be ignored, as they mark the VCN of
          the index block before an entry. This seems to be used for sorting. If we 
		  don't care about sorting, we can ignore these and just extract all files on
		  all runlists.
   TODO : Make sure there's no whacky situation where part of the runlist is really invalid
          which means we can only read nodes pointed to by leaf nodes.
*/
int cNTFS::DecodeIndexBlock(char *aBlock, int iBlock, MapFileMFT &mapFile) {
	word *usa, seq, *ustest;
	struct ntfs_attr_index_entry *ind;
	struct ntfs_attr_index_alloc *alloc = (struct ntfs_attr_index_alloc *)aBlock; 
	int i;
	int iSanity, iSanityLimit;
	// ---- DEBUG
/*	extern char out[];
	char buf[10000];
	char c; 
*/
	// ---- DEBUG

	// Check and do the fixups	
	usa = (word*)&aBlock[alloc->usa_ofs];
	if (!BOUNDS(usa, aBlock, iBlock)) return NTERR_BOUNDS;
	seq = usa[0]; ++usa;			// Get the sequence number, advance to the array
	for (i = 0; i < (alloc->usa_count - 1); ++i) {		// the count includes the seq num
		ustest = (word*)(&aBlock[(512*(i+1))-2]);
		if ( *ustest != seq) return NTERR_FIXUP;
		*ustest = usa[i];
	}

	// Setup the pointer to the first index entry
	ind = (struct ntfs_attr_index_entry *)(((char*)(&alloc->ih)) + alloc->ih.indx_ofs);
	if (!BOUNDS(usa, aBlock, iBlock)) return NTERR_BOUNDS;

	iSanityLimit = (iBlock / 16) + 10; // Assuming min length of 16
	iSanity = 0;

	// Walk through the entries, add filenames to the map
	while ( (++iSanity < iSanityLimit) & !(ind->flags & 0x02)) {
		// ------------------------------------- DEBUG
/*		sprintf(buf, "fileref: %8.8lu  indlen: %2i  keylen: %2i  flags: %hhXh\r\n", 
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
		strcat(out, "\r\n\r\n");
*/
		// ------------------------------------- DEBUG

		mapFile[(long)ind->fileref] = 1;	// Add to our map. If already in, will just
		                                    // update variable.
		ind = (struct ntfs_attr_index_entry *)((char*)ind + ind->index_len); // advance
		if (!BOUNDS(ind, aBlock, iBlock)) return NTERR_BOUNDS;
	}
	return NTFS_OK;
}




/* Decodes the $INDEX_ALLOCATION attribute pointed at by h. Fill mapFile with
   a list of the MFT's and flags of all files in the index.
   ir is the $INDEX_ROOT attribute.
   The run lists point to INDX blocks, which contain the indexes.

   IMPORTANT: h must point inside a buffer containing a valid MFT. h+iLen should
   be near the end of the buffer, or where the end of the data runs can reasonably 
   be expected. 
   
   NOTE I think the size of each INDX block can be less than one cluster, which 
   would result in a negative Clusters per Index. Right now Size of Index Alloc in
   bytes seems to always be 4096 and Index Clusters is 1, on a 4 sector per cluster
   drive. ulSectorsPerIndex is already pulled out of the boot sector, but each index
   also has its own size. I'm assuming INDX blocks will always be >1 cluster, and will
   check all of the various values and error out if this is not true.

   TODO See how this works out in the wild and verify that this is ok
   TODO Files appear more than once in the index, with different streams. Make sure that
        the flags are the same for each instance of the file. Also, decode the streams 
		and if we can use something in them.
*/

int cNTFS::DecodeIndexAlloc(struct ntfs_attribute *attrir, struct ntfs_attribute *attria, 
							int iLen, MapFileMFT &mapFile) {
	MapFileMFT::iterator itr;
	struct ntfs_runlist aRunList[2002];
	char *pRunData = (char*)(((char*)attria) + attria->non.data_ofs);
	int iRet;
	int iRun, iBlock;
	char aBlock[512*8*5];	// 5 clusters of 8 sectors or 20kb
	struct ntfs_attr_index_alloc *alloc = (struct ntfs_attr_index_alloc *)aBlock; 

	int iFlags = 0;
	struct ntfs_attr_index_root *ir = (struct ntfs_attr_index_root *)((char*)attrir + attrir->res.attr_ofs);




	// Should be an ok BOUNDS, keep it in the MFT buffer
	if (!BOUNDS(ir, attrir, 2048)) return NTERR_BOUNDS;



	// Some size checking
	if (ulSectorsPerIndex != (ir->index_size / ulBytesPerSector)) {
		char tBuf[501];
		sprintf(tBuf, "ulSectorsPerIndex: %lu   ir->index_size / ulBytesPerSector: %lu",
			ulSectorsPerIndex, ir->index_size / ulBytesPerSector);
		MessageBox(0, tBuf, "DecodeIndexAlloc()", MB_OK);
		return NTERR_ERROR;
	}

	// Decode the runlist
	/* NOTE : When there are 1 cluster index blocks in seperate locations, each block is a seperate
		      INDX record. If the runlist shows sequential blocks, are they one large block or 
			  several individual blocks? ASSUME INDIVIDUAL BLOCKS FOR NOW.
	   TODO : Find out 
	*/
	if (!BOUNDS(pRunData, attria, iLen)) return NTERR_BOUNDS;
	iRet = DecodeRunlist(pRunData, iLen, aRunList, 2000);
	if (iRet < 0) return iRet;
	
	iRun = iBlock = 0;

	while (aRunList[iRun].len != -1) {								// Go through the runlist
		for (iBlock = 0; iBlock < aRunList[iRun].len; ++iBlock) {	// Load and parse each block
			if (!(iRet = ReadClusters(aBlock, aRunList[iRun].lcn + iBlock, 1))) return NTERR_READ;
			if (alloc->magic != 0x58444e49) return NTERR_MAGIC;   // check for INDX
			if ((iRet = DecodeIndexBlock(aBlock, sizeof(aBlock), mapFile)) != NTFS_OK) return iRet;
		} // for (iBlock
		++iRun;
	} // while (aRunList...




/*
	char out[20000];
	char buf[10000];
	out[0] = 0;
	for (i = 0; i < 2 * 8 * 512; ++i) {
		sprintf(buf, "%2.2hhX ", ((char*)ind)[i]  & 0xFF);
		strcat(out,buf);
		if (i > 5 && !(i % 16)) {
		strcat(out, "\r\n");
		}
	}

	SetDlgItemText(hMFTDlg, IDC_TEXT, out);
*/	

	return NTFS_OK;
}



/* Reads the first 4 bytes of each cluster and searches for INDX, marking an
   index block. Puts the cluster number of each index block in vecIndex.
   If bFixup is true, check the index block fixups before adding an INDX record.
   This will make the scan slower, but reduces the chance of false hits.
   bCancel lets another thread call this function and then cancel it. If bCancel
   is true the function will exit early. The function should be called with bCancel
   set to false.
	NOTE - I'm guessing its faster to just read an entire sector instead of 4 bytes. 
	TODO - Do some benchmarks later on
	NOTE - I think that because an index can span multiple clusters, and will only
	       get the first cluster of a fragmented index block, we can only check the
		   first ulSectorsPerCluster fixups. Ignore the rest.
*/
int cNTFS::FindAllIndexBlocks(vector<int> &vecIndex, bool bFixup, bool *bCancel) {
	unsigned long ulCluster = 0;
	unsigned long ulSector = 0;
	char aBuf[512 * 8 * 8];	// 32768 - 8 sector clusters, 8 cluster ib's
	int iBufSize = sizeof(aBuf);
	int iRet;
	struct ntfs_attr_index_alloc *alloc = (struct ntfs_attr_index_alloc *)aBuf;
	word *usa, seq, *ustest;
	int i, iFix;

	ulCluster = 4900000;
	ulSector = 0;
	vecIndex.clear();

	memset(aBuf, 0, sizeof(aBuf));

	while ( (ulSector < (ulParSize + ulParOffset)) && !(*bCancel)) {
		if ( (iRet = ReadSector(aBuf, (qword)ulSector, 1)) != NTFS_OK) return iRet;
		if (alloc->magic == 0x58444e49) {
			if (bFixup) {
				ReadClusters(aBuf, ulCluster, 1);  // Read the entire cluster
				usa = (word*)&aBuf[alloc->usa_ofs];
				if (!BOUNDS(usa, aBuf, iBufSize)) goto FAILoop;
				seq = usa[0]; ++usa;			// Get the sequence number, advance to the array
				iFix = alloc->usa_count - 1;
				if (iFix > ulSectorsPerCluster) iFix = ulSectorsPerCluster;
				for (i = 0; i < iFix; ++i) {		// the count includes the seq num
					ustest = (word*)(&aBuf[(512*(i+1))-2]);
					if ( *ustest != seq) goto FAILoop;
					// *ustest = usa[i];  // dont actually have to do the fixup, just check
				}				
				vecIndex.push_back(ulCluster);
			} else {
				vecIndex.push_back(ulCluster);
			}
		}
		FAILoop:
		++ulCluster;
		ulSector = (ulCluster * ulSectorsPerCluster) + ulParOffset;
		if (ulCluster > 5000000) break;
	}

	if (*bCancel) return NTERR_CANCEL;
	return NTFS_OK;
}

/* Decodes an $INDEX_ROOT entry and puts the files in mapFile */
int cNTFS::DecodeIndexRoot(struct ntfs_attribute *attrir, MapFileMFT &mapFile) {
	struct ntfs_attr_index_root *ir = (struct ntfs_attr_index_root *)(((char*)attrir) + attrir->res.attr_ofs);
	struct ntfs_attr_index_entry *ind;
	int iSanity = 0, iSanityLimit;		

	if (!BOUNDS(ir, attrir, 2000)) return NTERR_BOUNDS;
	ind = (struct ntfs_attr_index_entry *)(((char*)(&ir->ih) + ir->ih.indx_ofs));
	if (!BOUNDS(ind, attrir, 2000)) return NTERR_BOUNDS;

	iSanityLimit = 4000 / 16;

	while (!(ind->flags & 0x02) && (++iSanity < iSanityLimit)) {
		mapFile[(long)ind->fileref] = 1;	// Add to our map. If already in, will just
		                                    // update variable.
		ind = (struct ntfs_attr_index_entry *)((char*)ind + ind->index_len); // advance
		if (!BOUNDS(ind, attrir, 2000)) return NTERR_BOUNDS;
	
	}
	
	return NTFS_OK;
}

int cNTFS::GetDirectoryFiles(unsigned long mftNum, MapFileMFT &mapFile) { 
	char aBuf[512*NTFS_MAX_CLUSTERSIZE];
	int iRet;
	struct ntfs_attribute *attrir = NULL;
	struct ntfs_attribute *attria = NULL;
	struct ntfs_attr_index_root *ir = NULL;

	// Read the MFT and get $INDEX_ROOT
	if ( (iRet = ReadMFT(mftNum, aBuf)) != NTFS_OK) return iRet;
	if ( (iRet = GetNextAttribute(NTATTR_INDEX_ROOT, &attrir, aBuf)) != NTFS_OK) return iRet;
	ir = (struct ntfs_attr_index_root *)(((char*)attrir) + attrir->res.attr_ofs);
	if (!BOUNDS(ir, aBuf, sizeof(aBuf))) return NTERR_BOUNDS;
	
	if (ir->ih.flags == 0) {		// Small index, decode right here
		return DecodeIndexRoot(attrir, mapFile);
	} else {						// Large index, get $INDEX_ALLOC and decode
		if ( (iRet = GetNextAttribute(NTATTR_INDEX_ALLOC, &attria, aBuf)) != NTFS_OK) return iRet;
		return DecodeIndexAlloc(attrir, attria, sizeof(aBuf), mapFile);
		
	}
}


int cNTFS::GetMFTFiledata(unsigned long mftNum, struct file_data *pData) {
	char aBuf[512*NTFS_MAX_CLUSTERSIZE];
	int iRet;
	struct ntfs_attribute *attr = NULL;
	struct ntfs_attr_file_name *fn;
	struct ntfs_attr_file_name *afn[4];
	struct ntfs_attribute *aat[4];
	struct ntfs_mft_header *m = (struct ntfs_mft_header *)aBuf;
	int iLen;
	int iSan = 0;

	afn[0] = afn[1] = afn[2] = afn[3] = NULL;
	aat[0] = aat[1] = aat[2] = aat[3] = NULL;
	if ( (iRet = ReadMFT(mftNum, aBuf)) != NTFS_OK) return iRet;

	// Scan the MFT for all $FILE_NAME entries
	while ( iSan++ < 10 && (GetNextAttribute(NTATTR_FILE_NAME, &attr, aBuf) == NTFS_OK) ) {
		fn = (struct ntfs_attr_file_name*)(((char*)attr) + attr->res.attr_ofs);
		if (!BOUNDS(fn, aBuf, sizeof(aBuf))) return NTERR_BOUNDS;
		if ( !( (fn->ns >= 0) && (fn->ns <= 3) ) ) continue;  // unknown namespace, ignore	
		afn[fn->ns] = fn; aat[fn->ns] = attr;
	}

	// Setup fn and attr pointer
	if (afn[NTFS_FILENS_WIN32] != NULL)      iRet = NTFS_FILENS_WIN32;
	else if (afn[NTFS_FILENS_BOTH] != NULL)  iRet = NTFS_FILENS_BOTH;
	else if (afn[NTFS_FILENS_DOS] != NULL)   iRet = NTFS_FILENS_DOS;
	else if (afn[NTFS_FILENS_POSIX] != NULL) iRet = NTFS_FILENS_POSIX;
	else return NTERR_NOTFOUND;

	fn = afn[iRet]; attr = aat[iRet];	
	

	iLen = (fn->len < MAX_FILENAME)?fn->len:MAX_FILENAME;
	wstrncpy(pData->szName, (char*)&fn->name, iLen);
	pData->mft          = mftNum;
	pData->mftparent    = fn->parent;
	pData->timeCreated  = fn->created;
	pData->timeModified = fn->altered;
	pData->fileFlags    = fn->flags;
	pData->mftFlags     = m->flags;
	pData->standFlags   = attr->flags;

	// NOTE the realsize and alloc size in the $FILE_NAME always seem to be 0. Some docs said
	// they only appear if the VCN is 0. Check the $DATA attribute for the size. Set to -1 if
	// not found.
	attr = NULL;
	if ( (iRet = GetNextAttribute(NTATTR_DATA, &attr, aBuf) != NTFS_OK) ) {
		pData->size = -1;
	} else {
		if (attr->resident == 0) pData->size = attr->res.attr_len;
		else pData->size = attr->non.realsize;
	}
	


	
	return NTFS_OK;
}