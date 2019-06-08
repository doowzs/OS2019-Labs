#include <common.h>
#include <file.h>
#include <vfs.h>

typedef struct commonfs_params {
  int32_t blk_size;
  int32_t map_head;
  int32_t data_head;
  int32_t min_free;
} commonfs_params_t;

typedef union commonfs_entry {
  char content[32];
  struct {
    int32_t head;
    int16_t type;
    int16_t flags;
    char path[24];
  };
} commonfs_entry_t;

int32_t commonfs_get_next_blk(filesystem_t *fs, int32_t blk) {
  int32_t ret = 0;
  commonfs_params_t *params = (commonfs_params_t *)fs->root->ptr;
  off_t offset = params->map_head + blk * sizeof(int32_t);
  fs->dev->ops->read(fs->dev, offset, (void *)(&ret), sizeof(int32_t));
  return ret;
}

commonfs_entry_t commonfs_get_entry(filesystem_t *fs, int32_t blk) {
  commonfs_entry_t ret;
  commonfs_params_t *params = (commonfs_params_t *)fs->root->ptr;
  off_t offset = params->data_head + blk * params->blk_size;
  fs->dev->ops->read(fs->dev, offset, (void *)(&ret), sizeof(commonfs_entry_t));
  return ret;
}

size_t commonfs_get_file_size(filesystem_t *fs, commonfs_entry_t *entry) {
  size_t ret = 0;
  int32_t blk = entry->head;
  commonfs_params_t *params = (commonfs_params_t *)fs->root->ptr;
  while (true) {
    int32_t next = commonfs_get_next_blk(fs, blk);
    if (next) {
      ret += params->blk_size;
    } else {
      commonfs_entry_t ce = commonfs_get_entry(fs, blk);
      for (size_t i = 0; i < params->blk_size; ++i) {
        if (!ce.content[i]) break;
        ++ret;
      }
      break;
    }
    blk = next;
  }
  return ret;
}

const char *inode_types_human[] = {
  "INVL",
  "MNTP",
  "DIRC",
  "FILE",
  "DEVI",
  "PROC",
  "PROX",
  "LINK",
};

inode_t *root;

inodeops_t common_ops = {
  .open    = common_open,
  .close   = common_close,
  .read    = common_read,
  .write   = common_write,
  .lseek   = common_lseek,
  .mkdir   = common_mkdir,
  .rmdir   = common_rmdir,
  .link    = common_link,
  .unlink  = common_unlink,
  .readdir = common_readdir,
};

fsops_t commonfs_ops = {
  .init   = commonfs_init,
  .lookup = commonfs_lookup,
  .close  = commonfs_close,
};

filesystem_t commonfs = {
  .ops = &commonfs_ops,
  .dev = NULL,
};

int common_open(file_t *file, int flags) {
  return 0;
}

int common_close(file_t *file) {
  return 0;
}

ssize_t common_read(file_t *file, char *buf, size_t size) {
  Assert(file->inode->fs->dev, "fs with no device");
  device_t *dev = file->inode->fs->dev;
  off_t offset = (off_t)file->inode->ptr + file->inode->offset;
  if (offset > file->inode->size) {
    buf[0] = '\0';
    return 0;
  } else {
    ssize_t ret = dev->ops->read(dev, offset, buf, size);
    file->inode->offset += (off_t)ret;
    return ret;
  }
}

ssize_t common_write(file_t *file, const char *buf, size_t size) {
  Assert(file->inode->fs->dev, "fs with no device");
  device_t *dev = file->inode->fs->dev;
  off_t offset = (off_t)file->inode->ptr + file->inode->offset;
  ssize_t ret = dev->ops->write(dev, offset, buf, size);
  file->inode->offset += (off_t)ret;
  if (file->inode->offset > file->inode->size) file->inode->size = (size_t)file->inode->offset;
  return ret;
}

off_t common_lseek(file_t *file, off_t offset, int whence) {
  switch (whence) {
    case SEEK_SET:
      file->inode->offset = offset;
      break;
    case SEEK_CUR:
      file->inode->offset += offset;
      break;
    case SEEK_END:
    default:
      file->inode->offset = file->inode->size + offset;
      break;
  }
  return file->inode->offset;
}

int common_mkdir(const char *name) {
  return 0;
}

int common_rmdir(const char *name) {
  inode_t *ip = inode_search(root, name);
  if (ip->type != TYPE_DIRC) {
    return -1;
  }
  inode_delete(ip);
  return 0;
}

int common_link(const char *name, inode_t *inode) {
  inode_t *pp = inode_search(root, name);
  if (strlen(pp->path) == strlen(name)) {
    return -1;
  }

  inode_t *ip = pmm->alloc(sizeof(inode_t));
  ip->refcnt = 0;
  ip->type = TYPE_LINK;
  ip->ptr = (void *)inode;
  sprintf(ip->path, name);
  ip->offset = 0;
  ip->size = sizeof(void *);
  ip->fs = pp->fs;
  ip->ops = pmm->alloc(sizeof(inodeops_t));
  memcpy(ip->ops, &common_ops, sizeof(inodeops_t));

  ip->parent = pp;
  ip->fchild = NULL;
  ip->cousin = NULL;
  inode_insert(pp, ip);
  return 0;
}

int common_unlink(const char *name) {
  inode_t *ip = inode_search(root, name);
  if (ip->type != TYPE_FILE || ip->type != TYPE_LINK) {
    return -1;
  }
  inode_delete(ip);
  return 0;
}

void common_readdir(inode_t *inode, char *ret) {
  sprintf(ret, "ls %s:\n", inode->path);
  strcat(ret, " + TYPE PRIV FILENAME\n");
  for (inode_t *ip = inode->fchild; ip != NULL; ip = ip->cousin) {
    CLog(FG_PURPLE, "%s %s", inode_types_human[ip->type], ip->path);
    strcat(ret, " - ");
    strcat(ret, inode_types_human[ip->type]);
    strcat(ret, " ");

    if (ip->type == TYPE_DIRC || ip->type == TYPE_MNTP) {
      strcat(ret, "D ");
    } else if (ip->type == TYPE_LINK) {
      strcat(ret, "L ");
    } else {
      strcat(ret, "- ");
    }
    strcat(ret, ip->flags & P_RD ? "R" : "-");
    strcat(ret, ip->flags & P_WR ? "W" : "-");
    strcat(ret, " ");

    strcat(ret, ip->path);
    strcat(ret, "\n");
  }
}

void commonfs_init(filesystem_t *fs, const char *path, device_t *dev) {
  fs->dev = dev;
  fs->root = pmm->alloc(sizeof(inode_t));

  root = fs->root;
  root->type = TYPE_MNTP;
  root->flags = P_RD;
  root->ptr = NULL;
  sprintf(root->path, path);
  root->fs = fs;
  root->ops = &common_ops;
  root->parent = root;
  root->fchild = NULL;
  root->cousin = NULL;

  root->ptr = pmm->alloc(sizeof(commonfs_params_t));
  dev->ops->read(dev, 0, root->ptr, sizeof(commonfs_params_t));
  int32_t blk = 1;
  while (blk) {
    commonfs_entry_t entry = commonfs_get_entry(fs, blk);  
    inode_t *pp = fs->ops->lookup(fs, entry.path, O_CREAT);
    inode_t *ip = pmm->alloc(sizeof(inode_t));
    ip->refcnt = 0;
    ip->type = (int)entry.type;
    ip->flags = (int)entry.flags;
    ip->ptr = (void *)(entry.head);
    sprintf(ip->path, entry.path);
    ip->offset = 0;
    ip->size = commonfs_get_file_size(fs, &entry);
    ip->fs = fs;
    ip->ops = pmm->alloc(sizeof(inodeops_t));
    memcpy(ip->ops, &common_ops, sizeof(inodeops_t));

    ip->parent = pp;
    ip->fchild = NULL;
    ip->cousin = NULL;
    inode_insert(pp, ip);

    blk = commonfs_get_next_blk(fs, blk);
  }
}

inode_t *commonfs_lookup(filesystem_t *fs, const char *path, int flags) {
  inode_t *ip = inode_search(root, path);
  if (strlen(ip->path) == strlen(path)) {
    return ip;
  } else {
    if (flags & O_CREAT) {
      size_t len = strlen(path);
      for (size_t i = strlen(ip->path) + strlen("/"); i < len; ++i) {
        if (path[i] == '/') return NULL;
      }
      
      inode_t *pp = ip;
      ip = pmm->alloc(sizeof(inode_t));
      ip->refcnt = 0;
      ip->type = TYPE_FILE;
      ip->ptr = NULL;
      ip->size = 0;
      ip->fs = fs;
      ip->ops = pmm->alloc(sizeof(inodeops_t));
      memcpy(ip->ops, &common_ops, sizeof(inodeops_t));

      ip->parent = pp;
      ip->fchild = NULL;
      ip->cousin = NULL;
      inode_insert(pp, ip);
      return ip;
    } else {
      return NULL;
    }
  }
}

int commonfs_close(inode_t *inode) {
  if (inode->size <= 0) {
    inode_remove(inode->parent, inode);
  }
  return 0;
}
