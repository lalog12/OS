#include "diskEmulator.h"
#include <iostream>
#include <filesystem>
#include <cerrno>
#include <cstring>
#include <cstdio>

FILE * disks[MAXDISKS] = {NULL};
int diskSizeInBlocks[MAXDISKS] = {0};
bool diskInUse[MAXDISKS] = {false};




int main(int argc,char *argv[]){

char file[10] = "hi.txt";
char buffer[256] = "I want to write this stuff into my buffer";
char readBuffer[256] = {0}; 
int disk = openDisk(file, 1 * BLOCKSIZE);
writeBlock(disk, 0, buffer);
readBlock(disk, 0, readBuffer);
closeDisk(disk);

}


int openDisk(char *filename, int nBytes){

    FILE *fp = NULL;

    if(nBytes == 0){  //done
        for(int i = 0; i < MAXDISKS; i++){
            if(disks[i] == NULL){   // reading and not writing or overwriting
                fp = fopen(filename, "r+b");
                if (fp == NULL){
                    std::cout << "Error opening " << filename << "." << "Error:" << strerror(errno) << std::endl;
                    return -6;
                }
                disks[i] = fp;
                diskInUse[i] = true; 
                if(fseek(fp, 0, SEEK_END) != 0){
                    std::cout << "Error seeking to end of file." << std::endl;
                    return -7;
                }
                int fileSize = ftell(fp);
                if(fileSize == -1){
                    std::cout << "Error getting size of file." << std::endl;
                    return -8;
                }
                diskSizeInBlocks[i] = fileSize / BLOCKSIZE;
                fseek(fp, 0, SEEK_SET);
                return i;         
            }
        }
        std::cout << "No free disk slots available" << std::endl;
        return -1;
    }

    else if(nBytes < BLOCKSIZE){    // Done
        std::cout << "nBytes < BLOCKSIZE" << std::endl;
        return -2;
    }

    else{  // nBytes >= 256

        int extraBytes = nBytes % BLOCKSIZE;
        nBytes -= extraBytes;   // cutting off extra bytes
        
        for(int i = 0; i < MAXDISKS; i++){
            if (disks[i] == NULL){
                fp = fopen(filename, "w+b");
                if(fp == NULL){   // overwriting 
                    std::cout << "Error opening " << filename << "." << "Error:" << strerror(errno) << std::endl;
                    return -3;
                }
                if(fseek(fp, nBytes - 1, SEEK_SET) < 0){
                    std::cout << "Error seeking to end of file." << std::endl;
                    return -10;
                }
                if(fputc(0, fp) < 0){
                    std::cout << "Error placing NULL character at end of file." << std::endl;
                    return -11;
                }
                fflush(fp);

                if(fseek(fp, 0, SEEK_SET) < 0){
                    std::cout << "Error seeking to beginning of file." << std::endl;
                    return -12;
                }

                disks[i] = fp;
                diskInUse[i] = true;
                diskSizeInBlocks[i] = nBytes / BLOCKSIZE;
                return i;
            }
        }
        std::cout << "No free disk slots available" << std::endl;
        return -4;
    }
}


int closeDisk(int disk){
    if (disk < 0){
        std::cout << "Invalid disk number." << std::endl;
        return -9;
    }

    std::cout << "Closing " << disk << std::endl;
    if(disks[disk] != NULL){
        fclose(disks[disk]);
        disks[disk] = NULL;
        diskSizeInBlocks[disk] = 0;
        diskInUse[disk] = false;
        return 0;
    }
    std::cout << "No disk to close." << std::endl;
    return 0;
}

int readBlock(int disk, int bNum, void *block){

    if(diskInUse[disk] == false){
        std::cout << "Disk is not in use." << std::endl;
        return -13;
    }
    if (bNum >= diskSizeInBlocks[disk]){
        std::cout << "bNum is greater than blocks in disk " << diskSizeInBlocks[disk] << "." << std::endl;
        return -14;
    }

    int blockOffset = bNum * BLOCKSIZE;  // 256 bytes (BLOCKSIZE) * offset (bNum) 

    if(fseek(disks[disk], blockOffset, SEEK_SET) != 0){
        return -15;
    }
    if(fread(block, 1, BLOCKSIZE, disks[disk]) != BLOCKSIZE){
        return -16;
    };

    return 0;
}

int writeBlock(int disk, int bNum, void *block){

    if(diskInUse[disk] == false){
        std::cout << "Disk is not in use." << std::endl;
        return -13;
    }
    if (bNum >= diskSizeInBlocks[disk]){
        std::cout << "bNum is greater than blocks in disk " << diskSizeInBlocks[disk] << "." << std::endl;
        return -14;
    }

    int blockOffset = bNum * BLOCKSIZE;  // 256 bytes (BLOCKSIZE) * offset (bNum) 

    if(fseek(disks[disk], blockOffset, SEEK_SET) != 0){
        return -15;
    }
    if(fwrite(block, 1, BLOCKSIZE, disks[disk]) != BLOCKSIZE){
        return -16;
    };

    return 0;
}