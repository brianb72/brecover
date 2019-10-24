#ifndef __CNTFS_H__
#define __CNTFS_H__


#include "structs.h"


#pragma warning (disable: 4786)
#pragma warning (disable: 4788)
#include <functional>
#include <algorithm>
#include <iostream> 
#include <vector>
#include <map>
#include <string>
using namespace std;


#define NTFS_OK			1			// No error occured
#define NTERR_ERROR		0			// Generic error
#define NTERR_READ		-1			// Error while reading
#define NTERR_MAGIC		-2			// Magic number or sequence is wrong
#define NTERR_FIXUP		-3			// Fixups failed
#define NTERR_NOTADIR	-4			// MTF entry is not a directory
#define NTERR_BOUNDS	-5			// 
#define NTERR_NOTFOUND	-6			// An item we are searching for was not found
#define NTERR_BUFFER	-7			// A buffer passed to a function was not large enough
#define NTERR_INVALID	-8			// A structure passed contained an invalid value
#define NTERR_CANCEL	-9			// An operation was canceled by the caller

#define NTFS_MAX_CLUSTERSIZE	20 // Largest possible cluster size


#define NTFS_ROOTDIR	5			// MFT of root directory

#define MAX_FILENAME	255

struct file_data {
	char szName[MAX_FILENAME+5];	
	qword	mft;					// This files MFT
	qword	mftparent;				// MFT of parent
	qword	timeCreated;			// Time of creation		
	qword	timeModified;			// Last modified (altered)
	qword	size;					// Real size in bytes
	byte	mftFlags;				// 1 = in use  2 = directory
	word	standFlags;				// SAH - 0x1 = compressed  0x4000 = enc  0x8000 = sparse
	word	fileFlags;				// $FILE_NAME flags
};

typedef map<long, int> MapFileMFT;			// MFT# / FLAG pairs
typedef map<long, file_data> MapFileData;	// MFT# / file_data pairs

class cNTFS {
public:	
	HANDLE hDev;		// Handle to the device to use
public:			// Low level device functions
	cNTFS();
	bool OpenDevice(char *szWhich);
	void CloseDevice();
	void SetPartition(unsigned long ulOffset, unsigned long ulSize);
	bool ReadSector(char *paTarget, qword u64Sector, int iCnt = 1);
public:			// NTFS Functions
	bool ParseMFTBoot();			// MUST BE CALLED FIRST BEFORE ANY OTHER OPERATIONS
	int ReadMFT(unsigned long mftNum, char *pBuf = NULL);	// Read an MFT record into memory
	int GetDirectoryFiles(unsigned long mftNum, MapFileMFT &mapFile);
	int StartReadingDirFile(unsigned long mftNum);
	int GetNextDirFile();
	int GetAttribute(int iVal, struct ntfs_attribute **pattr, char *pBuf = NULL);
	int GetNextAttribute(int iVal, struct ntfs_attribute **pattr, char *pBuf = NULL);
	int DecodeRunlist(char *pSource, int iSourceSize, ntfs_runlist *pList, int iListSize);
	char *AttributeName(int iVal);
	int GetMFTFilename(unsigned long mftNum, char *szTarget, int *iFlags = NULL);
	int GetMFTFiledata(unsigned long mftNum, struct file_data *pData);
	int ExtractMFTFilename(char *szTarget, int *iFlags, char *pMFT = NULL, int iSize = 0);
	int DecodeIndexAlloc(struct ntfs_attribute *attrir, struct ntfs_attribute *attria, int iLen, MapFileMFT &mapFile);
	bool ReadClusters(char *paTarget, qword ulCluster, int iCnt = 1);
	int FindAllIndexBlocks(vector<int> &vecIndex, bool bFixup, bool *bCancel);
	int DecodeIndexBlock(char *aBlock, int iBlock, MapFileMFT &mapFile);
	int DecodeIndexRoot(struct ntfs_attribute *attrir, MapFileMFT &mapFile);
public:
	unsigned long ulParOffset;		// LBA offset to start of partition. -1 when nonexistant
	unsigned long ulParSize;		// Size of partition in sectors. 0 when nonexistant
	unsigned long ulBytesPerSector;	// Should always be 512
	unsigned long ulSectorsPerCluster;
	char MediaDescriptor;			// F8h = hard disk
	qword	lcnMFT;			
	qword	lcnMFTMirr;
	qword	lcnCurrentMFT;					// MFT use
	unsigned long	ulSectorsPerMFT;	// Sectors per MFT, usually 2 
	unsigned long	ulSectorsPerIndex;	// Sectors per Index, ...
	char aSector[1050];				// Buffer for last read sector, 512 bytes long
	char aCluster[(512*NTFS_MAX_CLUSTERSIZE)+10];	// Buffer for last read cluster
	char aMFT[(512*NTFS_MAX_CLUSTERSIZE)+10];		// Buffer for last read MFT record
public:			// Owner controlled variables
	unsigned long mftCurDir;		// MFT # of Current Directory. 0 == 5 == Root directtory
	vector<int>   vecDir;			
};



#endif