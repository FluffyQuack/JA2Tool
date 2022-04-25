#include <stdio.h>
#include <tchar.h>
#include <direct.h>
#include "misc.h"
#include <Windows.h>
#include "SLFformat.h"
#include "ja2tool.h"
#include "STI\ConvertFromSTI.h"

static void FilenameVariantWithoutPathAndExtension(char *to, char *from)
{
	int lastSlash = LastSlash(from), copyFrom = 0;

	if ((from[lastSlash] == '\\' || from[lastSlash] == '/') && from[lastSlash + 1] != 0)
		copyFrom = lastSlash + 1;
	else
	{
		lastSlash = LastColon(from);
		if (from[lastSlash] == ':' && from[lastSlash + 1] != 0)
			copyFrom = lastSlash + 1;
	}

	int lastDot = LastDot(from), copyTo = (int) strlen(from);

	if (from[lastDot] == '.' && lastDot > 0)
		copyTo = lastDot - 1;

	memcpy(to, &from[copyFrom], (copyTo - copyFrom) + 1);
	to[(copyTo - copyFrom) + 1] = 0;
}

void DataToHex(char *to, char *from, int size) 
{
	char const hextable[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

	int i, j = 0;
	for(i = 0; i < size; i++)
	{
		to[j] = hextable[from[i] >> 4 & 0xf];
		to[j + 1] = hextable[from[i] & 0xf];
		j += 2;
	}
}

void MakeDirectory(char *fullpath)
{
	int pos = 0;
	char path[MAXPATH];
	memset(path, 0, MAXPATH);

	while(1)
	{
		if(fullpath[pos] == 0)
			return;
		path[pos] = fullpath[pos];
		pos++;
		if(fullpath[pos] == '\\' || fullpath[pos] == '/')
			_mkdir(path);
	}
}

bool LoadSLFFile(char *slfPath, SLFheader_s *slfHeader, SFLfileEntry_s **slfEntries, unsigned long long *slfSize)
{
	//Open file
	FILE *slfFile;
	fopen_s(&slfFile, slfPath, "rb");
	if(!slfFile)
	{
		printf("Error: Failed to open %s for reading.\n", slfPath);
		return 0;
	}

	//Acquire file size
	fpos_t fileSize;
	_fseeki64(slfFile, 0, SEEK_END);
	fgetpos(slfFile, &fileSize);
	_fseeki64(slfFile, 0, SEEK_SET);

	//Read header
	fread(slfHeader, sizeof(SLFheader_s), 1, slfFile);

	//Read entry list
	long long entryListPos = fileSize - (sizeof(SFLfileEntry_s) * slfHeader->entries);
	if(entryListPos < sizeof(SLFheader_s))
	{
		printf("Error: Invalid entry list position in %s.\n", slfPath);
		return 0;
	}
	_fseeki64(slfFile, entryListPos, SEEK_SET);
	*slfEntries = new SFLfileEntry_s[slfHeader->entries];
	fread(*slfEntries, sizeof(SFLfileEntry_s), slfHeader->entries, slfFile);

	if(slfSize)
		*slfSize = fileSize;

	//Finish
	fclose(slfFile);
	return 1;
}

void ListFilesInSLF(char *filename)
{
	printf("Attempting to list files in %s...\n", filename);

	//Load SLF archive
	SLFheader_s slfHeader;
	SFLfileEntry_s *slfEntries;
	if(!LoadSLFFile(filename, &slfHeader, &slfEntries, 0))
		return;

	//Make variant of SLF filename without any extension
	char slfDirName[MAXPATH];
	FilenameVariantWithoutPathAndExtension(slfDirName, filename);
	
	//Text file
	char listFilename[MAXPATH];
	sprintf_s(listFilename, MAXPATH, "%s.txt", slfDirName);
	FILE *file;
	fopen_s(&file, listFilename, "wb");
	if(!file)
	{
		printf("Could not open %s for writing\n", listFilename);
		return;
	}

	//List all files
	printf("Listing files...\n");
	for(int i = 0; i < slfHeader.entries; i++)
	{
		SYSTEMTIME fileDate;
		FileTimeToSystemTime(&slfEntries[i].filetime, &fileDate);
		fprintf(file, "%s%s (size: %u, time: %04i/%02i/%02i %02i:%02i)\r\n", slfHeader.basePath, slfEntries[i].filename, slfEntries[i].size, fileDate.wYear, fileDate.wMonth, fileDate.wDay, fileDate.wHour, fileDate.wMinute);
		printf("%s%s\n", slfHeader.basePath, slfEntries[i].filename);
	}

	//Finish
	delete[]slfEntries;
	fclose(file);
	printf("Wrote %s\n", listFilename);
	printf("Listed %i files from %s\n", slfHeader.entries, filename);
	printf("Quantity of 'usedEntries' in SLF: %i\n", slfHeader.usedEntries);
}

void ExtractSLF(char *filename)
{
	printf("Attempting to unpack %s...\n", filename);
	
	//Load SLF archive
	SLFheader_s slfHeader;
	SFLfileEntry_s *slfEntries;
	if(!LoadSLFFile(filename, &slfHeader, &slfEntries, 0))
		return;
	FILE *slfFile;
	fopen_s(&slfFile, filename, "rb");
	if(!slfFile)
	{
		delete[]slfEntries;
		return;
	}

	//Make variant of SLF filename without any extension
	char slfDirName[MAXPATH];
	FilenameVariantWithoutPathAndExtension(slfDirName, filename);

	//Extract all files
	printf("Extracting files...\n");
	bool anyErrors = 0;
	unsigned int filesExtracted = 0, filesFailed = 0, STIsConverted = 0, STIsFailedToConvert = 0;
	for(int i = 0; i < slfHeader.entries; i++)
	{
		char finalFilepath[MAXPATH];
		sprintf(finalFilepath, "%s\\%s", slfDirName, slfEntries[i].filename);
		
		if(slfEntries[i].size >= 1000000000 || slfEntries[i].size >= 1000000000)
		{
			printf("Error: Abnormally high file size for entry %s\n", finalFilepath);
			filesFailed++;
			anyErrors = 1;
			continue;
		}

		//Read data
		unsigned char *fileData = new unsigned char[slfEntries[i].size];
		_fseeki64(slfFile, slfEntries[i].offset, SEEK_SET);
		fread(fileData, 1, slfEntries[i].size, slfFile);

		//Write file
		MakeDirectory(finalFilepath);
		FILE *newFile;
		fopen_s(&newFile, finalFilepath, "wb");
		if(newFile)
		{
			//Write file data
			fwrite(fileData, slfEntries[i].size, 1, newFile);
			fclose(newFile);

			//Update the file's filetime entries
			HANDLE fileHandle;
			fileHandle = CreateFile(finalFilepath, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if(fileHandle != INVALID_HANDLE_VALUE)
			{
				if(SetFileTime(fileHandle, &slfEntries[i].filetime, &slfEntries[i].filetime, &slfEntries[i].filetime) == 0)
				{
					DWORD errorCode = GetLastError();
					printf("Warning: Failed to update file's FileTime entries. Error code: %i\n", errorCode);
				}
			}
			else
			{
				DWORD errorCode = GetLastError();
				printf("Warning: Failed to open a file for updating the file's FileTime entries. Error code: %i\n", errorCode);
			}

			printf("Extracted %s\n", finalFilepath);
			filesExtracted++;

			//Check if we should convert this file
			int fileType = CheckFileType(finalFilepath);
			if(convertFromSTI && fileType == FILETYPE_STI)
			{
				bool success = ConvertFromSTI(finalFilepath);
				if(success)
					STIsConverted++;
				else
					STIsFailedToConvert++;
			}
		}
		else
			printf("Error: Failed to open %s for writing.\n", finalFilepath);
		delete[]fileData;
	}

	//Finish
	delete[]slfEntries;
	fclose(slfFile);
	printf("\nExtracted %i files from %s\n", slfHeader.entries, filename);
	printf("Quantity of 'usedEntries' in SLF: %i\n", slfHeader.usedEntries);
	printf("Base path in SLF: %s\n", slfHeader.basePath);
	if(convertFromSTI && (STIsConverted || STIsFailedToConvert))
	{
		printf("Converted %i STI files\n", STIsConverted);
		if(STIsFailedToConvert)
			printf("Failed to convert %i STI files\n", STIsFailedToConvert);
	}
	if(filesFailed)
		printf("Failed to extract %i files\n", filesFailed);
	if(anyErrors || STIsFailedToConvert)
		printf("Errors were encountered.\n");
}