#ifndef TONE_H
#define TONE_H

#include <cstdint>

#pragma pack(push, 1)
struct BMPFileHeader {
    char signature[3];
    uint32_t fileSize;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t dataOffset;
};

struct BMPInfoHeader{
    uint32_t headerSize;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPixelsPerm;
    int32_t yPixelsPerm;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
};

void argCheck(int argc, char *argv[]);
void toneMap(int argc, char *argv[]);

#endif