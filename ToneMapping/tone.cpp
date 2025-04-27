#include <iostream>
#include <fstream>
#include <cstring>

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
    uint8_t r;
    uint8_t g;
    uint8_t b;

    BMPFileHeader bmpFile;
    BMPInfoHeader bmpInfo;

    bmpFile.signature[2] = '\0';

    readBMP.read(reinterpret_cast<char*>(&bmpFile.signature), 2);
    std::cout << "Signature: " << bmpFile.signature << std::endl;


    readBMP.read(reinterpret_cast<char*>(&bmpFile.fileSize), 4);
    std::cout << "File Size: " << bmpFile.fileSize << " bytes" << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpFile.reserved1), 2);
    std::cout << "Reserved1: " << bmpFile.reserved1 << std::endl;



    readBMP.read(reinterpret_cast<char*>(&bmpFile.reserved2), 2);
    std::cout << "Reserved2: " << bmpFile.reserved2 << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpFile.dataOffset), 4);
    std::cout << "Pixel Data Offset: " << bmpFile.dataOffset << " bytes" << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.headerSize), 4);
    std::cout << "Size of Header: " << bmpInfo.headerSize << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.width), 4);
    std::cout << "Image Width: " << bmpInfo.width << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.height), 4);
    std::cout << "Image Height: " << bmpInfo.height << std::endl;

    // After reading Image Height
    readBMP.read(reinterpret_cast<char*>(&bmpInfo.planes), 2);  // Read Planes (should be 1)
    std::cout << "Planes: " << bmpInfo.planes << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.bitCount), 2);
    std::cout << "Bits per pixel: " << bmpInfo.bitCount << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.compression), 4);
    std::cout << "Compression method: " << bmpInfo.compression << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.imageSize), 4);
    std::cout << "Image Size: " << bmpInfo.imageSize << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.xPixelsPerm), 4);
    std::cout << "Horizontal Resolution: " << bmpInfo.xPixelsPerm << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.yPixelsPerm), 4);
    std::cout << "Vertical Resolution: " << bmpInfo.yPixelsPerm << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.colorsUsed), 4);
    std::cout << "Colors used: " << bmpInfo.colorsUsed << std::endl;

    readBMP.read(reinterpret_cast<char*>(&bmpInfo.colorsImportant), 4);
    std::cout << "Important colors: " << bmpInfo.colorsImportant << std::endl;


    // for(int i = 0; i < 100; i++){
    //     readBMP.read(reinterpret_cast<char*>(&r), 1);
    //     std::cout << "red: " << (int)r << std::endl;

    //     readBMP.read(reinterpret_cast<char*>(&g), 1);
    //     std::cout << "green: " << (int)g << std::endl;

    //     readBMP.read(reinterpret_cast<char*>(&b), 1);
    //     std::cout << "blue: " << (int)b << std::endl;
    // }
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