#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "misc.h"

void AddExtensionBasedOnFileType(int fileType, char *fileName, unsigned char *data)
{
	int stringLength = strlen(fileName);
	char *stringFromNull = &fileName[stringLength];
	if(fileType == FILETYPE_UNKNOWN) sprintf_s(stringFromNull, MAXPATH - stringLength, ".bin");
	else if (fileType == FILETYPE_SLF) sprintf_s(stringFromNull, MAXPATH - stringLength, ".slf");
	else if (fileType == FILETYPE_STI) sprintf_s(stringFromNull, MAXPATH - stringLength, ".sti");
}

//Check file format
int CheckFileType(char *str1)
{
	int dotPos = FirstDot(str1);
	if (dotPos != 0)
	{
		dotPos += 1;
		if (str1[dotPos] != 0)
		{
			//Get variant of filename with only first part of extension
			char ext[MAXPATH];
			sprintf(ext, "%s", &str1[dotPos]);
			int dotPos2 = FirstDot(ext);
			if (dotPos2 != 0)
				ext[dotPos2] = 0;

			if (_stricmp(ext, "slf") == 0) //Looks like a filename with pak extension
				return FILETYPE_SLF;
			else if (_stricmp(ext, "sti") == 0) //Texture
				return FILETYPE_STI;
			else if (_stricmp(ext, "png") == 0) //Texture
				return FILETYPE_PNG;
		}
	}
	return FILETYPE_UNKNOWN;
}

int LastColon(char *path)
{
	int i = 0, colon = 0;
	while (1)
	{
		if (path[i] == 0)
			break;
		if (path[i] == ':' && path[i + 1] != 0)
			colon = i;
		i++;
	}
	return colon;
}

int LastSlash(char *path)
{
	int i = 0, slash = 0;
	while (1)
	{
		if (path[i] == 0)
			break;
		if ((path[i] == '/' || path[i] == '\\') && path[i + 1] != 0)
			slash = i;
		i++;
	}
	return slash;
}

int LastDot(char *path)
{
	int i = 0, dot = 0;
	while(1)
	{
		if(path[i] == 0)
			break;
		if(path[i] == '.' && path[i + 1] != 0)
			dot = i;
		i++;
	}
	return dot;
}

int FirstDot(char *path) //This finds the first dot after the last slash (to ensure we don't find the dot as part of a directory rather than file)
{
	int i = 0, dot = 0, slash = 0;
	while(1)
	{
		if(path[i] == '\\' || path[i] == '/')
		{
			slash = i;
			dot = 0;
		}
		if(path[i] == 0)
			break;
		if(path[i] == '.' && path[i + 1] != 0 && i > slash && dot == 0)
			dot = i;
		i++;
	}
	return dot;
}

bool ReadFile(char *fileName, unsigned char **data, unsigned int *dataSize)
{
	//Open file
	FILE *file;
	fopen_s(&file, fileName, "rb");
	if(!file)
		return 0;
	fpos_t fpos = 0;
	_fseeki64(file, 0, SEEK_END);
	fgetpos(file, &fpos);
	_fseeki64(file, 0, SEEK_SET);
	*dataSize = (unsigned int) fpos;
	*data = new unsigned char[*dataSize];
	fread(*data, *dataSize, 1, file);
	fclose(file);
	return 1;
}

unsigned char CharToHex(const char *bytes, int offset)
{
	unsigned char value = 0;
	unsigned char finValue = 0;

	for(int i = 0; i < 2; i++)
	{
		char c = bytes[i + offset];
		if( c >= '0' && c <= '9' )
			value = c - '0';
        else if( c >= 'A' && c <= 'F' )
            value = 10 + (c - 'A');
        else if( c >= 'a' && c <= 'f' )
            value = 10 + (c - 'a');

		if(i == 0 && value > 0)
			value *= 16;

		finValue += value;
	}
	return finValue;
}

void CharByteToData(char *num, unsigned char *c, int size, bool littleEndian) //Note, num size has to be twice the size of "size" otherwise this function fails
{
	int j = 0;
	int i;
	if(littleEndian)
		i = 0;
	else
		i = size - 1;
	while(1)
	{
		c[i] = CharToHex(num, j);
		j += 2;
		if(littleEndian)
		{
			if(i == size - 1)
				break;
			i++;
		}
		else
		{
			if(i == 0)
				break;
			i--;
		}
	}
}

bool IsNumber(char *str)
{
	int i = 0;
	while (str[i] != 0)
	{
		if (!(str[i] >= '0' && str[i] <= '9'))
		{
			return 0;
		}
		i++;
	}
	return 1;
}

void RemoveLineBreaks(char *str)
{
	int i = 0;
	while (str[i] != 0)
	{
		if (str[i] == '\r' || str[i] == '\n')
		{
			str[i] = 0;
			return;
		}
		i++;
	}
	return;
}

int SymbolCountInString(char *str, char symbol)
{
	int count = 0, i = 0;
	while (str[i] != 0)
	{
		if (str[i] == symbol)
			count++;
		i++;
	}
	return count;
}

//Separate a string into 2 based on the first occurence of a symbol (it does not keep the separator in either string). Returns true if it found separator
bool SeparateString(char *str, char *str2, char seperator)
{
	bool copy = 0;
	int i = 0, j = 0;
	while(str[i] != 0)
	{
		if(copy)
			str2[j] = str[i], j++;
		if(str[i] == seperator)
			copy = 1, str[i] = 0;
		i++;
	}
	str2[j] = 0;
	return copy;
}

unsigned long long StringToULongLong(char *string)
{
	unsigned long long number = 0, num = 1;
	int length = (int) strlen(string);
	for(int i = length - 1; i >= 0; i--)
	{
		number += (string[i] - '0') * num;
		num *= 10;
	}
	return number;
}

char *ReturnStringPos(char *find, char *in) //Find position of "find" string within "in" string. Does a lower case comparison.
{
	int findPos = 0, inPos = 0, returnPos = 0;
	while (in[inPos] != 0)
	{
		if (tolower(in[inPos]) == tolower(find[findPos]))
		{
			if (findPos == 0)
				returnPos = inPos;
			findPos++;
			if (find[findPos] == 0)
				return &in[returnPos];
		}
		else
		{
			findPos = 0;
			if (returnPos > 0)
			{
				inPos = returnPos + 1;
				returnPos = 0;
			}
		}
		inPos++;
	}
	return 0;
}