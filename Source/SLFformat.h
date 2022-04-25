#pragma once

#define SLF_PATHSIZE 256

#pragma pack(push, 1)
struct SLFheader_s //Header is at the start of the SLF file
{
	char archiveFilename[SLF_PATHSIZE]; //Matches filename of the SLF archive
	char basePath[SLF_PATHSIZE]; //Base path for everything in the archive (last character is a backward slash)
	int entries;
	int usedEntries;
	unsigned short sort;
	unsigned short version;
	bool containsSubDirectories;
	int unused;
	unsigned short unused2;
};

struct SFLfileEntry_s //File entries are at the end of the SLF file
{
	char filename[SLF_PATHSIZE];
	unsigned int offset;
	unsigned int size;
	unsigned char state;
	unsigned char unused;
	unsigned short unused3;
	_FILETIME filetime;
	unsigned short unused2;
	unsigned short unused4;
};
#pragma pack(pop)