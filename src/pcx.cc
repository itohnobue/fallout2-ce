#include "pcx.h"

#include "memory_manager.h"

namespace fallout {

// 0x519DC8 runcount
unsigned char gPcxLastRunLength = 0;

// 0x519DC9 runvalue
unsigned char gPcxLastValue = 0;

// NOTE: The reading method in this function is a little bit odd. It does not
// use high level reading functions, which can read right into struct. Instead
// they read everything into temporary variables.
//
// Returns 0 on success, -1 on any I/O error.
//
// 0x4961D4 readPcxHeader
int pcxReadHeader(PcxHeader* pcxHeader, File* stream)
{
    int ch;

    ch = fileReadChar(stream);
    if (ch == -1) return -1;
    pcxHeader->identifier = (unsigned char)ch;

    ch = fileReadChar(stream);
    if (ch == -1) return -1;
    pcxHeader->version = (unsigned char)ch;

    ch = fileReadChar(stream);
    if (ch == -1) return -1;
    pcxHeader->encoding = (unsigned char)ch;

    ch = fileReadChar(stream);
    if (ch == -1) return -1;
    pcxHeader->bitsPerPixel = (unsigned char)ch;

    short minX;
    if (fileRead(&minX, 2, 1, stream) != 1) return -1;
    pcxHeader->minX = minX;

    short minY;
    if (fileRead(&minY, 2, 1, stream) != 1) return -1;
    pcxHeader->minY = minY;

    short maxX;
    if (fileRead(&maxX, 2, 1, stream) != 1) return -1;
    pcxHeader->maxX = maxX;

    short maxY;
    if (fileRead(&maxY, 2, 1, stream) != 1) return -1;
    pcxHeader->maxY = maxY;

    short horizontalResolution;
    if (fileRead(&horizontalResolution, 2, 1, stream) != 1) return -1;
    pcxHeader->horizontalResolution = horizontalResolution;

    short verticalResolution;
    if (fileRead(&verticalResolution, 2, 1, stream) != 1) return -1;
    pcxHeader->verticalResolution = verticalResolution;

    for (int index = 0; index < 48; index++) {
        ch = fileReadChar(stream);
        if (ch == -1) return -1;
        pcxHeader->palette[index] = (unsigned char)ch;
    }

    ch = fileReadChar(stream);
    if (ch == -1) return -1;
    pcxHeader->reserved1 = (unsigned char)ch;

    ch = fileReadChar(stream);
    if (ch == -1) return -1;
    pcxHeader->planeCount = (unsigned char)ch;

    short bytesPerLine;
    if (fileRead(&bytesPerLine, 2, 1, stream) != 1) return -1;
    pcxHeader->bytesPerLine = bytesPerLine;

    short paletteType;
    if (fileRead(&paletteType, 2, 1, stream) != 1) return -1;
    pcxHeader->paletteType = paletteType;

    short horizontalScreenSize;
    if (fileRead(&horizontalScreenSize, 2, 1, stream) != 1) return -1;
    pcxHeader->horizontalScreenSize = horizontalScreenSize;

    short verticalScreenSize;
    if (fileRead(&verticalScreenSize, 2, 1, stream) != 1) return -1;
    pcxHeader->verticalScreenSize = verticalScreenSize;

    for (int index = 0; index < 54; index++) {
        ch = fileReadChar(stream);
        if (ch == -1) return -1;
        pcxHeader->reserved2[index] = (unsigned char)ch;
    }

    return 0;
}

// 0x49636C pcxDecodeScanline
int pcxReadLine(unsigned char* data, int size, File* stream)
{
    unsigned char runLength = gPcxLastRunLength;
    unsigned char value = gPcxLastValue;

    int uncompressedSize = 0;
    int index = 0;
    do {
        uncompressedSize += runLength;
        while (runLength > 0 && index < size) {
            data[index] = value;
            runLength--;
            index++;
        }

        gPcxLastRunLength = runLength;
        gPcxLastValue = value;

        if (runLength != 0) {
            uncompressedSize -= runLength;
            break;
        }

        value = fileReadChar(stream);
        if ((value & 0xC0) == 0xC0) {
            gPcxLastRunLength = value & 0x3F;
            value = fileReadChar(stream);
            runLength = gPcxLastRunLength;
        } else {
            runLength = 1;
        }
    } while (index < size);

    gPcxLastRunLength = runLength;
    gPcxLastValue = value;

    return uncompressedSize;
}

// 0x49641C readPcxVgaPalette
int pcxReadPalette(PcxHeader* pcxHeader, unsigned char* palette, File* stream)
{
    if (pcxHeader->version != 5) {
        return 0;
    }

    long pos = fileTell(stream);
    long size = fileGetSize(stream);
    fileSeek(stream, size - 769, SEEK_SET);
    if (fileReadChar(stream) != 12) {
        fileSeek(stream, pos, SEEK_SET);
        return 0;
    }

    for (int index = 0; index < 768; index++) {
        palette[index] = fileReadChar(stream);
    }

    fileSeek(stream, pos, SEEK_SET);

    return 1;
}

// 0x496494 loadPCX
unsigned char* pcxRead(const char* path, int* widthPtr, int* heightPtr, unsigned char* palette)
{
    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        return nullptr;
    }

    PcxHeader pcxHeader;
    if (pcxReadHeader(&pcxHeader, stream) != 0) {
        fileClose(stream);
        return nullptr;
    }

    int width = pcxHeader.maxX - pcxHeader.minX + 1;
    int height = pcxHeader.maxY - pcxHeader.minY + 1;

    *widthPtr = width;
    *heightPtr = height;

    int bytesPerLine = pcxHeader.planeCount * pcxHeader.bytesPerLine;
    unsigned char* data = (unsigned char*)internal_malloc_safe(bytesPerLine * height, __FILE__, __LINE__); // "..\\int\\PCX.C", 195
    if (data == nullptr) {
        // NOTE: This code is unreachable, internal_malloc_safe never fails.
        fileClose(stream);
        return nullptr;
    }

    gPcxLastRunLength = 0;
    gPcxLastValue = 0;

    unsigned char* ptr = data;
    for (int y = 0; y < height; y++) {
        pcxReadLine(ptr, bytesPerLine, stream);
        ptr += width;
    }

    pcxReadPalette(&pcxHeader, palette, stream);

    fileClose(stream);

    return data;
}

} // namespace fallout
