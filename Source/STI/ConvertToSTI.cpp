#include <stdio.h>
#include <tchar.h>
#include "..\misc.h"
#include "STIformat.h"
#include "..\lodepng\lodepng.h"

//offsetX and offsetY are values saved to the STI sub image headers
//If imageHeight is 0 we assume the PNG is one image. If it's a specific height, we assume the PNG consists of multiple images stacked vertically where each one has the same width and imageHeight as height (we automatically calculate image count)
bool ConvertToSTI(char *filename, int offsetX, int offsetY, int subImageHeight)
{
	//Filename without path
	char *filename_short;
	{
		int lastSlash = -1;
		int pos = 0;
		while(filename[pos] != 0)
		{
			if(filename[pos] == '\\' || filename[pos] == '/')
				lastSlash = pos;
			pos++;
		}

		if(lastSlash > 0)
		{
			if(filename[lastSlash + 1] != 0)
				filename_short = &filename[lastSlash + 1];
			else
				filename_short = filename;
		}
		else
			filename_short = filename;
	}

	//Extension
	int lastDot = LastDot(filename);
	if(lastDot == 0)
	{
		printf("Error: Failed to acquire file extension for %s\n", filename_short);
		return 0;
	}
	char *ext = &filename[lastDot + 1];
	if(_stricmp(ext, "png") != 0)
	{
		printf("Error: Expected PNG as input but extension is different for %s\n", filename_short);
		return 0;
	}

	//Open file
	unsigned char *fileData;
	unsigned int fileDataSize;
	if(!ReadFile(filename, &fileData, &fileDataSize))
	{
		printf("Error: Failed to open %s for reading.\n", filename_short);
		return 0;
	}

	//Decode PNG
	unsigned char *imgData = 0;
	unsigned int imgWidth = 0, imgHeight = 0;
	LodePNGState pngState;
	lodepng_state_init(&pngState);
	pngState.decoder.color_convert = 0;
	if(lodepng_decode(&imgData, &imgWidth, &imgHeight, &pngState, fileData, fileDataSize) != 0)
	{
		printf("Error: Failed to decode PNG file %s\n", filename_short);
		delete[]fileData;
		return 0;
	}
	delete[]fileData;
	if(pngState.info_raw.bitdepth != 8 || pngState.info_raw.colortype != LCT_PALETTE)
	{
		printf("Error: PNG file is not 8-bit per colour channel or isn't using a palette (%s)\n", filename_short);
		free(imgData);
		lodepng_state_cleanup(&pngState);
		return 0;
	}
	unsigned int imgDataSize = imgWidth * imgHeight;

	//Calculate quantity of sub images
	int subImageCount = 1;
	if(subImageHeight != 0)
	{
		if(imgHeight % subImageHeight != 0)
		{
			printf("Error: Height of PNG file is invalid. It should divide evenly with supplied height value of %i. Height of %s is %u\n", subImageHeight, filename_short, imgHeight);
			free(imgData);
			lodepng_state_cleanup(&pngState);
			return 0;
		}
		subImageCount = imgHeight / subImageHeight;
	}
	else
		subImageHeight = imgHeight;

	//STI header
	stiHeader_s stiHeader;
	memset(&stiHeader, 0, sizeof(stiHeader_s));
	stiHeader.magic = STI_MAGIC;
	stiHeader.originalSize = imgDataSize;
	stiHeader.imageDataSize = 0; //Will be updated later in this function after we've processed all 
	stiHeader.transparentValue = 0; //TODO: Should we always set this to be 0?
	stiHeader.flags = STIFLAG_INDEXED; //TODO: Add STIFLAG_ETRLE_COMPRESSED once we support ETRLE compression
	stiHeader.width = imgWidth;
	stiHeader.height = imgHeight;
	stiHeader.Indexed.colourCount = 256;
	stiHeader.Indexed.subImageCount = subImageCount;
	stiHeader.Indexed.redDepth = 8;
	stiHeader.Indexed.greenDepth = 8;
	stiHeader.Indexed.blueDepth = 8;
	stiHeader.colourDepth = 8;
	stiHeader.appDataSize = sizeof(stiSubImage_s) * subImageCount;

	//Sub image headers (these will be filled with data as we process each sub image)
	stiSubImage_s *subImageHeader = new stiSubImage_s[subImageCount];
	memset(subImageHeader, 0, sizeof(stiSubImage_s));

	//Path for STI file
	char stiPath[_MAX_PATH];
	strcpy_s(stiPath, filename);
	stiPath[lastDot + 1] = 'S';
	stiPath[lastDot + 2] = 'T';
	stiPath[lastDot + 3] = 'I';
	stiPath[lastDot + 4] = 0;

	//Create STI file
	FILE *stiFile = 0;
	fopen_s(&stiFile, stiPath, "wb");
	if(!stiFile)
	{
		printf("Error: Failed to open %s for writing.\n", stiPath);
		delete[]subImageHeader;
		free(imgData);
		lodepng_state_cleanup(&pngState);
		return 0;
	}

	//Write header (we'll re-write this later with updated data)
	fwrite(&stiHeader, sizeof(stiHeader_s), 1, stiFile);

	//Copy and write palette
	paletteElement_s palette[PALETTEENTRIES];
	memset(palette, 0, sizeof(paletteElement_s) * PALETTEENTRIES);
	for(int i = 0; i < pngState.info_raw.palettesize; i++)
	{
		palette[i].r = pngState.info_raw.palette[(i * 4) + 0];
		palette[i].g = pngState.info_raw.palette[(i * 4) + 1];
		palette[i].b = pngState.info_raw.palette[(i * 4) + 2];
	}
	fwrite(palette, sizeof(paletteElement_s), PALETTEENTRIES, stiFile);

	//Write sub image headers (we'll re-write this later with updated data)
	fwrite(subImageHeader, sizeof(stiSubImage_s), subImageCount, stiFile);

	//Start adding sub images
	unsigned int imageDataAdded = 0;
	unsigned int currentOffsetInSTIFile = 0;
	unsigned int currentOffsetInImgData = 0;
	for(int i = 0; i < subImageCount; i++)
	{
		subImageHeader[i].offset = currentOffsetInSTIFile;
		subImageHeader[i].size = subImageHeight * imgWidth;
		subImageHeader[i].offsetX = offsetX;
		subImageHeader[i].offsetY = offsetY;
		subImageHeader[i].height = subImageHeight;
		subImageHeader[i].width = imgWidth;
		fwrite(&imgData[currentOffsetInImgData], subImageHeader[i].size, 1, stiFile);
		currentOffsetInSTIFile += subImageHeader[i].size;
		currentOffsetInImgData += subImageHeader[i].size;
	}

	//Re-write header
	fseek(stiFile, 0, SEEK_SET);
	stiHeader.imageDataSize = currentOffsetInSTIFile;
	fwrite(&stiHeader, sizeof(stiHeader_s), 1, stiFile);

	//Re-write sub image headers
	fseek(stiFile, sizeof(stiHeader_s) + (sizeof(paletteElement_s) * PALETTEENTRIES), SEEK_SET);
	fwrite(subImageHeader, sizeof(stiSubImage_s), subImageCount, stiFile);

	//Finish and clean up
	printf("Wrote %s\n", stiPath);
	fclose(stiFile);
	delete[]subImageHeader;
	free(imgData);
	lodepng_state_cleanup(&pngState);
	return 1;
}