


#pragma pack(1)

#define NTATTR_FILE_NAME	0x30
#define NTATTR_DATA			0x80
#define NTATTR_INDEX_ROOT	0x90
#define NTATTR_INDEX_ALLOC	0xA0


// Filename name spaces
#define NTFS_FILENS_POSIX	0x00	// TODO find out if this is ever used
#define NTFS_FILENS_WIN32	0x01
#define NTFS_FILENS_DOS 	0x02
#define NTFS_FILENS_BOTH	0x03    // both WIN32 and DOS


/* NTFS Boot Sector- As defined by
	http://www.ntfs.com/ntfs-partition-boot-sector.htm
	http://www.geocities.com/thestarman3/asm/mbr/NTFSBR.htm
	http://linux-ntfs.sourceforge.net/ntfs/index.html
*/
struct ntfs_bootsector {
	byte	jump_instruction[3];
	byte	oem_id[8];
	word	BytesPerSector;	// always 512  --- BDP
	byte	SectorsPerCluster;
	word	ReservedSectors;
	byte	zero1[3];	
	word	unused1;
	byte	MediaDescriptor;
	word	zero2;
	word	SectorsPerTrack;
	word	NumberOfHeads;
	dword	HiddenSectors;
	dword	unused2;
	dword	unused3;				// -- EBDP
	qword	TotalSectors;
	qword	lcnMFT;				// Logical cluster number of $MFT
	qword	lcnMFTMirr;			// LCN of $MTFMirr
	dword	ClustersPerMFT;	// Clusters per Master File Table entry
	dword	ClustersPerIndex;		// Clusters per Index Block
	qword	VolumeSerial;		
	dword	Checksum;    
	byte	BootStrap[426];	// bootstrap code
 	word	EndOfSector;		// End of Sector Marker
};

/* Declare an array of struct ntfs_runlist, and then pass it to DecodeDatarun()
   to populate the list. */
struct ntfs_runlist {
	qword	lcn;					// Starting LCN of run
	dword	len;					// Length of entry
};


struct ntfs_res {	
	dword attr_len;
	word	attr_ofs;	// 0x18 + 2 * namelen
	byte	indexed;
	byte	padding;
};

struct ntfs_non {
	qword	start_vcn;
	qword	last_vcn;
	word	data_ofs;
	word	compsize;
	dword	padding;
	qword	allocsize;		// realsize rounded up to cluster
	qword	realsize;	
	qword	initsize;		// ?
};


/* NTFS Attribute structure */
struct ntfs_attribute {
	dword	type;
	dword	len;
	byte	resident;
	byte	namelen;		// IMPORTANT: number of wchars!
	word	name_ofs;
	word	flags;		// 0x0001 compressed 0x4000 encrypted 0x8000 sparse
	word	attrid;
	union {
		struct ntfs_res res;
		struct ntfs_non non;
	};
};

/* NTFS Master File Table header - For each file*/
struct ntfs_mft_header {
	dword magic;			// 0x454c4946 -> FILE 
	word	fixup_ofs;		// Offset to fixup
	word	fixup_cnt;		// Size in words of Update Seq Number & Array
	qword	lsn;				// Logical sequence (for $LOGFILE)
	word	seq_no;			// Sequence number  (the fixup seq number)
	word	links;			// Hard link count
	word	attr_ofs;		// Offset to attributes
	word	flags;			// 1 = in use  2 = directory
	dword	bytesinuse;		// attr_ofs + 8 ?
	dword	rec_size;		// Total allocated size
	qword	base_mft;		// Base MFT record
	word	next_attr;		//	Next attr instance
};

/* NTFS ATTRIBUTE - File Name */
struct ntfs_attr_file_name {
	qword	parent;
	qword	created;
	qword altered;
	qword mftchanged;
	qword read;
	qword	allocsize;
	qword realsize;
	dword	flags;
	dword	ea;
	byte	len;
	byte	ns;	// namespace 0x00 = posix  0x01 = win32 0x02 = dos 0x03 win32 + dos
	char name;  // first char of name
};

/* Got this def from somewhere, what is it for? 
struct ntfs_attr_index_header {
	union {
		struct {
			qword	indexed_file;	// MFT of the file described by this entry	
		} dir;
		struct {
			word	data_ofs;		// Byte offset to data from this entry
			word	data_len;		// Data length in bytes
			dword	pad;			// zero
		} indx;
	} data;
	word	length;			// byte size of this index entry
	word	key_length;		// byte size of the key value, in the index entry
	byte	flags;			// 0 = in index_root   1 = in index_allocation
	byte	pad2[3];
};  // should be 16 bytes?
*/

struct ntfs_attr_index_header {
	dword	indx_ofs;	// Offset to the first index entry
	dword	indx_len;	// Data size of index in bytes
	dword	alloc_len;	// Allocated size of index in bytes
	byte	flags;		// When in index_root  0 = small index  1 = large index (in alloc)
						// When in index_alloc 0 = leaf node (no more branches)  1 = indexes other nodes
	byte	pad[3];		// padding
};

struct ntfs_attr_index_root {
	dword	type;			// Must be 0x30h for $FILE_NAME
	dword	collation;		// Must be 1 for COLLATION_FILE_NAME
	dword	index_size;		// Size in bytes of index allocation entry
	byte	clustersperindex;	// Clusters per index record, can be negative if <1 cluster
	byte	pad1[3];
	struct ntfs_attr_index_header ih;
};


struct ntfs_attr_index_alloc {
	dword	magic;			// INDX 0x58444e49
	word	usa_ofs;		
	word	usa_count;
	qword	lsn;			// $LOGFILE sequence number of the last modification of block
	qword	lcnIndex;		// lcn of the index block. if cluster size <= index block size,
							// this counts in clusters. Otherwise, count in sectors
	struct ntfs_attr_index_header ih;
};

struct ntfs_attr_index_entry {
	qword	fileref;		// Reference to target file
	word	index_len;		// Length of index entry
	word	key_len;		// Length of the key value
	byte	flags;			// 1 = points to subnode  2 = last entry in node
	byte	pad[3];			// align to 8 byte boundry
	byte	stream;			// First byte of the key
	// index_len - 8 = qword vcn of subnode
	// byte pad[x]          // after key is a pad that aligns to 8 byte boundry
};


#pragma pack(DEFPAK)
