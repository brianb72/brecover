#ifndef __PHYSICAL_H__
#define __PHYSICAL_H__


#include <Winioctl.h>



#pragma pack(1)

struct mbr_partition {
	byte	boot;		// 80h = active
	byte	shead;		// Starting head
	byte	ssector;	// Starting sector 0-5,  6-7 high track bits
	byte	strack;		// Starting track bits 0-7
	byte	type;		// Partition type
	byte	ehead;		// Ending ...
	byte	esector;	 
	byte	etrack;		 
	dword	offset;		// Offset to partition
	dword	length;		// Length of partition in sectors
};

	
struct mbr_data {
	byte	code[446];		// 446 bytes of bootloader code
	struct mbr_partition par[4];
	word	signature;		// AA55h
};

#pragma pack(DEFPAK)


#define NUM_PHYDRIVE	10
struct phydrive_data {
	bool bExist;
	char model[101];
	char deviceid[201];
	bool bPar;					// true if par is valid
	struct mbr_partition par[4];	// partition tables
};



class cPhysical { 
public:
	cPhysical();
	~cPhysical();
	bool Init();
	bool InitWMI();
	bool ReadMBR(int iDrive);
	struct phydrive_data drive[NUM_PHYDRIVE];
	void DispMBR(int iDrive);
	char *GetPartitionName(unsigned char type);

};





#endif
