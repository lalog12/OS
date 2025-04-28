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

struct RGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct RGBf {
    float r;
    float g;
    float b;
};

#pragma pack(pop)
RGBf toneMapReinhard(RGBf color, float avgLum, float a);
void processChunk(int startIdx, int endIdx, const std::vector<RGBf>& input, std::vector<RGB>& output, float avgLum, float exposureKey);
void argCheck(int argc, char *argv[]);
void toneMap(int argc, char *argv[]);

#endif