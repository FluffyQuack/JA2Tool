#include <stdio.h>
#include <tchar.h>
#include "..\misc.h"
#include "STIformat.h"
#include "..\lodepng\lodepng.h"
#include "..\ja2tool.h"

static bool saveWithCompression = 1;
static bool autoTrim = 1;

//offsetX and offsetY are values saved to the STI sub image headers
//If imageHeight is 0 we assume the PNG is one image. If it's a specific height, we assume the PNG consists of multiple images stacked vertically where each one has the same width and imageHeight as height (we automatically calculate image count)
bool ConvertToSTI(char *filename, int offsetX, int offsetY, int subImageHeight)
{
	//Init a bunch of variables
	int animationLength = 0;
	unsigned int currentOffsetInImgData = 0;
	unsigned int currentOffsetInSTIFile = 0;
	unsigned int imageDataAdded = 0;
	FILE *stiFile = 0;
	AuxObjectData_s *auxData = 0;
	stiSubImage_s *subImageHeader = 0;
	int subImageCount = 0;
	size_t pngPaletteSize = 0;
	unsigned char *pngPalette = 0;
	bool schedulePNGStateForDeletion = 0;
	bool schedulePNGPaletteStateForDeletion = 0;
	unsigned int imgDataSize = 0;
	unsigned char *imgData = 0;
	unsigned int imgWidth = 0, imgHeight = 0;
	char *ext = 0;
	unsigned char *fileData = 0;
	unsigned int fileDataSize = 0;
	bool success = 1;
	
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
		success = 0;
		goto stop;
	}
	ext = &filename[lastDot + 1];
	if(_stricmp(ext, "png") != 0)
	{
		printf("Error: Expected PNG as input but extension is different for %s\n", filename_short);
		success = 0;
		goto stop;
	}

	//Open file
	fileData = 0;
	fileDataSize = 0;
	if(!ReadFile(filename, &fileData, &fileDataSize))
	{
		printf("Error: Failed to open %s for reading.\n", filename_short);
		success = 0;
		goto stop;
	}

	//Decode PNG
	imgData = 0;
	imgWidth = 0, imgHeight = 0;
	LodePNGState pngState;
	lodepng_state_init(&pngState);
	schedulePNGStateForDeletion = 1;
	pngState.decoder.color_convert = 0;
	if(lodepng_decode(&imgData, &imgWidth, &imgHeight, &pngState, fileData, fileDataSize) != 0)
	{
		printf("Error: Failed to decode PNG file %s\n", filename_short);
		success = 0;
		goto stop;
	}
	delete[]fileData;
	fileData = 0;
	if(pngState.info_raw.bitdepth != 8 || pngState.info_raw.colortype != LCT_PALETTE)
	{
		printf("Error: PNG file is not 8-bit per colour channel or isn't using a palette (%s)\n", filename_short);
		success = 0;
		goto stop;
	}
	imgDataSize = imgWidth * imgHeight;

	LodePNGState pngState_palette;
	if(pngWithPalette) //If pngWithPalette exists, we'll try to load this PNG and use this as palette (this will fail if the other PNG data contains colours that don't match)
	{
		//Open palette file
		if(!ReadFile(pngWithPalette, &fileData, &fileDataSize))
		{
			printf("Error: Failed to open %s for reading.\n", pngWithPalette);
			success = 0;
			goto stop;
		}

		//Decode PNG
		lodepng_state_init(&pngState_palette);
		schedulePNGPaletteStateForDeletion = 1;
		pngState_palette.decoder.color_convert = 0;
		unsigned char *imgData_palette = 0;
		unsigned int imgWidth_palette = 0, imgHeight_palette = 0;
		if(lodepng_decode(&imgData_palette, &imgWidth_palette, &imgHeight_palette, &pngState_palette, fileData, fileDataSize) != 0)
		{
			printf("Error: Failed to decode PNG file %s\n", pngWithPalette);
			success = 0;
			goto stop;
		}
		free(imgData_palette);
		delete[]fileData;
		fileData = 0;
		if(pngState_palette.info_raw.bitdepth != 8 || pngState_palette.info_raw.colortype != LCT_PALETTE)
		{
			printf("Error: PNG file is not 8-bit per colour channel or isn't using a palette (%s)\n", pngWithPalette);
			success = 0;
			goto stop;
		}

		//Update image data in primary loaded PNG so colour indices reflect the new palette
		unsigned char r, g, b, r2, g2, b2;
		for(unsigned int i = 0; i < imgDataSize; i++)
		{
			r = pngState.info_raw.palette[(imgData[i] * 4) + 0];
			g = pngState.info_raw.palette[(imgData[i] * 4) + 1];
			b = pngState.info_raw.palette[(imgData[i] * 4) + 2];

			//Find same colour in new palette
			int newIndex = -1;
			for(unsigned int j = 0; j < pngState_palette.info_raw.palettesize; j++)
			{
				r2 = pngState_palette.info_raw.palette[(j * 4) + 0];
				g2 = pngState_palette.info_raw.palette[(j * 4) + 1];
				b2 = pngState_palette.info_raw.palette[(j * 4) + 2];

				if(r2 == r && g2 == g && b2 == b)
				{
					newIndex = j;
					break;
				}
			}
			if(newIndex == -1)
			{
				printf("Error: At least one colour could not be found in palette: %s\n", pngWithPalette);
				success = 0;
				goto stop;
			}

			//Update entry
			imgData[i] = newIndex;
		}

		pngPalette = pngState_palette.info_raw.palette;
		pngPaletteSize = pngState_palette.info_raw.palettesize;
	}
	else
	{
		pngPalette = pngState.info_raw.palette;
		pngPaletteSize = pngState.info_raw.palettesize;
	}

	//Copy palette
	paletteElement_s palette[PALETTEENTRIES];
	memset(palette, 0, sizeof(paletteElement_s) * PALETTEENTRIES);
	for(int i = 0; i < pngPaletteSize; i++)
	{
		palette[i].r = pngPalette[(i * 4) + 0];
		palette[i].g = pngPalette[(i * 4) + 1];
		palette[i].b = pngPalette[(i * 4) + 2];
	}

	//A bit hacky, but we find the colour used for shadow and then swap its palette index with something else (this requires changes to both image data and palette)
	for(unsigned int j = 0; j < PALETTEENTRIES; j++)
	{
		if(palette[j].r == 175 && palette[j].g == 187 && palette[j].b == 0)
		{
			if(j == 254) //If entry is already #254, then shadow palette entry is already in the correct spot
				break;

			//Swap palette colours
			unsigned char r = palette[254].r;
			unsigned char g = palette[254].g;
			unsigned char b = palette[254].b;
			palette[254].r = palette[j].r;
			palette[254].g = palette[j].g;
			palette[254].b = palette[j].b;
			palette[j].r = r;
			palette[j].g = g;
			palette[j].b = b;

			//Swap indices in image data
			for(unsigned int i = 0; i < imgDataSize; i++)
			{
				if(imgData[i] == 254)
					imgData[i] = j;
				else if(imgData[i] == j)
					imgData[i] = 254;
			}
		}
	}

	//Calculate quantity of sub images
	subImageCount = 1;
	if(subImageHeight != 0) //subImageHeight is defined, so we calculate quantity of sub images by looking at the height of PNG image
	{
		if(imgHeight % subImageHeight != 0)
		{
			printf("Error: Height of PNG file is invalid. It should divide evenly with supplied height value of %i. Height of %s is %u\n", subImageHeight, filename_short, imgHeight);
			success = 0;
			goto stop;
		}
		subImageCount = imgHeight / subImageHeight;
	}
	else //subImageHeight is not supplied, so we assume the entire PNG image is supposed represent one image
		subImageHeight = imgHeight;

	//STI header
	stiHeader_s stiHeader;
	memset(&stiHeader, 0, sizeof(stiHeader_s));
	stiHeader.magic = STI_MAGIC;
	stiHeader.originalSize = imgDataSize;
	stiHeader.imageDataSize = 0; //Will be updated later in this function after we've processed all image data
	stiHeader.transparentValue = 0; //TODO: Should we always set this to be 0?
	stiHeader.flags = STIFLAG_INDEXED | STIFLAG_TRANSPARENT;
	if(saveWithCompression)
		stiHeader.flags |= STIFLAG_ETRLE_COMPRESSED;
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
	subImageHeader = new stiSubImage_s[subImageCount];
	memset(subImageHeader, 0, sizeof(stiSubImage_s));

	//Path for STI file
	char stiPath[_MAX_PATH];
	strcpy_s(stiPath, filename);
	stiPath[lastDot + 1] = 'S';
	stiPath[lastDot + 2] = 'T';
	stiPath[lastDot + 3] = 'I';
	stiPath[lastDot + 4] = 0;

	//Create STI file
	stiFile = 0;
	fopen_s(&stiFile, stiPath, "wb");
	if(!stiFile)
	{
		printf("Error: Failed to open %s for writing.\n", stiPath);
		success = 0;
		goto stop;
	}

	//Write header (we'll re-write this later with updated data)
	fwrite(&stiHeader, sizeof(stiHeader_s), 1, stiFile);

	//Write palette
	fwrite(palette, sizeof(paletteElement_s), PALETTEENTRIES, stiFile);

	//Write sub image headers (we'll re-write this later with updated data)
	fwrite(subImageHeader, sizeof(stiSubImage_s), subImageCount, stiFile);

	//Start adding sub images
	imageDataAdded = 0;
	currentOffsetInSTIFile = 0;
	currentOffsetInImgData = 0;
	for(int i = 0; i < subImageCount; i++)
	{
		bool deleteSubImgDataWhenDone = 0;
		unsigned char *subImgData = 0;
		unsigned long long subImgDataSize;
		unsigned int subImgX, subImgY, trimX, trimY;
		if(autoTrim) //Edit the input image and trim down each side to first occurance of opaque pixels
		{
			//Find first occurance of opaque pixels on each side of image
			unsigned int x1 = imgWidth, x2 = 0, y1 = subImageHeight, y2 = 0, curX = 0, curY = 0;
			unsigned long long curPos = currentOffsetInImgData, endPos = currentOffsetInImgData + (subImageHeight * imgWidth);
			while(curPos < endPos)
			{
				if(imgData[curPos] != 0) //TODO: Should we always treat index #0 in palette as transparent?
				{
					if(x1 > curX) x1 = curX;
					if(y1 > curY) y1 = curY;
					if(x2 < curX) x2 = curX;
					if(y2 < curY) y2 = curY;
				}
				curPos++;
				curX++;
				if(curX >= imgWidth)
				{
					curX = 0;
					curY++;
				}
			}
			if(x1 > x2 || y1 > y2)
			{
				x1 = 0;
				y1 = 0;
				x2 = 0;
				y2 = 0;
			}

			//Create new image data
			unsigned int newWidth = (x2 - x1) + 1;
			unsigned int newHeight = (y2 - y1) + 1;
			unsigned int newImgDataSize = newWidth * newHeight;
			unsigned char *newImgData = new unsigned char[newImgDataSize];

			//Copy to new image data
			curPos = currentOffsetInImgData;
			curX = 0;
			curY = 0;
			unsigned int curPosInNewData = 0;
			while(curPos < endPos)
			{
				if(curX >= x1 && curY >= y1 && curX <= x2 && curY <= y2)
				{
					newImgData[curPosInNewData] = imgData[curPos];
					curPosInNewData++;
				}
				curPos++;
				curX++;
				if(curX >= imgWidth)
				{
					curX = 0;
					curY++;
					if(curY > y2)
						break;
				}
			}

			//Error handling
			if(curPosInNewData != newImgDataSize)
			{
				printf("Error: Failed to trim image data in input file %s", filename_short);
				delete[]newImgData;
				success = 0;
				goto stop;
			}

			//Update sub image variables based on trimmed image
			deleteSubImgDataWhenDone = 1;
			subImgData = newImgData;
			subImgDataSize = newImgDataSize;
			subImgX = newWidth;
			subImgY = newHeight;
			trimX = x1;
			trimY = y1;
		}
		else
		{
			//Update sub image variables based on non-trimmed image
			deleteSubImgDataWhenDone = 0;
			subImgData = &imgData[currentOffsetInImgData];
			subImgDataSize = subImageHeight * imgWidth;
			subImgX = imgWidth;
			subImgY = subImageHeight;
			trimX = 0;
			trimY = 0;
		}

		subImageHeader[i].offset = currentOffsetInSTIFile;
		subImageHeader[i].offsetX = offsetX + trimX;
		subImageHeader[i].offsetY = offsetY + trimY;
		subImageHeader[i].width = subImgX;
		subImageHeader[i].height = subImgY;
		
		if(saveWithCompression) //ETRLE compression
		{
			unsigned char *compressedData = new unsigned char[subImgX * subImgY * 2];
			memset(compressedData, 0, subImgX * subImgY * 2);
			unsigned int compressedDataPos = 0;
			unsigned int imgDataPos = 0;
			unsigned int currentStridePos = 0;
			unsigned int currentHeight = 0;
			bool error = 0;
			
			while(currentHeight < subImgY)
			{
				//Error handling
				if(currentStridePos > subImgX)
				{
					printf("Error: Stride pos exceeded stride when compressing %s\n", stiPath);
					error = 1;
					break;
				}

				//If we've hit stride, then add a zero byte
				if(currentStridePos == subImgX)
				{
					compressedData[compressedDataPos] = 0;
					compressedDataPos++;
					currentStridePos = 0;
					currentHeight++;
					continue;
				}

				//Count how many transparent or opaque pixels follow from this position
				bool transparent = 0;
				int count = 0;
				for(int k = 0; k + currentStridePos < subImgX && k < NUMBER_OF_BYTES_MASK; k++)
				{
					if(k == 0) //Check if pixel is transparent or opaque
					{
						if(subImgData[imgDataPos] == 0) //TODO: Right now we're assuming palette entry #0 is always transparent. Should we change that behaviour somehow?
							transparent = 1;
						else
							transparent = 0;
					}
					else //Check if next pixel is same type
					{
						if((transparent && subImgData[imgDataPos + k] != 0)
							|| (!transparent && subImgData[imgDataPos + k] == 0))
							break;
					}
					count++;
				}

				//Error handling
				if(count == 0)
				{
					printf("Error: Processed line as being size 0 in %s\n", filename_short);
					error = 1;
					break;
				}

				//Write byte with the length of pixel sequence
				compressedData[compressedDataPos] = count;
				compressedDataPos++;

				if(transparent) //Update control byte to indicate this is a sequence of transparent pixels
				{
					compressedData[compressedDataPos - 1] |= IS_COMPRESSED_BYTE_MASK;
				}
				else //Write bytes corresponding to the amount of opaque pixels we found
				{
					memcpy(&compressedData[compressedDataPos], &subImgData[imgDataPos], count);
					compressedDataPos += count;
				}
				imgDataPos += count;
				currentStridePos += count;
			}

			if(imgDataPos != subImgX * subImgY)
			{
				printf("Error: Image data position doesn't match size of image after processing a sub image in %s\n", filename_short);
				error = 1;
				break;
			}

			if(error)
			{
				if(deleteSubImgDataWhenDone)
					delete[]subImgData;
				delete[]compressedData;
				success = 0;
				goto stop;
			}

			subImageHeader[i].size = compressedDataPos;
			fwrite(compressedData, compressedDataPos, 1, stiFile);
			delete[]compressedData;
		}
		else //No compression
		{
			subImageHeader[i].size = subImgX * subImgY;
			fwrite(subImgData, subImageHeader[i].size, 1, stiFile);
		}

		currentOffsetInSTIFile += subImageHeader[i].size;
		currentOffsetInImgData += subImageHeight * imgWidth;
		if(deleteSubImgDataWhenDone)
			delete[]subImgData;
		deleteSubImgDataWhenDone = 0;
	}
	if(currentOffsetInImgData != imgDataSize)
	{
		printf("Error: Failed to process image data. Did not reach end of image data in input file when writing %s (file was written, but it is corrupt)\n", stiPath);
		success = 0;
		goto stop;
	}

	//Create and write the aux data at the end of the file
	auxData = new AuxObjectData_s[subImageCount];
	memset(auxData, 0, sizeof(AuxObjectData_s) * subImageCount);
	animationLength = subImageCount / 8;
	for(int i = 0; i < subImageCount; i++)
	{
		if(i % subImageCount == 0)
		{
			auxData[i].ubNumberOfFrames = animationLength;
			auxData[i].fFlags = STIAUX_ANIMATED_TILE;
		}
	}
	fwrite(auxData, sizeof(AuxObjectData_s) * subImageCount, 1, stiFile);
	delete[]auxData;
	auxData = 0;

	//Re-write header
	fseek(stiFile, 0, SEEK_SET);
	stiHeader.imageDataSize = currentOffsetInSTIFile;
	fwrite(&stiHeader, sizeof(stiHeader_s), 1, stiFile);

	//Re-write sub image headers
	fseek(stiFile, sizeof(stiHeader_s) + (sizeof(paletteElement_s) * PALETTEENTRIES), SEEK_SET);
	fwrite(subImageHeader, sizeof(stiSubImage_s), subImageCount, stiFile);

	//Finish and clean up
	printf("Wrote %s\n", stiPath);
stop:
	if(stiFile)
		fclose(stiFile);
	if(subImageHeader)
		delete[]subImageHeader;
	if(imgData)
		free(imgData);
	if(schedulePNGStateForDeletion)
		lodepng_state_cleanup(&pngState);
	if(schedulePNGPaletteStateForDeletion)
		lodepng_state_cleanup(&pngState_palette);
	if(fileData)
		delete[]fileData;
	return success;
}