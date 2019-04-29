#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dbDefs.h>
#include <registryFunction.h>
#include <subRecord.h>
#include <aSubRecord.h>
#include <epicsExport.h>
#include <unistd.h>
#include "epicsThread.h"
#include <zmq.h>
#include "jfrxsub.h"
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

// Global variables
epicsThreadId tid[10];
sem_t WriterSem, ROISem;

struct JFDATA {
   double blob;
   unsigned long int frameid;
   char *rawbuffer;
   int index;
   double isrunning;
   unsigned long int liveframeid;
   char *rawframe;
   char *gainmap;
   char *rawadu;
   double gaincount[4];
   int TID;
   int writerenable;
   char userfilename[40];
   int last_TID_index;
   int detstate;
   int roix1;
   int roix2;
   int roiy1;
   int roiy2;
   int roixsize;
   int roiysize;
   double roibuflag;
} jfdata;

struct GPool {
  int dummy;
  char *roibuffer;
  unsigned long int roiprocframe;
  unsigned long int writerprocframe;
  int roidatasize;
  int roiframesize;
  char *roiaduframe;
} gpool;

// Functions

// Write data from record to jfdata struct
static long writeaddr(subRecord *precord){
  int addr = (int)precord->a;
  int intval, aux;
  switch(addr){
    case 0:
      jfdata.blob=precord->val;
      break;
    case 10:
      jfdata.TID=(int)precord->val;
      break;
    case 11:
      jfdata.writerenable=(int)precord->val;
      break;
    case 15:
      intval=(int)precord->val;
      if( (intval < jfdata.roix2) && (intval>=1) && (intval<1024) && (jfdata.detstate==0) ){
        jfdata.roix1=intval;
        jfdata.roixsize=jfdata.roix2-jfdata.roix1+1;
      }
      break;
    case 16:
      intval=(int)precord->val;
      if( (intval > jfdata.roix1) && (intval>1) && (intval<=1024) && (jfdata.detstate==0)){
        jfdata.roix2=intval;
        jfdata.roixsize=jfdata.roix2-jfdata.roix1+1;
      }
      break;
    case 17:
      intval=(int)precord->val;
      if( (intval < jfdata.roiy2) && (intval>=1) && (intval<256) && (jfdata.detstate==0)){
        jfdata.roiy1=intval;
        jfdata.roiysize=jfdata.roiy2-jfdata.roiy1+1;
      }
      break;
    case 18:
      intval=(int)precord->val;
      if( (intval > jfdata.roiy1) && (intval>1) && (intval<=256) && (jfdata.detstate==0)){
        jfdata.roiy2=intval;
        jfdata.roiysize=jfdata.roiy2-jfdata.roiy1+1;
      }
      break;
    case 19:
      intval=(int)precord->val;
      aux = intval-1+jfdata.roix1;
      if( (intval>1) && (aux<=1024) ){
        jfdata.roix2=aux;
        jfdata.roixsize=intval;
      }
      break;
    case 20:
      intval=(int)precord->val;
      aux = intval-1+jfdata.roiy1;
      if( (intval>1) && (aux<=256) ){
        jfdata.roiy2=aux;
        jfdata.roiysize=intval;
      }
      break;
  }
  return 0;
}

// Read data from jfdata struct based on their position within the struct
static long readaddr(subRecord *precord){
  int addr = (int)precord->a;
  switch(addr){
    case 0:
      precord->val=jfdata.blob;
      break;
    case 1:
      precord->val=(double)jfdata.frameid;
      break;
    case 4:
      precord->val=jfdata.isrunning;
      break;
    case 5:
      precord->val=(double)jfdata.liveframeid;
      break;
    case 10:
      precord->val=(double)jfdata.TID;
      break;
    case 11:
      precord->val=(int)jfdata.writerenable;
      break;
    case 14:
      precord->val=jfdata.detstate;
      break;
    case 15:
      precord->val=jfdata.roix1;
      break;
    case 16:
      precord->val=jfdata.roix2;
      break;
    case 17:
      precord->val=jfdata.roiy1;
      break;
    case 18:
      precord->val=jfdata.roiy2;
      break;
    case 19:
      precord->val=jfdata.roixsize;
      break;
    case 20:
      precord->val=jfdata.roiysize;
      break;
    case 21:
      precord->val=jfdata.roibuflag;
      break;
    case 22:
      precord->val=(double)gpool.roiprocframe;
      break;
    case 23:
      precord->val=(double)gpool.writerprocframe;
      break;
    default:
      precord->val=0;
      break;
  }
  return 0;
}

static long userfilename(aSubRecord *precord){
  char *rawstring, fname[40];
  char *pvala;
  int err;

  rawstring = (char *)precord->a;
  pvala = (char *)precord->vala;

  // only takes first sequence of charactes to ignore empty spaces
  err = sscanf(rawstring, "%s", fname);
  if(err==1){
    strcpy(jfdata.userfilename, fname);
    strcpy(pvala, jfdata.userfilename);
    return 0;
  }
  return 1;
}

static long readarrays(aSubRecord *precord){
    unsigned long int *pvale;
    double *pvalf, *pvalg, *pvalh, *pvali;

    pvale=precord->vale;
    pvalf=precord->valf;
    pvalg=precord->valg;
    pvalh=precord->valh;
    pvali=precord->vali;

    if(*pvale!=jfdata.liveframeid){
      memcpy(precord->vala, jfdata.rawadu, FRAME_SIZE);
      precord->neva=NPIXELS;
      memcpy(precord->valb, jfdata.gainmap, FRAME_SIZE);
      precord->nevb=NPIXELS;
      memcpy(precord->valj, gpool.roiaduframe, FRAME_SIZE);
      precord->nevj=NPIXELS;

      *pvale=jfdata.liveframeid;
      *pvalf=jfdata.gaincount[0];
      *pvalg=jfdata.gaincount[1];
      *pvalh=jfdata.gaincount[2];
      *pvali=jfdata.gaincount[3];
      return 0;
    }
    return 1;
}

static long startserver(subRecord *precord){
  //Creates all semaphores
  sem_init( &WriterSem, 0, 0);
  sem_init( &ROISem, 0, 0);

  // Init thread 0 - ZMQ receiver
  tid[0]=epicsThreadCreate("ZMQ_subscriber", epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium), zmqserver, 0);
  if (!tid[0]){
    printf("epicsThreadCreate [0] failed\n");
  }else{
    jfdata.isrunning++;
    printf("ZMQ_subscriber: Running...\n");
  }
  // Init thread 1 - Slow Live data analysis
  tid[1]=epicsThreadCreate("Live_updater", epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium), liveserver, 0);
  if (!tid[1]){
    printf("epicsThreadCreate [1] failed\n");
  }else{
    jfdata.isrunning++;
    printf("Live_updater: Running...\n");
  }

  // Init thread 2 - File Writer
  // tid[2]=epicsThreadCreate("file_writer", epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium), filewriter, 0);
  // if (!tid[2]){
  //   printf("epicsThreadCreate [2] failed\n");
  // }else{
  //   jfdata.isrunning++;
  //   printf("File Writer: Running...\n");
  // }

  // Init thread 3 - ROI Server
  // tid[3]=epicsThreadCreate("roi_server", epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium), roiserver, 0);
  // if (!tid[3]){
  //   printf("epicsThreadCreate [3] failed\n");
  // }else{
  //   jfdata.isrunning++;
  //   printf("ROI Server: Running...\n");
  // }

  return 0;
}

// Creates ZMQ Server and buffers to receive data
static void zmqserver(void *ctx){
  int size, index;
  unsigned long int *frameid;
  char *tmpframe;
  unsigned long int datablock;

  datablock = (unsigned long int)(FRAMES)*(unsigned long int)DATA_SIZE;

  // create buffers
  jfdata.rawbuffer = (char *)malloc(datablock);
  jfdata.rawframe = (char *)malloc(FRAME_SIZE);
  jfdata.gainmap = (char *)malloc(FRAME_SIZE);
  jfdata.rawadu = (char *)malloc(FRAME_SIZE);
  tmpframe = (char *)malloc(DATA_SIZE);
  gpool.roiaduframe = (char *)malloc(FRAME_SIZE);

  frameid = (unsigned long int *)tmpframe;


  jfdata.detstate=0;

  // prepare ZMQ context and subscriber
  void *context = zmq_ctx_new ();
  void *subscriber = zmq_socket (context, ZMQ_SUB);
  zmq_connect (subscriber, "tcp://localhost:30001");
  zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
  // infinite loop
  printf("ZMQ_subscriber: Listening on port 30001...\n");

  while (1) {
    switch(jfdata.detstate){
      case 0:
        size = zmq_recv (subscriber, tmpframe, DATA_SIZE, 0);
        if(size==DATA_SIZE){
          index = *frameid%FRAMES;
          memcpy(&jfdata.rawbuffer[DATA_SIZE*index], tmpframe, DATA_SIZE);
          // update received frameid from JF
          jfdata.frameid = *frameid;
          // export internal index to global index
          jfdata.index = index;
          jfdata.detstate = 1;
          sem_post(&ROISem);
          sem_post(&WriterSem);
          //unlockbinarysem(&ROISem);
          //unlockbinarysem(&WriterSem);
        }else{
          jfdata.detstate = 0;
        }
        break;
      case 1:
        size = zmq_recv (subscriber, tmpframe, DATA_SIZE, 0);
        if(size==DATA_SIZE){
          index = *frameid%FRAMES;
          memcpy(&jfdata.rawbuffer[DATA_SIZE*index], tmpframe, DATA_SIZE);
          // update received frameid from JF
          jfdata.frameid = *frameid;
          // export internal index to global index
          jfdata.index = index;
        }else{
          jfdata.detstate = 0;
        }
        break;
      default:
        jfdata.detstate = 0;
        break;
    }
  }
  //  We never get here, but clean up anyhow
  zmq_close (subscriber);
  zmq_ctx_destroy (context);
}

// Check for new frame every second
// Creates statistics and image for live view
static void liveserver(void *ctx){
  unsigned long int liveframeid=0;
  unsigned long int liveroiframeid=0;

  int index;
  while(1){
    if(liveframeid!=jfdata.frameid){
      liveframeid=jfdata.frameid;
      index = jfdata.index;
      memcpy(jfdata.rawframe, &jfdata.rawbuffer[(DATA_SIZE*index)+48], FRAME_SIZE);
      calcgainmap(jfdata.gainmap, jfdata.rawframe);
      calcrawadu(jfdata.rawadu, jfdata.rawframe, FRAME_SIZE);
      jfdata.liveframeid = liveframeid;
    }
    if(liveroiframeid!=gpool.roiprocframe){
      liveroiframeid=gpool.roiprocframe;
      index = gpool.roiprocframe%FRAMES;
      calcrawadu(gpool.roiaduframe, &gpool.roibuffer[(gpool.roidatasize*index)+48], gpool.roiframesize);
    }

    sleep(1);
  }
}

// File writing logic
static void filewriter(void *ctx){
  int tid, fcount, bunchid;
  int nframes, roidatasize, writerstate=0;
  char savingfolder[64]="/home/epics/jfdata/detector_data";
  char folderpath[128], filename[256];
  FILE *fp;
  unsigned long int nextframe, savebytesize, chksize;
  int index;

  gpool.writerprocframe = 0;

  while(1){
    switch(writerstate){
      case 0:
        //printf("Writer waiting...\n");
        sem_wait(&WriterSem);
        if(jfdata.writerenable){
          nextframe = jfdata.frameid;
          //printf("frameid: %lu, roiprocframe: %lu, roibytesize: %d\n", nextframe, gpool.roiprocframe, gpool.roibytesize);
          //printf("Creating folder\n");
          if(createfolder(folderpath, savingfolder)){
            printf("Unable to create folder\n");
            writerstate = 0;
          }else{
            //printf("Folder OK. Path: %s\n", folderpath);
            tid = jfdata.TID;
            fcount=0;
            bunchid=0;
            usleep(100000);
            roidatasize = gpool.roidatasize;
            writerstate = 1;
            createmasterfile(folderpath, jfdata.roix1, jfdata.roix2, jfdata.roiy1, jfdata.roiy2, tid);
            //printf("frameid: %lu, roiprocframe: %lu, roibytesize: %d\n", nextframe, gpool.roiprocframe, gpool.roibytesize);
          }
        }else{
          writerstate = 0;
        }
        break;
      case 1:
        sprintf(filename, "%s/%s_%04d_%04d.raw", folderpath, jfdata.userfilename, tid, bunchid);
        //printf("saving on file: %s\n", filename);
        fp = fopen(filename, "w");
        if(fp==NULL){
          printf("Unable to create file %s\n", filename);
          fclose(fp);
          writerstate=0;
        }
        writerstate=2;
        break;
      case 2:
        if(gpool.writerprocframe == gpool.roiprocframe){
          usleep(100000);
        }else{
          index = nextframe%FRAMES;
          nframes = calcframeseq(nextframe, gpool.roiprocframe, FRAMES, fcount);

          // in case something is wrong
          if(nframes==0){
            printf("Writer::Error:nframes=0!\n");
            sleep(1);
            break;
          }
          savebytesize = (unsigned long int)(nframes*roidatasize);
          //printf("nframes: %d, savebytesize: %lu\n", nframes, savebytesize);
          chksize = fwrite(&gpool.roibuffer[index*roidatasize], 1, savebytesize, fp);
          //usleep(10000);
          //printf("%d > %d\n", index, index+nframes);
          //chksize = savebytesize;

          if(chksize != savebytesize){
            printf("Writer::Error saving to file\n");
            fclose(fp);
            writerstate = 0;
            break;
          }
          nextframe += nframes;
          gpool.writerprocframe = nextframe-1;
          fcount += nframes;
          if(fcount>=BUNCH){
            fclose(fp);
            fcount=0;
            bunchid++;
            writerstate=1;
          }
        }
        if( (jfdata.detstate == 0) && (gpool.writerprocframe==gpool.roiprocframe) ){
          fclose(fp);
          jfdata.TID++;
          writerstate = 0;
        }
        break;
      default:
        writerstate = 0;
        break;
    }
  }
}

static void roiserver(void *ctx){
  int roistate=0;
  unsigned long int datablock;
  int roix1, roiy1, roixsize, roiysize, roiframesize, roidatasize, index;
  unsigned long int nextframe;
  //struct timeval  tv1, tv2;
  //double avgtime;

  datablock = (unsigned long int)(FRAMES)*(unsigned long int)DATA_SIZE;
  gpool.roibuffer = (char *)malloc(datablock);

  // initial roi settings
  jfdata.roix1=1;
  jfdata.roix2=1024;
  jfdata.roiy1=1;
  jfdata.roiy2=256;
  jfdata.roixsize=jfdata.roix2-jfdata.roix1+1;
  jfdata.roiysize=jfdata.roiy2-jfdata.roiy1+1;

  while(1){
    switch(roistate){
      case 0:
        //printf("ROI waiting... \n");
        sem_wait(&ROISem);
        roistate = 1;
        roix1 = jfdata.roix1-1;
        roiy1 = jfdata.roiy1-1;
        roixsize = jfdata.roixsize;
        roiysize = jfdata.roiysize;
        roiframesize = (2*roixsize*roiysize);
        roidatasize = roiframesize+HEADER_SIZE;
        gpool.roiframesize = roiframesize;
        gpool.roidatasize = roidatasize;
        gpool.roiprocframe = 0xFFFFFFFFFFFFFFFF;
        jfdata.roibuflag = 0;
        nextframe = jfdata.frameid;
        break;
      case 1:
        // Main ROI processing loop
        if(gpool.roiprocframe == jfdata.frameid){
          // waits a little for new data to arrive
          usleep(10000);
        }else{
          // process nextframe
          index = nextframe%FRAMES;
          //if(index==0) gettimeofday(&tv1, NULL);
          calcroiframe(&gpool.roibuffer[index*roidatasize], &jfdata.rawbuffer[index*DATA_SIZE], roix1, roiy1, roixsize, roiysize);
          // save frame processed and increment nextframe
          gpool.roiprocframe = nextframe++;
          //printf("roi: nextframe:%lu roiprocframe: %lu jfframe: %lu\n ", nextframe, gpool.roiprocframe, jfdata.frameid);
          // calculate number of frames remaining to reach top of buffer
          jfdata.roibuflag = (100*(jfdata.frameid-gpool.roiprocframe))/FRAMES;
          /*
          if(index==0){
            gettimeofday(&tv2, NULL);
            avgtime = ((double) (tv2.tv_usec - tv1.tv_usec) / 1000000 + (double) (tv2.tv_sec - tv1.tv_sec))/1000;
            printf ("Time for frame zero = %.9f seconds\n", avgtime);
          }
          */
        }
        if( (jfdata.detstate == 0) && (gpool.roiprocframe==jfdata.frameid) )
          roistate = 0;
        break;
      default:
        roistate=0;
        break;
    }
  }
}

// Creates roi data in a separate buffer
void calcroiframe(char *destframe, char *srcframe, int x1, int y1, int xsize, int ysize){
  int i, possrc;
  //unsigned short *val;

  int head = 48+(2*x1)+(2*y1*1024);
  //memcpy(destframe, srcframe, (2*1024*256)+48);
  memcpy(destframe, srcframe, 48);
  for(i=0; i<ysize; i++){
    possrc=head+(i*2048);
    memcpy(&destframe[(i*2*xsize)+48], &srcframe[possrc], (2*xsize));
  }
  /*
  val = (unsigned short *)&srcframe[48+2048];
  printf("SRC: ");
  for(i=0; i<10; i++){
    printf("%u ", val[i]);
  }
  printf("\n");

  val = (unsigned short *)&destframe[48];
  printf("DST: ");
  for(i=0; i<10; i++){
    printf("%u ", val[i]);
  }
  printf("\n");
  */
}

// Creates gain map
void calcgainmap(char *destframe, char *srcframe){
  unsigned short *gainarray;
  int i;
  // reset gain counts
  for(i=0;i<4;i++)
    jfdata.gaincount[i]=0;
  memcpy(destframe, srcframe, FRAME_SIZE);
  gainarray=(unsigned short *)destframe;
  // Needs to be modified for roi
  for(i=0;i<262144;i++){
      gainarray[i]=gainarray[i]>>14;
      jfdata.gaincount[gainarray[i]]++;
  }
}

// Creates the ADU frame (raw - gain informartion)
void calcrawadu(char *destframe, char *srcframe, int size){
  unsigned short *rawadu;
  int i;
  // reset gain counts
  memcpy(destframe, srcframe, size);
  rawadu=(unsigned short *)destframe;
  // Needs to be modified for roi
  for(i=0;i<(size/2);i++){
      rawadu[i]=rawadu[i]&0x3fff;
  }
}

void unlockbinarysem(sem_t *semaphore){
  int count;
  sem_getvalue(semaphore, &count);
  if(count==0)
    sem_post(semaphore);
}

int createfolder(char *fullpath, char *path){
        time_t timer;
        struct tm* tm_info;
        struct stat st = {0};
        char day[3];
        char month[3];
        char year[5];
        char fname[128];
        int fcheck;

        time(&timer);
        tm_info = localtime(&timer);
        strftime(day, 3, "%d", tm_info);
        strftime(month, 3, "%m", tm_info);
        strftime(year, 5, "%Y", tm_info);
        sprintf(fname, "%s/%s_%s_%s", path, year, month, day);

        fcheck = stat(fname, &st);

        if(fcheck == 0){
                strcpy(fullpath, fname);
                return 0;
        }else{
                mkdir(fname, 0775);
                fcheck = stat(fname, &st);
                if(fcheck==0){
                        strcpy(fullpath, fname);
                        return 0;
                }else{
                        return -1;
                }
        }
}

unsigned long int saveroibuf(FILE *fp, unsigned long int nextframe, unsigned long int lastframe, char *buffer, int frames, int *fcount){
    int nframes;
    char data[32]="10";

    printf("Saving from %lu to %lu\n", nextframe, lastframe);
    nframes = (int)(lastframe-nextframe);
    for(int i=0; i<nframes; i++){
      fwrite(&data, sizeof(char), 2, fp);
    }
    *fcount =+ nframes;
    usleep(500000);
    return lastframe;
}

int calcframeseq(unsigned long int next, unsigned long int last, int frames, int fcount){
        int gap, ret;
        // frames remaining
        ret = (int)(last-next+1);
        // number of frames in continous segment in buffer
        gap = FRAMES-(int)(next%FRAMES);
        if( (ret>gap) && (gap>0) ) ret=gap;
        // number of frames remaining to fill file
        gap = BUNCH-fcount;
        if(ret>gap) ret=gap;
        // limit of 1000 frames per fwrite call
        if(ret>1000) ret=1000;
        // if broken, avoid negative count
        if(ret<0) ret=0;
        return ret;
}

void createmasterfile(char *folderpath, int x1, int x2, int y1, int y2, int tid){
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char strtext[1024];
  int size = (2*(x2-x1+1)*(y2-y1+1))+48;
  char filename[128];
  FILE *fp;

  sprintf(filename, "%s/%s_%04d_master.txt", folderpath, jfdata.userfilename, tid);
  sprintf(strtext, "\
Date: %02d.%02d.%d\n\
Time: %02d:%02d:%02d\n\
TID: %04d\n\
ROI x1: %d\n\
ROI y1: %d\n\
ROI x2: %d\n\
ROI y2: %d\n\
ROI Size: %d Bytes\n\
", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, tid, x1, y1, x2, y2, size);

  fp = fopen(filename, "w");
  if(fp==NULL) return;

  fwrite(strtext, sizeof(char), strlen(strtext), fp);
  fclose(fp);
  //printf("%s\n", strtext);
}

epicsRegisterFunction(startserver);
epicsRegisterFunction(readaddr);
epicsRegisterFunction(writeaddr);
epicsRegisterFunction(readarrays);
epicsRegisterFunction(userfilename);
