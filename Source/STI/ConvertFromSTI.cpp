#include <stdio.h>
#include <tchar.h>
#include "..\misc.h"
#include "STIformat.h"
#include "..\lodepng\lodepng.h"

static bool stiAddBorderBasedOnOffset = 0;
static bool stiAddBorderForSubImages = 0;
static bool stiRenderSTIToMiddleOfCanvas = 0;

static bool WriteBMP(char *filename, unsigned char *imgData, unsigned int imgDataSize, int width, int height, unsigned int bitDepth, bool subImages = 0, unsigned subImageNum = 0, paletteElement_s *palette = 0, int offsetX = 0, int offsetY = 0, unsigned int redMask = 0, unsigned int greenMask = 0, unsigned int blueMask = 0)
{
	//Generate path
	char BMPPath[MAXPATH];
	memset(BMPPath, 0, MAXPATH);
	char filenameWithoutExtension[MAXPATH];
	strcpy(filenameWithoutExtension, filename);
	int firstDot = FirstDot(filenameWithoutExtension);
	if(firstDot != 0)
		filenameWithoutExtension[firstDot] = 0;
	if(subImages == 0)
		sprintf(BMPPath, "%s.bmp", filenameWithoutExtension);
	else
		sprintf(BMPPath, "%s_%03i.bmp", filenameWithoutExtension, subImageNum);

	if(bitDepth != 8 && bitDepth != 16)
	{
		printf("Error: Unsupported bit depth %u in %s.\n", bitDepth, BMPPath);
		return 0;
	}

	//Important note: BMP format requires each row to be a multiple of 4. If a stride doesn't add to a multiple of 4, then we round up the stride by adding padding.
	unsigned int imgDataSizeInBMP = imgDataSize;
	int padding = 0;
	if(bitDepth == 16 && (width * 2) % 4 != 0)
		padding = 4 - ((width * 2) % 4);
	else if(bitDepth == 8 && width % 4 != 0)
		padding = 4 - (width % 4);
	imgDataSizeInBMP += padding * height;

	//BMP header
	bmpHeader_s bmpHeader;
	bmpHeader.bfType = BMP_MAGIC;
	if(bitDepth == 16)
	{
		bmpHeader.bfSize = sizeof(bmpHeader_s) + sizeof(bmpBitfieldMasks_s) + imgDataSizeInBMP;
		bmpHeader.bfOffBits = sizeof(bmpHeader_s) + sizeof(bmpBitfieldMasks_s);
	}
	else if(bitDepth == 8)
	{
		bmpHeader.bfSize = sizeof(bmpHeader_s) + (sizeof(bmpColorMapEntry_s) * PALETTEENTRIES) + imgDataSizeInBMP;
		bmpHeader.bfOffBits = sizeof(bmpHeader_s) + (sizeof(bmpColorMapEntry_s) * PALETTEENTRIES);
	}
	bmpHeader.bfReserved1 = 0;
	bmpHeader.bfReserved2 = 0;

	//Info header
	bmpHeader.biSize = 40;
	bmpHeader.biWidth = width;
	bmpHeader.biHeight = -height;
	bmpHeader.biPlanes = 1;
	bmpHeader.biBitCount = bitDepth;
	if(bitDepth == 16)
	{
		bmpHeader.biCompression = BMPC_BITFIELDS;
		bmpHeader.biClrUsed = 0;
	}
	else if(bitDepth == 8)
	{
		bmpHeader.biCompression = BMPC_RGB;
		bmpHeader.biClrUsed = PALETTEENTRIES;
	}
	bmpHeader.biSizeImage = imgDataSizeInBMP;
	bmpHeader.biXPelsPerMeter = 10000;
	bmpHeader.biYPelsPerMeter = 10000;
	bmpHeader.biClrImportant = 0;

	bmpBitfieldMasks_s bitFieldMask;
	bmpColorMapEntry_s bmpPalette[PALETTEENTRIES];
	if(bitDepth == 16)
	{
		//Since we use bitfield "compression", we have colour masks here
		bitFieldMask.r = redMask;
		bitFieldMask.g = greenMask;
		bitFieldMask.b = blueMask;
	}
	else if(bitDepth == 8)
	{
		//We need to include color map here aka palette
		for(int i = 0; i < PALETTEENTRIES; i++)
		{
			bmpPalette[i].rgbRed = palette[i].r;
			bmpPalette[i].rgbGreen = palette[i].g;
			bmpPalette[i].rgbBlue = palette[i].b;
			bmpPalette[i].rgbReserved = 0;
		}
	}

	//Save BMP as file
	FILE *file;
	fopen_s(&file, BMPPath, "wb");
	if(!file)
	{
		printf("Error: Couldn't open %s for writing.\n", BMPPath);
		return 0;
	}
	fwrite(&bmpHeader, 1, sizeof(bmpHeader_s), file);
	if(bitDepth == 16)
		fwrite(&bitFieldMask, 1, sizeof(bmpBitfieldMasks_s), file);
	else if(bitDepth == 8)
		fwrite(bmpPalette, sizeof(bmpColorMapEntry_s), PALETTEENTRIES, file);
	if(padding) //If there's padding, then we write rows one at a time
	{
		unsigned char nullBytes[4] = {0,0,0,0};
		for(int y = 0; y < height; y++)
		{
			if(bitDepth == 16)
				fwrite(&imgData[width * 2 * y], 1, width, file);
			else if(bitDepth == 8)
				fwrite(&imgData[width * y], 1, width, file);
			fwrite(nullBytes, 1, padding, file);
		}
	}
	else 
		fwrite(imgData, 1, imgDataSize, file);
	fclose(file);
	if(bitDepth == 16)
		printf("Wrote %s (size: %i/%i)\n", BMPPath, width, height);
	else if(bitDepth == 8)
		printf("Wrote %s (size: %i/%i, offset: %i/%i)\n", BMPPath, width, height, offsetX, offsetY);

	return 1;
}

bool ConvertFromSTI(char *filename)
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

	//Open file
	unsigned char *data;
	unsigned int dataSize;
	if(!ReadFile(filename, &data, &dataSize))
	{
		printf("Error: Failed to open %s for reading.\n", filename_short);
		return 0;
	}

	//Check magic
	if((unsigned int &) data[0] != STI_MAGIC)
	{
		printf("Error: Incorrect magic in %s\n", filename_short);
		delete[]data;
		return 0;
	}

	//Header
	stiHeader_s *stiHeader = (stiHeader_s *) data;

	//Handle variants
	if(stiHeader->flags & STIFLAG_RGB) //16-bit RGB(A) image
	{
		if(stiHeader->flags & STIFLAG_ZLIB_COMPRESSED || stiHeader->flags & STIFLAG_ETRLE_COMPRESSED)
		{
			printf("Error: Unsupported compression in %s\n", filename_short);
			delete[]data;
			return 0;
		}

		if(!WriteBMP(filename, &data[sizeof(stiHeader_s)], stiHeader->imageDataSize, stiHeader->width, stiHeader->height, 16, 0, 0, 0, 0, 0, stiHeader->RGB.redMask, stiHeader->RGB.greenMask, stiHeader->RGB.blueMask))
		{
			delete[]data;
			return 0;
		}
	}
	else if(stiHeader->flags & STIFLAG_INDEXED) //8-bit indexed image
	{
		if(stiHeader->flags & STIFLAG_ZLIB_COMPRESSED)
		{
			printf("Error: Unsupported compression in %s\n", filename_short);
			delete[]data;
			return 0;
		}

		//Read palette
		int paletteSize = 3 * PALETTEENTRIES;
		paletteElement_s palette[PALETTEENTRIES];
		memcpy(palette, &data[sizeof(stiHeader_s)], paletteSize);
		unsigned long long offset = sizeof(stiHeader_s) + paletteSize;
		stiSubImage_s *subImageHeader = (stiSubImage_s *) &data[offset];
		offset += sizeof(stiSubImage_s) * stiHeader->Indexed.subImageCount;

		for(int subImageNum = 0; subImageNum < stiHeader->Indexed.subImageCount; subImageNum++)
		{
			unsigned int imgDataSize = subImageHeader[subImageNum].width * subImageHeader[subImageNum].height;
			unsigned char *imgData;

			//Handle RLE compression
			if(stiHeader->flags & STIFLAG_ETRLE_COMPRESSED)
			{
				imgData = new unsigned char[imgDataSize];
				unsigned int posFrom = sizeof(stiHeader_s) + paletteSize + (sizeof(stiSubImage_s) * stiHeader->Indexed.subImageCount) + subImageHeader[subImageNum].offset;
				offset += subImageHeader[subImageNum].size;
				unsigned int posTo = 0;

#define ALPHA_VALUE 0
#define IS_COMPRESSED_BYTE_MASK 0x80
#define NUMBER_OF_BYTES_MASK 0x7F

				int bytes_til_next_control_byte = 0;
				unsigned int progress;
				for(progress = 0; progress < subImageHeader[subImageNum].size; progress++, posFrom++)
				{
					if(bytes_til_next_control_byte == 0)
					{
						bool is_compressed_alpha_byte = ((data[posFrom] & IS_COMPRESSED_BYTE_MASK) >> 7) == 1;
						int length_of_subsequence = data[posFrom] & NUMBER_OF_BYTES_MASK;
						if(is_compressed_alpha_byte)
						{
							for(int i = 0; i < length_of_subsequence; i++)
							{
								if(posTo >= imgDataSize)
									goto stop;
								imgData[posTo] = ALPHA_VALUE;
								posTo++;
							}
						}
						else
							bytes_til_next_control_byte = length_of_subsequence;
					}
					else
					{
						if(posTo >= imgDataSize)
							goto stop;
						imgData[posTo] = data[posFrom];
						posTo++;
						bytes_til_next_control_byte--;
					}
				}
			stop:

				if(posTo < imgDataSize)
					memset(&imgData[posTo], 0, imgDataSize - posTo);

				if(bytes_til_next_control_byte != 0)
				{
					printf("Error: Failed to decompress ETRLE compression in %s\n", filename_short);
					delete[]data;
					delete[]imgData;
					return 0;
				}

				int offsetX = subImageHeader[subImageNum].offsetX;
				int offsetY = subImageHeader[subImageNum].offsetY;
				int width = subImageHeader[subImageNum].width;
				int height = subImageHeader[subImageNum].height;
				if(stiAddBorderBasedOnOffset && (offsetX < 0 || offsetY < 0)) //Add padding to left or upper sides of the image based on offset values
				{
					//Calculate new size
					int newWidth = width;
					int newHeight = height;
					int xPadding = 0;
					int yPadding = 0;
					if(offsetX < 0)
					{
						xPadding = -offsetX;
						newWidth += xPadding;
					}
					if(offsetY < 0)
					{
						yPadding = -offsetY;
						newHeight += yPadding;
					}
					unsigned int newImgDataSize = newWidth * newHeight;

					//Create new buffer and move data to it
					unsigned char *newImgData = new unsigned char[newImgDataSize];
					memset(newImgData, 0, newImgDataSize);
					for(int y = 0; y < height; y++)
						memcpy(&newImgData[xPadding + (newWidth * (y + yPadding))], &imgData[width * y], width);

					//Update variables
					width = newWidth;
					height = newHeight;
					imgDataSize = newImgDataSize;
					delete[]imgData;
					imgData = newImgData;
					offsetX = 0;
					offsetY = 0;
				}

				if(stiAddBorderForSubImages && (width < stiHeader->width || height < stiHeader->height)) //Add padding to the right or lower sides of the image based on the image size listed in STI header
				{
					//Calculate new size
					int newWidth = width;
					int newHeight = height;
					int xPadding = 0;
					int yPadding = 0;
					if(width < stiHeader->width)
					{
						xPadding = stiHeader->width - width;
						newWidth += xPadding;
					}
					if(height < stiHeader->height)
					{
						yPadding = stiHeader->height - height;
						newHeight += yPadding;
					}
					unsigned int newImgDataSize = newWidth * newHeight;

					//Create new buffer and move data to it
					unsigned char *newImgData = new unsigned char[newImgDataSize];
					memset(newImgData, 0, newImgDataSize);
					for(int y = 0; y < height; y++)
						memcpy(&newImgData[newWidth * y], &imgData[width * y], width);

					//Update variables
					width = newWidth;
					height = newHeight;
					imgDataSize = newImgDataSize;
					delete[]imgData;
					imgData = newImgData;
					offsetX = 0;
					offsetY = 0;
				}

				if(stiRenderSTIToMiddleOfCanvas && (width < stiHeader->width || height < stiHeader->height))
				{
					//Calculate new size
					int newWidth = stiHeader->width;
					int newHeight = stiHeader->height;
					int posX = 0;
					int posY = 0;
					if(width < stiHeader->width)
						posX = (stiHeader->width / 2) + offsetX;
					if(height < stiHeader->height)
						posY = (stiHeader->height / 2) + offsetY;

					unsigned int newImgDataSize = newWidth * newHeight;

					//Create new buffer and move data to it
					unsigned char *newImgData = new unsigned char[newImgDataSize];
					memset(newImgData, 0, newImgDataSize);
					for(int y = 0; y < height; y++)
						memcpy(&newImgData[posX + (newWidth * (y + posY))], &imgData[width * y], width);

					//Update variables
					width = newWidth;
					height = newHeight;
					imgDataSize = newImgDataSize;
					delete[]imgData;
					imgData = newImgData;
					offsetX = 0;
					offsetY = 0;
				}

				if(!WriteBMP(filename, imgData, imgDataSize, width, height, 8, stiHeader->Indexed.subImageCount > 1, subImageNum, palette, offsetX, offsetY))
				{
					delete[]data;
					delete[]imgData;
					return 0;
				}
				delete[]imgData;
			}
			else
			{
				//TODO: Handle non-compressed image data
				printf("Error: No support for non-compressed indexed STIs yet in %s\n", filename_short);
				delete[]data;
				return 0;
			}
		}
	}
	else
	{
		printf("Error: Unknown image format in %s\n", filename_short);
		delete[]data;
		return 0;
	}

	delete[]data;
	return 1;
}