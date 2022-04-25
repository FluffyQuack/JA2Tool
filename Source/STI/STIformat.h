#pragma once

#define STI_MAGIC 1229149267 //STCI
#define BMP_MAGIC 19778 //BM
#define PALETTEENTRIES 256

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
	TGATYPE_NONE = 0, //No image data
	TGATYPE_UNCOMPRESSED_INDEXED = 1,
	TGATYPE_UNCOMPRESSED_RGB = 2,
	TGATYPE_UNCOMPRESSED_BLACKANDWHITE = 3,
	TGATYPE_RLE_INDEXED = 9,
	TGATYPE_RLE_RGB = 9,
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
	unsigned int appDataSize; //Always 0 for images with no extra sub-images? If sub-images, 16 * image count?
	unsigned char unused[15];
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
} RGBQUAD;

#pragma pack(pop)