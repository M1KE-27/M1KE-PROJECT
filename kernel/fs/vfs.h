/* vfs.h - Virtual File System: a mount table over pluggable filesystem backends.
 *
 * Path-based design: each mounted filesystem provides an ops table that operates
 * on paths relative to its mount point. ramfs is the first backend (mounted at
 * "/"); a future FAT32 driver mounts elsewhere (e.g. "/mnt") with the same ops,
 * so apps keep using vfs_* without knowing the backend. */
#ifndef M1KE_VFS_H
#define M1KE_VFS_H
#include <stdint.h>

typedef enum { VNODE_FILE, VNODE_DIR } vtype_t;

typedef struct { vtype_t type; uint32_t size; } vstat_t;

/* called once per directory entry during vfs_readdir */
typedef void (*vfs_dir_cb)(const char *name, vtype_t type, uint32_t size, void *ud);

struct filesystem;
typedef struct filesystem {
    char  name[16];
    void *data;                                                  /* backend private */
    int (*stat)   (struct filesystem *, const char *path, vstat_t *out);
    int (*read)   (struct filesystem *, const char *path, char *buf, uint32_t off, uint32_t len);
    int (*write)  (struct filesystem *, const char *path, const char *buf, uint32_t len);
    int (*mkdir)  (struct filesystem *, const char *path);
    int (*create) (struct filesystem *, const char *path);
    int (*unlink) (struct filesystem *, const char *path);
    int (*readdir)(struct filesystem *, const char *path, vfs_dir_cb cb, void *ud);
} filesystem_t;

void vfs_init(void);
int  vfs_mount(const char *mountpoint, filesystem_t *fs);

/* path-based operations (absolute paths) */
int  vfs_stat   (const char *path, vstat_t *out);
int  vfs_read   (const char *path, char *buf, uint32_t off, uint32_t len);
int  vfs_write  (const char *path, const char *buf, uint32_t len);
int  vfs_mkdir  (const char *path);
int  vfs_create (const char *path);
int  vfs_unlink (const char *path);
int  vfs_readdir(const char *path, vfs_dir_cb cb, void *ud);

void vfs_list_mounts(void (*cb)(const char *mountpoint, const char *fsname, void *ud), void *ud);

#endif
