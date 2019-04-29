#include <semaphore.h>
// half a module
// (1024*256)*2 + 48 (bytes)
//#define DATA_SIZE 524336
//#define FRAME_SIZE 524288
//#define NPIXELS 262144

// quarter of a module
// (512*256)*2 + 48 (bytes)
#define DATA_SIZE 262192
#define FRAME_SIZE 262144
#define NPIXELS 131072

#define HEADER_SIZE 48
#define FRAMES 1000
#define BUNCH 10000

//function declaration
static void zmqserver(void *ctx);
static void liveserver(void *ctx);
static void procserver(void *ctx);
static void filewriter(void *ctx);
static void roiserver(void *ctx);

//void calcroiframe(char *destframe, char *srcframe);
void unlockbinarysem(sem_t *semaphore);

void calcroiframe(char *destframe, char *srcframe, int x1, int y1, int xsize, int ysize);
void calcgainmap(char *destframe, char *srcframe);
void calcrawadu(char *destframe, char *srcframe, int size);

int createfolder(char *fullpath, char *path);

unsigned long int saveroibuf(FILE *fp, unsigned long int nextframe, unsigned long int lastframe, char *buffer, int frames, int *fcount);
int calcframeseq(unsigned long int next, unsigned long int last, int frames, int fcount);

void createmasterfile(char *folderpath, int x1, int x2, int y1, int y2, int tid);
