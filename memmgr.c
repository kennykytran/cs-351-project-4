//compile: clang -pthread -lm -o memmgr.exe memmgr.c
//run: ./memmgr.exe addresses.txt

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <alloca.h>

#define ARGC_ERROR 1
#define FILE_ERROR 2
#define FRAME_SIZE 256 
#define BUFLEN 256
#define FRAME_NUMBER 128

int page_table_num[256];
int page_table_frames[256];
int tlb[16][2];
int main_mem[256][FRAME_SIZE]; 
int page_fault_count = 0;  
int tlbh = 0;      
int available_frame = 0; 
int available_page = 0;  
int current_tlb_entry = 0; 
char address[BUFLEN];
int logic_add;
signed char buffer[FRAME_SIZE];
signed char value;

unsigned getpage(unsigned x) { return (0xff00 & x) >> 8; }

unsigned getoffset(unsigned x) { return (0xff & x); }

void read_store(int page, FILE** fadd, FILE** fstore){

  if (fseek(*fstore, page * FRAME_SIZE, SEEK_SET) != 0) {
    fprintf(stderr, "Reading Error\n");
  }
    
  if (fread(buffer, sizeof(signed char), FRAME_SIZE, *fstore) == 0) {
    fprintf(stderr, "Reading Error\n");        
  }
    
  for(int i = 0; i < FRAME_SIZE; i++){
      main_mem[available_frame][i] = buffer[i];
  }
    
  page_table_num[available_page] = page;
  page_table_frames[available_page] = available_frame;
    
  available_frame++;
  available_page++;
}

int tlb_contains(unsigned x){
  int i;
  for(i = 0; i < current_tlb_entry; i++){
        if(tlb[i][0] == x){
           return i;//hit
        }
    }
  return i;
} 

void update_tlb(int page, int frame){
    
  int i = tlb_contains(page); 
    
  if(i == current_tlb_entry){//if miss
    if(current_tlb_entry < 16){//not full
      tlb[current_tlb_entry][0] = page;
      tlb[current_tlb_entry][1] = frame;
    }
    else{
      for(i = 0; i < 16 - 1; i++){//if full
        tlb[i][0] = tlb[i + 1][0];
        tlb[i][1] = tlb[i + 1][1];
      }
      tlb[current_tlb_entry-1][0] = page;
      tlb[current_tlb_entry-1][1] = frame;
    }        
  }
  else{//if hit
    for(i = i; i < current_tlb_entry - 1; i++){      
      tlb[i][0] = tlb[i + 1][0]; //push everything forward once
      tlb[i][1] = tlb[i + 1][1];
    }
    if(current_tlb_entry < 16){//not full
      tlb[current_tlb_entry][0] = page;
      tlb[current_tlb_entry][1] = frame;
    }
    else{//if full put at the end
      tlb[current_tlb_entry-1][0] = page;
      tlb[current_tlb_entry-1][1] = frame;
    }
  }
  
  if(current_tlb_entry < 16){
    current_tlb_entry++;
  }    
}

void getframe(int logic_add, FILE** fadd, FILE** fstore){
    
  int page = getpage(logic_add);
  int offset = getoffset(logic_add);

  int frame = -1;
     
  for(int i = 0; i < 16; i++){
    if(tlb[i][0] == page){   
      frame = tlb[i][1];  
      tlbh++;           //tlb hit
    }
  }
    
    //tlb miss
    
  if(frame == -1){
    for(int i = 0; i < available_page; i++){
      if(page_table_num[i] == page){         //Check page table
        frame = page_table_frames[i];        //update frame if found
      }
    }
    if(frame == -1){                         //page table miss
      read_store(page, fadd, fstore);        //page fault
      page_fault_count++;                    
      frame = available_frame - 1;
    }
  }
    
  update_tlb(page, frame);

  value = main_mem[frame][offset];
    
}

void open_files(FILE** fadd, FILE** fcorr, FILE** fstore, int argc, char *argv[]) {
  *fstore = fopen("BACKING_STORE.bin", "rb");
    
  if (fstore == NULL) {
    fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");
    exit(FILE_ERROR);
  }
    
  //*fadd = fopen("addresses.txt", "r");

  if (argc != 2) {
        fprintf(stderr,"Usage: ./memmgr addresses.txt\n");
        exit(FILE_ERROR);
  }

  *fadd = fopen(argv[1], "r");

  if (fadd == NULL) {
    fprintf(stderr, "Could not open file: 'addresses.txt'\n");
    exit(ARGC_ERROR);
  }

  *fcorr = fopen("correct.txt", "r");
    
  if (fadd == NULL) {
    fprintf(stderr, "Could not open file: 'correct.txt'\n");
    exit(FILE_ERROR);
  }

}

void close_files(FILE* fadd, FILE* fcorr, FILE* fstore) {
  fclose(fcorr);
  fclose(fadd);
  fclose(fstore);
}

int main(int argc, char *argv[]){
  FILE* fadd;
  FILE* fstore;
  FILE* fcorr;


  char buf[BUFLEN];
  unsigned   page, offset, physical_add, frame = 0;
  unsigned   virt_add, phys_add, cvalue;  // read from file correct.txt


  open_files( &fadd, &fcorr, &fstore, argc, argv);
    
  int access_count = 0;

  while ( fgets(address, BUFLEN, fadd) != NULL) {
    fscanf(fcorr, "%s %s %d %s %s %d %s %d", buf, buf, &virt_add,
        buf, buf, &phys_add, buf, &cvalue); 
    
    logic_add = atoi(address);
    getframe(logic_add, &fadd, &fstore);//get frame & check tlb
    
    page   = getpage(  logic_add);
    offset = getoffset(logic_add);//get offset

    physical_add = access_count * FRAME_SIZE + offset;//read value from physical memory

    //assert(value == cvalue);
    //read BINARY_STORE and confirm value matches read value from correct.txt
    if(value != cvalue) break; 

    printf("logical: %d\t(page: %d,\toffset: %d) \t---> physical: %d -- passed\n", logic_add, page, offset, (frame << 8) | offset);

    access_count++;
    if(access_count%5==0){printf("\n");}
  }
    
  printf("\nAccess count   Tlb hit count   Page fault count   Tlb hit rate   Page fault rate\n");

  printf("%9d %12d %18d %18.4f %14.4f\n", access_count, tlbh, page_fault_count, tlbh / (double)access_count, page_fault_count / (double)access_count);

  close_files(fadd, fcorr, fstore);
    
  return 0;
}
