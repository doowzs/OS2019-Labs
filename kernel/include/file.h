#ifndef __FILE_H__
#define __FILE_H__

#include <common.h>
#include <vfs.h>

#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_RDWR   0x03

struct file {
  int fd;
  int refcnt;
  int flags;
  inode_t *inode;
  uint64_t offset;
};

struct inodeops {
  int (*open)(file_t *file, int flags);
  int (*close)(file_t *file);
  ssize_t (*read)(file_t *file, char *buf, size_t size);
  ssize_t (*write)(file_t *file, const char *buf, size_t size);
  off_t (*lseek)(file_t *file, off_t offset, int whence);
  int (*mkdir)(const char *name);
  int (*rmdir)(const char *name);
  int (*link)(const char *name, inode_t *inode);
  int (*unlink)(const char *name);
};

struct inode {
  int refcnt;
  void *ptr;
  char path[128];
  filesystem_t *fs;
  inodeops_t *ops;

  inode_t *parent;
  inode_t *fchild;
  inode_t *cousin;
};

inline inode_t *inode_search(inode_t *cur, const char *path) {
  for (inode_t *ip = cur->fchild; ip != NULL; ip = ip->cousin) {
    if (!strncmp(path, ip->path, strlen(ip->path))) {
      if (strlen(path) == strlen(ip->path)) return ip;
      else return inode_search(ip, path + strlen(ip->path));
    }
  }
  return cur;
}

#endif
