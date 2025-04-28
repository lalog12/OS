#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cmath>
#include <thread>
#include <algorithm>
#include "tone.h"



int main(int argc, char* argv[]){

    argCheck(argc, argv);

    toneMap(argc, argv);

}

void toneMap(int argc, char *argv[]){

    std::fstream readBMP(argv[1], std::ios::binary | std::ios::in);

    std::fstream writeBMP(argv[2], std::ios::binary | std::ios::out);

    if(!readBMP){
        std::cerr << "Error: Cannot open file " << argv[1] << std::endl;
        exit(1);
    }
    if(!writeBMP){
        std::cerr << "Error: Cannot open file " <<  argv[2] << std::endl;
        exit(1);
    }

    char signature[3];
    signature[2] = '\0';


    BMPFileHeader bmpFile;
    BMPInfoHeader bmpInfo;


    bmpFile.signature[2] = '\0';

    readBMP.read(reinterpret_cast<char*>(&bmpFile.signature), 2);
    // std::cout << "Signature: " << bmpFile.signature << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpFile.fileSize), 4);
    // std::cout << "File Size: " << bmpFile.fileSize << " bytes" << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpFile.reserved1), 2);
    // std::cout << "Reserved1: " << bmpFile.reserved1 << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpFile.reserved2), 2);
    // std::cout << "Reserved2: " << bmpFile.reserved2 << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpFile.dataOffset), 4);
    // std::cout << "Pixel Data Offset: " << bmpFile.dataOffset << " bytes" << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.headerSize), 4);
    // std::cout << "Size of Header: " << bmpInfo.headerSize << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.width), 4);
    // std::cout << "Image Width: " << bmpInfo.width << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.height), 4);
    // std::cout << "Image Height: " << bmpInfo.height << std::endl;

    // After reading Image Height
    readBMP.read(reinterpret_cast<char*>(&bmpInfo.planes), 2);  // Read Planes (should be 1)
    // std::cout << "Planes: " << bmpInfo.planes << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.bitCount), 2);
    // std::cout << "Bits per pixel: " << bmpInfo.bitCount << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.compression), 4);
    // std::cout << "Compression method: " << bmpInfo.compression << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.imageSize), 4);
    // std::cout << "Image Size: " << bmpInfo.imageSize << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.xPixelsPerm), 4);
    // std::cout << "Horizontal Resolution: " << bmpInfo.xPixelsPerm << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.yPixelsPerm), 4);
    // std::cout << "Vertical Resolution: " << bmpInfo.yPixelsPerm << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.colorsUsed), 4);
    // std::cout << "Colors used: " << bmpInfo.colorsUsed << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.colorsImportant), 4);
    // std::cout << "Important colors: " << bmpInfo.colorsImportant << std::endl;

    RGB rgb;
    RGBf rgbf;
    std::vector<RGB> pixels;
    std::vector<RGBf> normalizedpixels;

    int padding = (4 - (bmpInfo.width * 3) % 4) % 4; // bmpInfo.width are pixels. Each pixel has 3 bytes.
    for (int i = 0; i < bmpInfo.height; i++){
        for(int j = 0; j < bmpInfo.width; j++){

            readBMP.read(reinterpret_cast<char*>(&rgb.b), 1);
            rgbf.b = static_cast<float>(rgb.b) / 255.0f;

            readBMP.read(reinterpret_cast<char*>(&rgb.g), 1);
            rgbf.g = static_cast<float>(rgb.g) / 255.0f;

            readBMP.read(reinterpret_cast<char*>(&rgb.r), 1);
            rgbf.r = static_cast<float>(rgb.r) / 255.0f;

            pixels.push_back(rgb);
            normalizedpixels.push_back(rgbf);
        }
        readBMP.seekg(padding, std::ios::cur); // skips the padding at the end of the RGB row
    }

    float logSum = 0.0f;
    float delta = 1e-4f;
    RGBf luminanceWeights = {0.2126f, 0.7152f, 0.0722f};

    int totalPixels = bmpInfo.height * bmpInfo.width;

    for(int i = 0; i < totalPixels; i++){

        float luminance = luminanceWeights.b * normalizedpixels[i].b + 
        luminanceWeights.g * normalizedpixels[i].g + 
        luminanceWeights.r * normalizedpixels[i].r;
        logSum += log(delta + luminance);
    }

    float logAvgLuminance = exp(logSum / (float)totalPixels);

    float exposureKey = std::stof(argv[3]);

    int numThreads = std::stoi(argv[4]);

    std::cout << logAvgLuminance << std::endl;


    // Prepare output pixel array
std::vector<RGB> outputPixels(totalPixels);

// Create and launch threads
std::vector<std::thread> threads;
int chunkSize = totalPixels / numThreads;

for (int i = 0; i < numThreads; i++) {
    int startIdx = i * chunkSize;
    int endIdx = (i == numThreads - 1) ? totalPixels : (i + 1) * chunkSize;
    
    threads.push_back(std::thread(processChunk, startIdx, endIdx, std::ref(normalizedpixels), 
                                 std::ref(outputPixels), logAvgLuminance, exposureKey));
}

// Join all threads
for (auto& thread : threads) {
    thread.join();
}

// Write the BMP headers to output file
writeBMP.write(reinterpret_cast<char*>(&bmpFile.signature), 2);
writeBMP.write(reinterpret_cast<char*>(&bmpFile.fileSize), 4);
writeBMP.write(reinterpret_cast<char*>(&bmpFile.reserved1), 2);
writeBMP.write(reinterpret_cast<char*>(&bmpFile.reserved2), 2);
writeBMP.write(reinterpret_cast<char*>(&bmpFile.dataOffset), 4);

writeBMP.write(reinterpret_cast<char*>(&bmpInfo.headerSize), 4);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.width), 4);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.height), 4);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.planes), 2);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.bitCount), 2);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.compression), 4);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.imageSize), 4);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.xPixelsPerm), 4);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.yPixelsPerm), 4);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.colorsUsed), 4);
writeBMP.write(reinterpret_cast<char*>(&bmpInfo.colorsImportant), 4);

// Write pixel data with padding
char paddingBytes[3] = {0, 0, 0}; // Up to 3 bytes of padding

for (int i = 0; i < bmpInfo.height; i++) {
    for (int j = 0; j < bmpInfo.width; j++) {
        int pixelIndex = i * bmpInfo.width + j;
        
        // Write BGR (not RGB) order as per BMP format
        writeBMP.write(reinterpret_cast<char*>(&outputPixels[pixelIndex].b), 1);
        writeBMP.write(reinterpret_cast<char*>(&outputPixels[pixelIndex].g), 1);
        writeBMP.write(reinterpret_cast<char*>(&outputPixels[pixelIndex].r), 1);
    }
    
    // Write padding bytes at the end of each row
    if (padding > 0) {
        writeBMP.write(paddingBytes, padding);
    }
}


readBMP.close();
writeBMP.close();

std::cout << "Tone mapping completed successfully." << std::endl;


}


// Function to apply Reinhard tone mapping to a pixel
RGBf toneMapReinhard(RGBf color, float avgLum, float a) {
    // Calculate luminance
    RGBf luminanceWeights = {0.2126f, 0.7152f, 0.0722f};
    float L = luminanceWeights.r * color.r + luminanceWeights.g * color.g + luminanceWeights.b * color.b;
    
    // Scale luminance based on exposure key
    float L_scaled = (a / avgLum) * L;
    
    // Apply Reinhard tone mapping
    float L_mapped = L_scaled / (1.0f + L_scaled);
    

    if (L < 1e-5f) return {0.0f, 0.0f, 0.0f};
    
    // Preserve color ratio (preserves chrominance)
    RGBf result;
    result.r = color.r * (L_mapped / L);
    result.g = color.g * (L_mapped / L);
    result.b = color.b * (L_mapped / L);
    
    return result;
}

// Function that will be executed by each thread
void processChunk(int startIdx, int endIdx, const std::vector<RGBf>& input, 
                 std::vector<RGB>& output, float avgLum, float exposureKey) {
    for (int i = startIdx; i < endIdx; i++) {
        // Apply tone mapping
        RGBf mappedPixel = toneMapReinhard(input[i], avgLum, exposureKey);
        
        // Denormalize back to 0-255 range
        RGB outPixel;
        outPixel.r = static_cast<uint8_t>(std::min(std::max(mappedPixel.r * 255.0f, 0.0f), 255.0f));
        outPixel.g = static_cast<uint8_t>(std::min(std::max(mappedPixel.g * 255.0f, 0.0f), 255.0f));
        outPixel.b = static_cast<uint8_t>(std::min(std::max(mappedPixel.b * 255.0f, 0.0f), 255.0f));
        
        output[i] = outPixel;
    }
}

void argCheck(int argc, char *argv[]){
    int error = 0;
    if(argc != 5){
        std::cout << "./tone [SRC imagename] [TARGET imagename] [exposure_key] [number of threads]" << std::endl;
        exit(1);
    }
    
    // Checks that SRC imagename and TARGET imagename are .bmp files.
    for (int i = 1; i <= 2; i++){
        char *filename = argv[i];

        if (strlen(argv[i]) < 4 || (strcmp(&filename[strlen(argv[i]) - 4], ".bmp") != 0) ){
            std::cout << argv[i] << " is not a bmp file." << std::endl;
            error = 1;
        }
    }
    // checks that the [exposure_key] is a float
    try {
        float exposure_key = std::stof(argv[3]);
    } 
    catch (const std::invalid_argument& e) {
        std::cout << argv[3] << " is not a valid float." << std::endl;
        error = 1;
    } 
    catch (const std::out_of_range& e) {
        std::cout << argv[3] << " is out of range for a float." << std::endl;
        error = 1;
    }

    // Check to make sure [number of threads] is an int.
    for (int j = 0; j < strlen(argv[4]); j++)

        if ('0' > argv[4][j] || argv[4][j] > '9'){
            std::cout << argv[4] << " is not a number." << std::endl;
            error = 1;
            break;
        }

    if (error)
        exit(1);

}