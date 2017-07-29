#include "bflim.h"
#include <png.h>
#include <PVRTextureUtilities.h>

const u32 CBflim::s_uSignatureBflim = SDW_CONVERT_ENDIAN32('FLIM');
const u32 CBflim::s_uSignatureImage = SDW_CONVERT_ENDIAN32('imag');
const int CBflim::s_nBPP[] = { 8, 8, 8, 16, 16, 16, 24, 16, 16, 32, 4, 8, 4, 4, 0, 0, 0, 0, 4, 4 };
const int CBflim::s_nDecodeTransByte[64] =
{
	 0,  1,  4,  5, 16, 17, 20, 21,
	 2,  3,  6,  7, 18, 19, 22, 23,
	 8,  9, 12, 13, 24, 25, 28, 29,
	10, 11, 14, 15, 26, 27, 30, 31,
	32, 33, 36, 37, 48, 49, 52, 53,
	34, 35, 38, 39, 50, 51, 54, 55,
	40, 41, 44, 45, 56, 57, 60, 61,
	42, 43, 46, 47, 58, 59, 62, 63
};

CBflim::CBflim()
	: m_bVerbose(false)
{
}

CBflim::~CBflim()
{
}

void CBflim::SetFileName(const UString& a_sFileName)
{
	m_sFileName = a_sFileName;
}

void CBflim::SetPngName(const UString& a_sPngName)
{
	m_sPngName = a_sPngName;
}

void CBflim::SetVerbose(bool a_bVerbose)
{
	m_bVerbose = a_bVerbose;
}

bool CBflim::DecodeFile()
{
	bool bResult = true;
	FILE* fp = UFopen(m_sFileName.c_str(), USTR("rb"));
	if (fp == nullptr)
	{
		return false;
	}
	fseek(fp, 0, SEEK_END);
	u32 uBflimSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	u8* pBflim = new u8[uBflimSize];
	fread(pBflim, 1, uBflimSize, fp);
	fclose(fp);
	SBflimHeader* pBflimHeader = reinterpret_cast<SBflimHeader*>(pBflim + uBflimSize - (sizeof(SBflimHeader) + sizeof(SImageBlock)));
	SImageBlock* pImageBlock = reinterpret_cast<SImageBlock*>(pBflim + uBflimSize - sizeof(SImageBlock));
	do
	{
		if (pImageBlock->Format < kTextureFormatL8 || (pImageBlock->Format > kTextureFormatA4 && pImageBlock->Format < kTextureFormatL4_another) || pImageBlock->Format > kTextureFormatA4_another)
		{
			bResult = false;
			UPrintf(USTR("ERROR: unknown format %d\n\n"), pImageBlock->Format);
			break;
		}
		n32 nWidth = getBoundSize(pImageBlock->Width);
		n32 nHeight = getBoundSize(pImageBlock->Height);
		n32 nCheckSize = nWidth * nHeight * s_nBPP[pImageBlock->Format] / 8;
		if (pImageBlock->ImageSize != nCheckSize && m_bVerbose)
		{
			UPrintf(USTR("INFO: width: %X(%X), height: %X(%X), checksize: %X, size: %X, bpp: %d, format: %0X\n"), nWidth, pImageBlock->Width, nHeight, pImageBlock->Height, nCheckSize, pImageBlock->ImageSize, pImageBlock->ImageSize * 8 / nWidth / nHeight, pImageBlock->Format);
		}
		pvrtexture::CPVRTexture* pPVRTexture = nullptr;
		if (decode(pBflim, nWidth, nHeight, pImageBlock->Format, pImageBlock->Flag, &pPVRTexture) == 0)
		{
			fp = UFopen(m_sPngName.c_str(), USTR("wb"));
			if (fp == nullptr)
			{
				delete pPVRTexture;
				bResult = false;
				break;
			}
			png_structp pPng = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
			if (pPng == nullptr)
			{
				fclose(fp);
				delete pPVRTexture;
				bResult = false;
				UPrintf(USTR("ERROR: png_create_write_struct error\n\n"));
				break;
			}
			png_infop pInfo = png_create_info_struct(pPng);
			if (pInfo == nullptr)
			{
				png_destroy_write_struct(&pPng, nullptr);
				fclose(fp);
				delete pPVRTexture;
				bResult = false;
				UPrintf(USTR("ERROR: png_create_info_struct error\n\n"));
				break;
			}
			if (setjmp(png_jmpbuf(pPng)) != 0)
			{
				png_destroy_write_struct(&pPng, &pInfo);
				fclose(fp);
				delete pPVRTexture;
				bResult = false;
				UPrintf(USTR("ERROR: setjmp error\n\n"));
				break;
			}
			png_init_io(pPng, fp);
			png_set_IHDR(pPng, pInfo, pImageBlock->Width, pImageBlock->Height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
			u8* pData = static_cast<u8*>(pPVRTexture->getDataPtr());
			png_bytepp pRowPointers = new png_bytep[pImageBlock->Height];
			for (n32 i = 0; i < pImageBlock->Height; i++)
			{
				pRowPointers[i] = pData + i * nWidth * 4;
			}
			png_set_rows(pPng, pInfo, pRowPointers);
			png_write_png(pPng, pInfo, PNG_TRANSFORM_IDENTITY, nullptr);
			png_destroy_write_struct(&pPng, &pInfo);
			delete[] pRowPointers;
			fclose(fp);
			delete pPVRTexture;
		}
		else
		{
			bResult = false;
			UPrintf(USTR("ERROR: decode error\n\n"));
			break;
		}
	} while (false);
	delete[] pBflim;
	return bResult;
}

bool CBflim::EncodeFile()
{
	bool bResult = true;
	FILE* fp = UFopen(m_sFileName.c_str(), USTR("rb"));
	if (fp == nullptr)
	{
		return false;
	}
	fseek(fp, 0, SEEK_END);
	u32 uBflimSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	u8* pBflim = new u8[uBflimSize];
	fread(pBflim, 1, uBflimSize, fp);
	fclose(fp);
	SBflimHeader* pBflimHeader = reinterpret_cast<SBflimHeader*>(pBflim + uBflimSize - (sizeof(SBflimHeader) + sizeof(SImageBlock)));
	SImageBlock* pImageBlock = reinterpret_cast<SImageBlock*>(pBflim + uBflimSize - sizeof(SImageBlock));
	do
	{
		if (pImageBlock->Format < kTextureFormatL8 || (pImageBlock->Format > kTextureFormatA4 && pImageBlock->Format < kTextureFormatL4_another) || pImageBlock->Format > kTextureFormatA4_another)
		{
			bResult = false;
			UPrintf(USTR("ERROR: unknown format %d\n\n"), pImageBlock->Format);
			break;
		}
		n32 nWidth = getBoundSize(pImageBlock->Width);
		n32 nHeight = getBoundSize(pImageBlock->Height);
		n32 nCheckSize = nWidth * nHeight * s_nBPP[pImageBlock->Format] / 8;
		if (pImageBlock->ImageSize != nCheckSize && m_bVerbose)
		{
			UPrintf(USTR("INFO: width: %X(%X), height: %X(%X), checksize: %X, size: %X, bpp: %d, format: %0X\n"), nWidth, pImageBlock->Width, nHeight, pImageBlock->Height, nCheckSize, pImageBlock->ImageSize, pImageBlock->ImageSize * 8 / nWidth / nHeight, pImageBlock->Format);
		}
		fp = UFopen(m_sPngName.c_str(), USTR("rb"));
		if (fp == nullptr)
		{
			bResult = false;
			break;
		}
		png_structp pPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		if (pPng == nullptr)
		{
			fclose(fp);
			bResult = false;
			UPrintf(USTR("ERROR: png_create_read_struct error\n\n"));
			break;
		}
		png_infop pInfo = png_create_info_struct(pPng);
		if (pInfo == nullptr)
		{
			png_destroy_read_struct(&pPng, nullptr, nullptr);
			fclose(fp);
			bResult = false;
			UPrintf(USTR("ERROR: png_create_info_struct error\n\n"));
			break;
		}
		png_infop pEndInfo = png_create_info_struct(pPng);
		if (pEndInfo == nullptr)
		{
			png_destroy_read_struct(&pPng, &pInfo, nullptr);
			fclose(fp);
			bResult = false;
			UPrintf(USTR("ERROR: png_create_info_struct error\n\n"));
			break;
		}
		if (setjmp(png_jmpbuf(pPng)) != 0)
		{
			png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
			fclose(fp);
			bResult = false;
			UPrintf(USTR("ERROR: setjmp error\n\n"));
			break;
		}
		png_init_io(pPng, fp);
		png_read_info(pPng, pInfo);
		n32 nPngWidth = png_get_image_width(pPng, pInfo);
		n32 nPngHeight = png_get_image_height(pPng, pInfo);
		n32 nBitDepth = png_get_bit_depth(pPng, pInfo);
		if (nBitDepth != 8)
		{
			png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
			fclose(fp);
			bResult = false;
			UPrintf(USTR("ERROR: nBitDepth != 8\n\n"));
			break;
		}
		n32 nColorType = png_get_color_type(pPng, pInfo);
		if (nColorType != PNG_COLOR_TYPE_RGB_ALPHA)
		{
			png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
			fclose(fp);
			bResult = false;
			UPrintf(USTR("ERROR: nColorType != PNG_COLOR_TYPE_RGB_ALPHA\n\n"));
			break;
		}
		nWidth = getBoundSize(nPngWidth);
		nHeight = getBoundSize(nPngHeight);
		u8* pData = new unsigned char[nWidth * nHeight * 4];
		png_bytepp pRowPointers = new png_bytep[nHeight];
		for (n32 i = 0; i < nHeight; i++)
		{
			pRowPointers[i] = pData + i * nWidth * 4;
		}
		png_read_image(pPng, pRowPointers);
		png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
		for (n32 i = 0; i < nPngHeight; i++)
		{
			for (n32 j = nPngWidth; j < nWidth; j++)
			{
				*reinterpret_cast<u32*>(pRowPointers[i] + j * 4) = *reinterpret_cast<u32*>(pRowPointers[i] + (nPngWidth - 1) * 4);
			}
		}
		for (n32 i = nPngHeight; i < nHeight; i++)
		{
			memcpy(pRowPointers[i], pRowPointers[nPngHeight - 1], nWidth * 4);
		}
		delete[] pRowPointers;
		fclose(fp);
		pvrtexture::CPVRTexture* pPVRTexture = nullptr;
		bool bSame = false;
		if (pImageBlock->Width == nPngWidth && pImageBlock->Height == nPngHeight && decode(pBflim, nWidth, nHeight, pImageBlock->Format, pImageBlock->Flag, &pPVRTexture) == 0)
		{
			u8* pTextureData = static_cast<u8*>(pPVRTexture->getDataPtr());
			bSame = true;
			for (n32 i = 0; i < nPngHeight; i++)
			{
				if (memcmp(pTextureData + i * nWidth * 4, pData + i * nWidth * 4, nPngWidth * 4) != 0)
				{
					bSame = false;
					break;
				}
			}
		}
		delete pPVRTexture;
		if (!bSame)
		{
			fp = UFopen(m_sFileName.c_str(), USTR("wb"));
			if (fp != nullptr)
			{
				u8* pBuffer = nullptr;
				if (encode(pData, nWidth, nHeight, pImageBlock->Format, pImageBlock->Flag, 1, s_nBPP[pImageBlock->Format], &pBuffer) != 0)
				{
					delete[] pBuffer;
					fclose(fp);
					delete[] pData;
					bResult = false;
					UPrintf(USTR("ERROR: encode error\n\n"));
					break;
				}
				pImageBlock->Width = nPngWidth;
				pImageBlock->Height = nPngHeight;
				pImageBlock->ImageSize = nWidth * nHeight * s_nBPP[pImageBlock->Format] / 8;
				pBflimHeader->FileSize = pImageBlock->ImageSize + sizeof(*pBflimHeader) + sizeof(*pImageBlock);
				fwrite(pBuffer, 1, pImageBlock->ImageSize, fp);
				delete[] pBuffer;
				fwrite(pBflimHeader, sizeof(*pBflimHeader), 1, fp);
				fwrite(pImageBlock, sizeof(*pImageBlock), 1, fp);
				fclose(fp);
			}
			else
			{
				bResult = false;
			}
		}
		delete[] pData;
	} while (false);
	delete[] pBflim;
	return bResult;
}

bool CBflim::IsBflimFile(const UString& a_sFileName)
{
	FILE* fp = UFopen(a_sFileName.c_str(), USTR("rb"));
	if (fp == nullptr)
	{
		return false;
	}
	fseek(fp, 0, SEEK_END);
	u32 uFileSize = ftell(fp);
	if (uFileSize < sizeof(SBflimHeader) + sizeof(SImageBlock))
	{
		return false;
	}
	fseek(fp, uFileSize - (sizeof(SBflimHeader) + sizeof(SImageBlock)), SEEK_SET);
	SBflimHeader bflimHeader;
	fread(&bflimHeader, sizeof(bflimHeader), 1, fp);
	fclose(fp);
	return bflimHeader.Signature == s_uSignatureBflim;
}

n32 CBflim::getBoundSize(n32 a_nSize)
{
	n32 nSize = 8;
	while (nSize < a_nSize)
	{
		nSize *= 2;
	}
	return nSize;
}

int CBflim::decode(u8* a_pBuffer, n32 a_nWidth, n32 a_nHeight, n32 a_nFormat, n32 a_nFlag, pvrtexture::CPVRTexture** a_pPVRTexture)
{
	n32 nWidth = a_nWidth;
	n32 nHeight = a_nHeight;
	n32 nRotation = a_nFlag >> kImageFlagTexelRotationPos & (SDW_BIT32(kImageFlagTexelRotationLen) - 1);
	switch (nRotation)
	{
	case kTexelRotationNone:
		break;
	case kTexelRotationRotate:
	case kTexelRotationFlipUV:
		nWidth = a_nHeight;
		nHeight = a_nWidth;
		break;
	default:
		return 1;
	}
	u8* pRGBA = nullptr;
	u8* pAlpha = nullptr;
	switch (a_nFormat)
	{
	case kTextureFormatRGBA8888:
		{
			u8* pTemp = new u8[nWidth * nHeight * 4];
			for (n32 i = 0; i < nWidth * nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					for (n32 k = 0; k < 4; k++)
					{
						pTemp[(i * 64 + j) * 4 + k] = a_pBuffer[(i * 64 + s_nDecodeTransByte[j]) * 4 + 3 - k];
					}
				}
			}
			pRGBA = new u8[nWidth * nHeight * 4];
			for (n32 i = 0; i < nHeight; i++)
			{
				for (n32 j = 0; j < nWidth; j++)
				{
					for (n32 k = 0; k < 4; k++)
					{
						pRGBA[(i * nWidth + j) * 4 + k] = pTemp[(((i / 8) * (nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 4 + k];
					}
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatRGB888:
		{
			u8* pTemp = new u8[nWidth * nHeight * 3];
			for (n32 i = 0; i < nWidth * nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					for (n32 k = 0; k < 3; k++)
					{
						pTemp[(i * 64 + j) * 3 + k] = a_pBuffer[(i * 64 + s_nDecodeTransByte[j]) * 3 + 2 - k];
					}
				}
			}
			pRGBA = new u8[nWidth * nHeight * 3];
			for (n32 i = 0; i < nHeight; i++)
			{
				for (n32 j = 0; j < nWidth; j++)
				{
					for (n32 k = 0; k < 3; k++)
					{
						pRGBA[(i * nWidth + j) * 3 + k] = pTemp[(((i / 8) * (nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 3 + k];
					}
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatRGBA5551:
	case kTextureFormatRGB565:
	case kTextureFormatRGBA4444:
		{
			u8* pTemp = new u8[nWidth * nHeight * 2];
			for (n32 i = 0; i < nWidth * nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					for (n32 k = 0; k < 2; k++)
					{
						pTemp[(i * 64 + j) * 2 + k] = a_pBuffer[(i * 64 + s_nDecodeTransByte[j]) * 2 + k];
					}
				}
			}
			pRGBA = new u8[nWidth * nHeight * 2];
			for (n32 i = 0; i < nHeight; i++)
			{
				for (n32 j = 0; j < nWidth; j++)
				{
					for (n32 k = 0; k < 2; k++)
					{
						pRGBA[(i * nWidth + j) * 2 + k] = pTemp[(((i / 8) * (nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 2 + k];
					}
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatLA88:
	case kTextureFormatHL8:
		{
			u8* pTemp = new u8[nWidth * nHeight * 2];
			for (n32 i = 0; i < nWidth * nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					for (n32 k = 0; k < 2; k++)
					{
						pTemp[(i * 64 + j) * 2 + k] = a_pBuffer[(i * 64 + s_nDecodeTransByte[j]) * 2 + 1 - k];
					}
				}
			}
			pRGBA = new u8[nWidth * nHeight * 2];
			for (n32 i = 0; i < nHeight; i++)
			{
				for (n32 j = 0; j < nWidth; j++)
				{
					for (n32 k = 0; k < 2; k++)
					{
						pRGBA[(i * nWidth + j) * 2 + k] = pTemp[(((i / 8) * (nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 2 + k];
					}
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatL8:
	case kTextureFormatA8:
	case kTextureFormatLA44:
		{
			u8* pTemp = new u8[nWidth * nHeight];
			for (n32 i = 0; i < nWidth * nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					pTemp[i * 64 + j] = a_pBuffer[i * 64 + s_nDecodeTransByte[j]];
				}
			}
			pRGBA = new u8[nWidth * nHeight];
			for (n32 i = 0; i < nHeight; i++)
			{
				for (n32 j = 0; j < nWidth; j++)
				{
					pRGBA[i * nWidth + j] = pTemp[((i / 8) * (nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8];
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatL4:
	case kTextureFormatA4:
	case kTextureFormatL4_another:
	case kTextureFormatA4_another:
		{
			u8* pTemp = new u8[nWidth * nHeight];
			for (n32 i = 0; i < nWidth * nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j += 2)
				{
					pTemp[i * 64 + j] = (a_pBuffer[i * 32 + s_nDecodeTransByte[j] / 2] & 0xF) * 0x11;
					pTemp[i * 64 + j + 1] = (a_pBuffer[i * 32 + s_nDecodeTransByte[j] / 2] >> 4 & 0xF) * 0x11;
				}
			}
			pRGBA = new u8[nWidth * nHeight];
			for (n32 i = 0; i < nHeight; i++)
			{
				for (n32 j = 0; j < nWidth; j++)
				{
					pRGBA[i * nWidth + j] = pTemp[((i / 8) * (nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8];
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatETC1:
		{
			u8* pTemp = new u8[nWidth * nHeight / 2];
			for (n32 i = 0; i < nWidth * nHeight / 2 / 8; i++)
			{
				for (n32 j = 0; j < 8; j++)
				{
					pTemp[i * 8 + j] = a_pBuffer[i * 8 + 7 - j];
				}
			}
			pRGBA = new u8[nWidth * nHeight / 2];
			for (n32 i = 0; i < nHeight; i += 4)
			{
				for (n32 j = 0; j < nWidth; j += 4)
				{
					memcpy(pRGBA + ((i / 4) * (nWidth / 4) + j / 4) * 8, pTemp + (((i / 8) * (nWidth / 8) + j / 8) * 4 + (i % 8 / 4 * 2 + j % 8 / 4)) * 8, 8);
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatETC1_A4:
		{
			u8* pTemp = new u8[nWidth * nHeight / 2];
			for (n32 i = 0; i < nWidth * nHeight / 2 / 8; i++)
			{
				for (n32 j = 0; j < 8; j++)
				{
					pTemp[i * 8 + j] = a_pBuffer[8 + i * 16 + 7 - j];
				}
			}
			pRGBA = new u8[nWidth * nHeight / 2];
			for (n32 i = 0; i < nHeight; i += 4)
			{
				for (n32 j = 0; j < nWidth; j += 4)
				{
					memcpy(pRGBA + ((i / 4) * (nWidth / 4) + j / 4) * 8, pTemp + (((i / 8) * (nWidth / 8) + j / 8) * 4 + (i % 8 / 4 * 2 + j % 8 / 4)) * 8, 8);
				}
			}
			delete[] pTemp;
			pTemp = new u8[nWidth * nHeight];
			for (n32 i = 0; i < nWidth * nHeight / 16; i++)
			{
				for (n32 j = 0; j < 4; j++)
				{
					pTemp[i * 16 + j] = (a_pBuffer[i * 16 + j * 2] & 0x0F) * 0x11;
					pTemp[i * 16 + j + 4] = (a_pBuffer[i * 16 + j * 2] >> 4 & 0x0F) * 0x11;
					pTemp[i * 16 + j + 8] = (a_pBuffer[i * 16 + j * 2 + 1] & 0x0F) * 0x11;
					pTemp[i * 16 + j + 12] = (a_pBuffer[i * 16 + j * 2 + 1] >> 4 & 0x0F) * 0x11;
				}
			}
			pAlpha = new u8[nWidth * nHeight];
			for (n32 i = 0; i < nHeight; i++)
			{
				for (n32 j = 0; j < nWidth; j++)
				{
					pAlpha[i * nWidth + j] = pTemp[(((i / 8) * (nWidth / 8) + j / 8) * 4 + i % 8 / 4 * 2 + j % 8 / 4) * 16 + i % 4 * 4 + j % 4];
				}
			}
			delete[] pTemp;
		}
		break;
	}
	PVRTextureHeaderV3 pvrTextureHeaderV3;
	switch (a_nFormat)
	{
	case kTextureFormatRGBA8888:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8).PixelTypeID;
		break;
	case kTextureFormatRGB888:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 0, 8, 8, 8, 0).PixelTypeID;
		break;
	case kTextureFormatRGBA5551:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 5, 5, 5, 1).PixelTypeID;
		break;
	case kTextureFormatRGB565:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 0, 5, 6, 5, 0).PixelTypeID;
		break;
	case kTextureFormatRGBA4444:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 4, 4, 4, 4).PixelTypeID;
		break;
	case kTextureFormatLA88:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('l', 'a', 0, 0, 8, 8, 0, 0).PixelTypeID;
		break;
	case kTextureFormatHL8:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 0, 0, 8, 8, 0, 0).PixelTypeID;
		break;
	case kTextureFormatL8:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatA8:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatLA44:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('l', 'a', 0, 0, 4, 4, 0, 0).PixelTypeID;
		break;
	case kTextureFormatL4:
	case kTextureFormatL4_another:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatA4:
	case kTextureFormatA4_another:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatETC1:
		pvrTextureHeaderV3.u64PixelFormat = ePVRTPF_ETC1;
		break;
	case kTextureFormatETC1_A4:
		pvrTextureHeaderV3.u64PixelFormat = ePVRTPF_ETC1;
		break;
	}
	pvrTextureHeaderV3.u32Height = nHeight;
	pvrTextureHeaderV3.u32Width = nWidth;
	MetaDataBlock metaDataBlock;
	metaDataBlock.DevFOURCC = PVRTEX3_IDENT;
	metaDataBlock.u32Key = ePVRTMetaDataTextureOrientation;
	metaDataBlock.u32DataSize = 3;
	metaDataBlock.Data = new PVRTuint8[metaDataBlock.u32DataSize];
	metaDataBlock.Data[0] = ePVRTOrientRight;
	metaDataBlock.Data[1] = ePVRTOrientUp;
	metaDataBlock.Data[2] = ePVRTOrientIn;
	pvrtexture::CPVRTextureHeader pvrTextureHeader(pvrTextureHeaderV3, 1, &metaDataBlock);
	*a_pPVRTexture = new pvrtexture::CPVRTexture(pvrTextureHeader, pRGBA);
	delete[] pRGBA;
	pvrtexture::Transcode(**a_pPVRTexture, pvrtexture::PVRStandard8PixelType, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB);
	if (a_nFormat == kTextureFormatETC1_A4)
	{
		u8* pData = static_cast<u8*>((*a_pPVRTexture)->getDataPtr());
		for (n32 i = 0; i < nHeight; i++)
		{
			for (n32 j = 0; j < nWidth; j++)
			{
				pData[(i * nWidth + j) * 4 + 3] = pAlpha[i * nWidth + j];
			}
		}
		delete[] pAlpha;
	}
	if (nRotation == kTexelRotationRotate)
	{
		pvrtexture::Rotate90(**a_pPVRTexture, ePVRTAxisZ, false);
	}
	else if (nRotation == kTexelRotationFlipUV)
	{
		pvrtexture::Rotate90(**a_pPVRTexture, ePVRTAxisZ, true);
		pvrtexture::Flip(**a_pPVRTexture, ePVRTAxisX);
	}
	return 0;
}

int CBflim::encode(u8* a_pData, n32 a_nWidth, n32 a_nHeight, n32 a_nFormat, n32 a_nFlag, n32 a_nMipmapLevel, n32 a_nBPP, u8** a_pBuffer)
{
	PVRTextureHeaderV3 pvrTextureHeaderV3;
	pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PVRStandard8PixelType.PixelTypeID;
	pvrTextureHeaderV3.u32Height = a_nHeight;
	pvrTextureHeaderV3.u32Width = a_nWidth;
	MetaDataBlock metaDataBlock;
	metaDataBlock.DevFOURCC = PVRTEX3_IDENT;
	metaDataBlock.u32Key = ePVRTMetaDataTextureOrientation;
	metaDataBlock.u32DataSize = 3;
	metaDataBlock.Data = new PVRTuint8[metaDataBlock.u32DataSize];
	metaDataBlock.Data[0] = ePVRTOrientRight;
	metaDataBlock.Data[1] = ePVRTOrientUp;
	metaDataBlock.Data[2] = ePVRTOrientIn;
	pvrtexture::CPVRTextureHeader pvrTextureHeader(pvrTextureHeaderV3, 1, &metaDataBlock);
	pvrtexture::CPVRTexture* pPVRTexture = nullptr;
	pvrtexture::CPVRTexture* pPVRTextureAlpha = nullptr;
	if (a_nFormat != kTextureFormatETC1_A4)
	{
		pPVRTexture = new pvrtexture::CPVRTexture(pvrTextureHeader, a_pData);
	}
	else
	{
		u8* pRGBAData = new u8[a_nWidth * a_nHeight * 4];
		memcpy(pRGBAData, a_pData, a_nWidth * a_nHeight * 4);
		u8* pAlphaData = new u8[a_nWidth * a_nHeight * 4];
		memcpy(pAlphaData, a_pData, a_nWidth * a_nHeight * 4);
		for (n32 i = 0; i < a_nWidth * a_nHeight; i++)
		{
			pRGBAData[i * 4 + 3] = 0xFF;
			pAlphaData[i * 4] = 0;
			pAlphaData[i * 4 + 1] = 0;
			pAlphaData[i * 4 + 2] = 0;
		}
		pPVRTexture = new pvrtexture::CPVRTexture(pvrTextureHeader, pRGBAData);
		pPVRTextureAlpha = new pvrtexture::CPVRTexture(pvrTextureHeader, pAlphaData);
		delete[] pRGBAData;
		delete[] pAlphaData;
	}
	n32 nWidth = a_nWidth;
	n32 nHeight = a_nHeight;
	n32 nRotation = a_nFlag >> kImageFlagTexelRotationPos & (SDW_BIT32(kImageFlagTexelRotationLen) - 1);
	switch (nRotation)
	{
	case kTexelRotationNone:
		break;
	case kTexelRotationRotate:
	case kTexelRotationFlipUV:
		nWidth = a_nHeight;
		nHeight = a_nWidth;
		break;
	default:
		return 1;
	}
	if (nRotation == kTexelRotationRotate)
	{
		pvrtexture::Rotate90(*pPVRTexture, ePVRTAxisZ, true);
		if (a_nFormat == kTextureFormatETC1_A4)
		{
			pvrtexture::Rotate90(*pPVRTextureAlpha, ePVRTAxisZ, true);
		}
	}
	else if (nRotation == kTexelRotationFlipUV)
	{
		pvrtexture::Flip(*pPVRTexture, ePVRTAxisX);
		pvrtexture::Rotate90(*pPVRTexture, ePVRTAxisZ, false);
		if (a_nFormat == kTextureFormatETC1_A4)
		{
			pvrtexture::Flip(*pPVRTextureAlpha, ePVRTAxisX);
			pvrtexture::Rotate90(*pPVRTextureAlpha, ePVRTAxisZ, false);
		}
	}
	if (a_nMipmapLevel != 1)
	{
		pvrtexture::GenerateMIPMaps(*pPVRTexture, pvrtexture::eResizeNearest, a_nMipmapLevel);
		if (a_nFormat == kTextureFormatETC1_A4)
		{
			pvrtexture::GenerateMIPMaps(*pPVRTextureAlpha, pvrtexture::eResizeNearest, a_nMipmapLevel);
		}
	}
	pvrtexture::uint64 uPixelFormat = 0;
	pvrtexture::ECompressorQuality eCompressorQuality = pvrtexture::ePVRTCBest;
	switch (a_nFormat)
	{
	case kTextureFormatRGBA8888:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8).PixelTypeID;
		break;
	case kTextureFormatRGB888:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 0, 8, 8, 8, 0).PixelTypeID;
		break;
	case kTextureFormatRGBA5551:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 5, 5, 5, 1).PixelTypeID;
		break;
	case kTextureFormatRGB565:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 0, 5, 6, 5, 0).PixelTypeID;
		break;
	case kTextureFormatRGBA4444:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 4, 4, 4, 4).PixelTypeID;
		break;
	case kTextureFormatLA88:
		uPixelFormat = pvrtexture::PixelType('l', 'a', 0, 0, 8, 8, 0, 0).PixelTypeID;
		break;
	case kTextureFormatHL8:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 0, 0, 8, 8, 0, 0).PixelTypeID;
		break;
	case kTextureFormatL8:
		uPixelFormat = pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatA8:
		uPixelFormat = pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatLA44:
		uPixelFormat = pvrtexture::PixelType('l', 'a', 0, 0, 4, 4, 0, 0).PixelTypeID;
		break;
	case kTextureFormatL4:
	case kTextureFormatL4_another:
		uPixelFormat = pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatA4:
	case kTextureFormatA4_another:
		uPixelFormat = pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatETC1:
		uPixelFormat = ePVRTPF_ETC1;
		eCompressorQuality = pvrtexture::eETCSlowPerceptual;
		break;
	case kTextureFormatETC1_A4:
		uPixelFormat = ePVRTPF_ETC1;
		eCompressorQuality = pvrtexture::eETCSlowPerceptual;
		break;
	}
	pvrtexture::Transcode(*pPVRTexture, uPixelFormat, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB, eCompressorQuality);
	if (a_nFormat == kTextureFormatETC1_A4)
	{
		pvrtexture::Transcode(*pPVRTextureAlpha, pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB, pvrtexture::ePVRTCBest);
	}
	n32 nTotalSize = 0;
	n32 nCurrentSize = 0;
	for (n32 l = 0; l < a_nMipmapLevel; l++)
	{
		nTotalSize += (nWidth >> l) * (nHeight >> l) * a_nBPP / 8;
	}
	*a_pBuffer = new u8[nTotalSize];
	for (n32 l = 0; l < a_nMipmapLevel; l++)
	{
		n32 nMipmapWidth = nWidth >> l;
		n32 nMipmapHeight = nHeight >> l;
		u8* pRGBA = static_cast<u8*>(pPVRTexture->getDataPtr(l));
		u8* pAlpha = nullptr;
		if (a_nFormat == kTextureFormatETC1_A4)
		{
			pAlpha = static_cast<u8*>(pPVRTextureAlpha->getDataPtr(l));
		}
		switch (a_nFormat)
		{
		case kTextureFormatRGBA8888:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight * 4];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						for (n32 k = 0; k < 4; k++)
						{
							pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 4 + k] = pRGBA[(i * nMipmapWidth + j) * 4 + k];
						}
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						for (n32 k = 0; k < 4; k++)
						{
							pMipmapBuffer[(i * 64 + s_nDecodeTransByte[j]) * 4 + 3 - k] = pTemp[(i * 64 + j) * 4 + k];
						}
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatRGB888:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight * 3];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						for (n32 k = 0; k < 3; k++)
						{
							pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 3 + k] = pRGBA[(i * nMipmapWidth + j) * 3 + k];
						}
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						for (n32 k = 0; k < 3; k++)
						{
							pMipmapBuffer[(i * 64 + s_nDecodeTransByte[j]) * 3 + 2 - k] = pTemp[(i * 64 + j) * 3 + k];
						}
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatRGBA5551:
		case kTextureFormatRGB565:
		case kTextureFormatRGBA4444:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight * 2];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						for (n32 k = 0; k < 2; k++)
						{
							pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 2 + k] = pRGBA[(i * nMipmapWidth + j) * 2 + k];
						}
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						for (n32 k = 0; k < 2; k++)
						{
							pMipmapBuffer[(i * 64 + s_nDecodeTransByte[j]) * 2 + k] = pTemp[(i * 64 + j) * 2 + k];
						}
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatLA88:
		case kTextureFormatHL8:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight * 2];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						for (n32 k = 0; k < 2; k++)
						{
							pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 2 + k] = pRGBA[(i * nMipmapWidth + j) * 2 + k];
						}
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						for (n32 k = 0; k < 2; k++)
						{
							pMipmapBuffer[(i * 64 + s_nDecodeTransByte[j]) * 2 + 1 - k] = pTemp[(i * 64 + j) * 2 + k];
						}
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatL8:
		case kTextureFormatA8:
		case kTextureFormatLA44:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						pTemp[((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8] = pRGBA[i * nMipmapWidth + j];
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						pMipmapBuffer[i * 64 + s_nDecodeTransByte[j]] = pTemp[i * 64 + j];
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatL4:
		case kTextureFormatA4:
		case kTextureFormatL4_another:
		case kTextureFormatA4_another:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						pTemp[((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8] = pRGBA[i * nMipmapWidth + j];
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j += 2)
					{
						pMipmapBuffer[i * 32 + s_nDecodeTransByte[j] / 2] = ((pTemp[i * 64 + j] / 0x11) & 0x0F) | ((pTemp[i * 64 + j + 1] / 0x11) << 4 & 0xF0);
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatETC1:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight / 2];
				for (n32 i = 0; i < nMipmapHeight; i += 4)
				{
					for (n32 j = 0; j < nMipmapWidth; j += 4)
					{
						memcpy(pTemp + (((i / 8) * (nMipmapWidth / 8) + j / 8) * 4 + (i % 8 / 4 * 2 + j % 8 / 4)) * 8, pRGBA + ((i / 4) * (nMipmapWidth / 4) + j / 4) * 8, 8);
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 2 / 8; i++)
				{
					for (n32 j = 0; j < 8; j++)
					{
						pMipmapBuffer[i * 8 + 7 - j] = pTemp[i * 8 + j];
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatETC1_A4:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight / 2];
				for (n32 i = 0; i < nMipmapHeight; i += 4)
				{
					for (n32 j = 0; j < nMipmapWidth; j += 4)
					{
						memcpy(pTemp + (((i / 8) * (nMipmapWidth / 8) + j / 8) * 4 + (i % 8 / 4 * 2 + j % 8 / 4)) * 8, pRGBA + ((i / 4) * (nMipmapWidth / 4) + j / 4) * 8, 8);
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 2 / 8; i++)
				{
					for (n32 j = 0; j < 8; j++)
					{
						pMipmapBuffer[8 + i * 16 + 7 - j] = pTemp[i * 8 + j];
					}
				}
				delete[] pTemp;
				pTemp = new u8[nMipmapWidth * nMipmapHeight];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 4 + i % 8 / 4 * 2 + j % 8 / 4) * 16 + i % 4 * 4 + j % 4] = pAlpha[i * nMipmapWidth + j];
					}
				}
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 16; i++)
				{
					for (n32 j = 0; j < 4; j++)
					{
						pMipmapBuffer[i * 16 + j * 2] = ((pTemp[i * 16 + j] / 0x11) & 0x0F) | ((pTemp[i * 16 + j + 4] / 0x11) << 4 & 0xF0);
						pMipmapBuffer[i * 16 + j * 2 + 1] = ((pTemp[i * 16 + j + 8] / 0x11) & 0x0F) | ((pTemp[i * 16 + j + 12] / 0x11) << 4 & 0xF0);
					}
				}
				delete[] pTemp;
			}
			break;
		}
		nCurrentSize += nMipmapWidth * nMipmapHeight * a_nBPP / 8;
	}
	delete pPVRTexture;
	if (a_nFormat == kTextureFormatETC1_A4)
	{
		delete pPVRTextureAlpha;
	}
	return 0;
}
