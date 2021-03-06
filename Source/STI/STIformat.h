#pragma once

#define STI_MAGIC 1229149267 //STCI
#define BMP_MAGIC 19778 //BM
#define PALETTEENTRIES 256

/*
*  How the ETRLE compression works for STI images:
* - Transparency is defined with the encoding rather than a colour in the palette. Though I think palette entry #0 is supposed to represent transparency anyway
* - Decode the data like this:
* -- First byte is a "control byte"
* -- Control bytes contain two pieces of data: first 7 bites signify length of data to follow, and the last bit signify transparency
* -- If control byte has its last bit (0x80) set to true, then the control byte defines quantity of transparent bytes in a row. So we save X amount of transparent bytes into output, and move input position forward by one
* -- If control byte has its last bit set to false, then the control byte define quantity of opaque pixels to follow. So, we copy X amount of bytes from input array to output array, and then move input and output positions by X amount as well
* -- If control byte is 0, then that means we've reached the end of a row of bytes (stride), and we should simply move input position one forward. When encoding, we need to always keep track of how far long a row we are, so we know when to add a control byte with value 0
*/
#define ALPHA_VALUE 0
#define IS_COMPRESSED_BYTE_MASK 0x80
#define NUMBER_OF_BYTES_MASK 0x7F

enum
{
	STIFLAG_TRANSPARENT = 1 << 0, //Never referenced in JA2 code
	STIFLAG_ALPHA = 1 << 1, //Never referenced in JA2 code
	STIFLAG_RGB = 1 << 2, //If true, this is a 16-bit RGBA image
	STIFLAG_INDEXED = 1 << 3, //If true, this is an 8-bit indexed image
	STIFLAG_ZLIB_COMPRESSED = 1 << 4,
	STIFLAG_ETRLE_COMPRESSED = 1 << 5,
};

enum
{
	STIAUX_FULL_TILE = 1 << 0, //Only applicable to terrain tiles
	STIAUX_ANIMATED_TILE = 1 << 1, //Used for cursor and some tiles. Not sure if it's used for sprites too
	STIAUX_DYNAMIC_TILE = 1 << 2, //Never referenced in JA2 code
	STIAUX_INTERACTIVE_TILE = 1 << 3, //Never referenced in JA2 code
	STIAUX_IGNORES_HEIGHT = 1 << 4, //Never referenced in JA2 code
	STIAUX_USES_LAND_Z = 1 << 5, //Never referenced in JA2 code
};

enum
{
	TGATYPE_NONE = 0, //No image data
	TGATYPE_UNCOMPRESSED_INDEXED = 1,
	TGATYPE_UNCOMPRESSED_RGB = 2,
	TGATYPE_UNCOMPRESSED_BLACKANDWHITE = 3,
	TGATYPE_RLE_INDEXED = 9,
	TGATYPE_RLE_RGB = 10,
	TGATYPE_HUFFMAN_INDEXED = 32,
	TGATYPE_HUFFMAN_INDEXED_ALT = 33,
};

enum
{
	TGADESCRIPTOR_REDATTRIBUTE = 1 << 0,
	TGADESCRIPTOR_GREENATTRIBUTE = 1 << 1,
	TGADESCRIPTOR_BLUEATTRIBUTE = 1 << 2,
	TGADESCRIPTOR_ALPHAATTRIBUTE = 1 << 3,
	TGADESCRIPTOR_UNUSED = 1 << 4,
	TGADESCRIPTOR_FLIPPED = 1 << 5, //If true, origin is top-left rather than bottom-left (must be 0 for "truevision" images?)
	TGADESCRIPTOR_INTERLEAVINGFLAG1 = 1 << 6,
	TGADESCRIPTOR_INTERLEAVINGFLAG2 = 1 << 7,
};

enum
{
	BMPC_RGB = 0x0000,
	BMPC_RLE8 = 0x0001,
	BMPC_RLE4 = 0x0002,
	BMPC_BITFIELDS = 0x0003,
	BMPC_JPEG = 0x0004,
	BMPC_PNG = 0x0005,
	BMPC_CMYK = 0x000B,
	BMPC_CMYKRLE8 = 0x000C,
	BMPC_CMYKRLE4 = 0x000D
};

#define REDMASK 63488
#define GREENMASK 2016
#define BLUEMASK 31

#pragma pack(push, 1)
struct stiHeader_s
{
	unsigned int magic;
	unsigned int originalSize; //Size of the image used as input when being converted to STI
	unsigned int imageDataSize; //Size of image data in STI
	unsigned int transparentValue; //Which colour in the palette is transparancy (only applies to 8-bit indexed images). Always 0?
	unsigned int flags; //See above flag enum
	unsigned short height; //Size of image (only used for RGBA images?)
	unsigned short width; //Size of image (only used for RGBA images?)
	
	//Next part varies depending on if this is a 16-bit RGBA image or an 8-bit indexed image
	union
	{
		struct
		{
			unsigned int redMask; //Always equal to 63488 (00000000 00000000 11111000 00000000)?
			unsigned int greenMask; //Always equel to 2016 (00000000 00000000 00000111 11100000)?
			unsigned int blueMask; //Always equal to 31 (00000000 00000000 00000000 00011111)?
			unsigned int alphaMask; //Always equal to 0?
			unsigned char redDepth; //Always 5?
			unsigned char greenDepth; //Always 6?
			unsigned char blueDepth; //Always 5?
			unsigned char alphaDepth; //Always 0?
		} RGB;
		struct
		{
			unsigned int colourCount; //Always 256?
			unsigned short subImageCount; //Quantity of sub-images (does it include main image?)
			unsigned char redDepth; //Always 8
			unsigned char greenDepth; //Always 8
			unsigned char blueDepth; //Always 8
			unsigned char unused[11];
		} Indexed;
	};

	unsigned char colourDepth; //Bits per pixel
	unsigned char padding[3]; //Padding to reflect the alignment the JA2 code expects
	unsigned int appDataSize; //Always 0 for images with no extra sub-images? If sub-images, 16 * image count?
	unsigned char unused[12];
};

struct stiSubImage_s
{
	unsigned int offset;
	unsigned int size;
	short offsetX;
	short offsetY;
	unsigned short height;
	unsigned short width;
};

struct AuxObjectData_s
{
	unsigned char ubWallOrientation;
	unsigned char ubNumberOfTiles;
	unsigned short usTileLocIndex;
	unsigned char ubUnused1[3];
	unsigned char ubCurrentFrame;
	unsigned char ubNumberOfFrames;
	unsigned char fFlags;
	unsigned char ubUnused[6];
};

struct paletteElement_s
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
};

struct tgaHeader_s
{
	char idlength; //Length of an optional string after the header
	char colourmaptype; //1 if this is an indexed image
	char datatypecode;
	short int colourmaporigin; //Index of first entry in palette
	short int colourmaplength; //Count of colours in palette
	char colourmapdepth; //Quantity of bits per colour
	short int x_origin; //Coordinates for the lower left corner of the image
	short int y_origin;
	short width; //Image width
	short height; //Image height
	char bitsperpixel; //Colour depth
	char imagedescriptor;
	//Optional id goes here. Nothing is idlength is 0
	//Palette goes here if it exists
	//Image date is rest of the file data
};

struct bmpHeader_s
{
	//Header
	unsigned short bfType; //Magic
	unsigned int bfSize; //Size of entire file
	unsigned short bfReserved1; //Unused?
	unsigned short bfReserved2; //Unused?
	unsigned int bfOffBits; //Offset for image data

	//Info header
	unsigned int biSize; //Size of info header (40)
	int biWidth;
	int biHeight;
	unsigned short biPlanes;
	unsigned short biBitCount;
	unsigned int biCompression;
	unsigned int biSizeImage;
	int biXPelsPerMeter;
	int biYPelsPerMeter;
	unsigned int biClrUsed;
	unsigned int biClrImportant;
};

struct bmpBitfieldMasks_s
{
	unsigned int r;
	unsigned int g;
	unsigned int b;
};

struct bmpColorMapEntry_s {
	unsigned char rgbBlue;
	unsigned char rgbGreen;
	unsigned char rgbRed;
	unsigned char rgbReserved;
};

#pragma pack(pop)