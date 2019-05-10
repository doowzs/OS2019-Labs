#ifndef __COMMON_H__
#define __COMMON_H__

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEBUG
#include "debug.h"
#include "bmp.h"
#include "fat32.h"

struct DataSeg {
  void *head;
  void *tail;
  struct DataSeg *prev;
  struct DataSeg *next;
};

struct Image {
  char name[128];
  uint32_t cluster;
  size_t size;
  struct BMP bmp;
  struct Image *next;
};

enum ClusterTypes {
  TYPE_FDT, // file entry
  TYPE_BMP, // bmp image
  TYPE_EMP  // empty entry
};

void recover_images();
int get_cluster_type(void *, int);
void handle_bmp(void *);
void handle_fdt(void *, int, bool);
bool handle_fdt_aux(void *, int);

#endif
