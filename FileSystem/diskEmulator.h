#ifndef DISK_EMULATOR_H
#define DISK_EMULATOR_H


#define BLOCKSIZE 256
#define MAXDISKS 16
#define MAXBLOCKS 40

int openDisk(char * filename, int nBytes);
int closeDisk(int disk);
int readBlock(int disk, int bNum, void *block);


#endif