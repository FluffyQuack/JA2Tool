#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include "extract.h"
#include "create.h"
#include "misc.h"
#include "dir.h"
#include "STI\ConvertFromSTI.h"
#include "STI\ConvertToSTI.h"

enum
{
	MODE_NOTHING,
	MODE_EXTRACT,
	MODE_LISTFILES,
	//MODE_REPACK,
	MODE_SINGLEFILE_STI,
	MODE_SINGLEFILE_PNG,
};

bool convertFromSTI = 0;
char *pngWithPalette = 0;

void HelpText()
{
	printf("JA2 modding tool v0.1 (WIP) (by patreon.com/FluffyQuack)\n\n");
	printf("usage: retool [options]\n");
	printf("  -x [sfl]		Unpack SLF archive\n");
	//printf("  -c [folder]		Create a SLF file from directory\n");
	printf("  -l [pak]		List files in SLF archive\n");
	printf("  -sti			Convert STI files during extraction\n");
}

int _tmain(int argc, _TCHAR *argv[])
{
	//Defaults
	int mode = MODE_NOTHING;
	int offsetX = 0, offsetY = 0, subImageHeight = 0; //Used for PNG to STI conversion

	//Process command line arguments
	int i = 1, strcount = 0;
	char *str1 = 0, *str2 = 0;
	while(argc > i)
	{
		if (argv[i][0] == '-')
		{
			if (_stricmp(argv[i], "-x") == 0)
				mode = MODE_EXTRACT;
			//else if (_stricmp(argv[i], "-c") == 0)
				//mode = MODE_REPACK;
			else if (_stricmp(argv[i], "-l") == 0)
				mode = MODE_LISTFILES;
			else if (_stricmp(argv[i], "-sti") == 0)
				convertFromSTI = 1;
			else if (_stricmp(argv[i], "-subImageHeight") == 0 || _stricmp(argv[i], "-imageHeight") == 0 || _stricmp(argv[i], "-height") == 0)
			{
				if (argc > i)
				{
					subImageHeight = atoi(argv[i + 1]);
					i++;
				}
			}
			else if (_stricmp(argv[i], "-offsets") == 0)
			{
				if (argc > i + 1)
				{
					offsetX = atoi(argv[i + 1]);
					i++;
					offsetY = atoi(argv[i + 1]);
					i++;
				}
			}
			else if (_stricmp(argv[i], "-palette") == 0)
			{
				if (argc > i)
				{
					pngWithPalette = argv[i + 1];
					i++;
				}
			}
		}
		else
		{
			if(strcount == 0)
				str1 = argv[i];
			else if(strcount == 1)
				str2 = argv[i];
			strcount++;
		}
		i++;
	}

	Dir_Init();

	if(mode == MODE_NOTHING && str1) //Try to determine mode if no mode is specified but we have one string (potentially file or directory name)
	{
		int fileType = CheckFileType(str1);

		if(fileType == FILETYPE_SLF)
			mode = MODE_EXTRACT;
		else if(fileType == FILETYPE_STI)
			mode = MODE_SINGLEFILE_STI;
		else if(fileType == FILETYPE_PNG)
			mode = MODE_SINGLEFILE_PNG;
	}

	if(mode == MODE_EXTRACT && str1)
	{
		ExtractSLF(str1);
	}
	/*else if(mode == MODE_REPACK && str1)
	{
		CreateSLF(str1);
	}*/
	else if(mode == MODE_LISTFILES && str1)
	{
		ListFilesInSLF(str1);
	}
	else if(mode == MODE_SINGLEFILE_STI && str1)
	{
		ConvertFromSTI(str1);
	}
	else if(mode == MODE_SINGLEFILE_PNG && str1)
	{
		ConvertToSTI(str1, offsetX, offsetY, subImageHeight);
	}
	else
		mode = MODE_NOTHING;

	if(mode == MODE_NOTHING)
		HelpText();

	Dir_Deinit();
	return 1;
}