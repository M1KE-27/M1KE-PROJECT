/* ramfs.h - simple in-memory hierarchical filesystem */
#ifndef M1KE_RAMFS_H
#define M1KE_RAMFS_H
#include <stddef.h>
#include <stdbool.h>

#define FS_NAME_MAX 64

typedef enum { FS_FILE, FS_DIR } fs_type_t;

typedef struct fs_node {
    char             name[FS_NAME_MAX];
    fs_type_t        type;
    struct fs_node  *parent;
    struct fs_node  *children;   /* first child (dir) */
    struct fs_node  *next;       /* next sibling */
    char            *data;       /* file contents (heap) */
    size_t           size;       /* file size in bytes */
    size_t           cap;        /* allocated capacity */
} fs_node_t;

void        ramfs_init(void);
fs_node_t  *fs_root(void);

/* path resolution (absolute "/a/b" or relative to `cwd`) */
fs_node_t  *fs_resolve(fs_node_t *cwd, const char *path);

fs_node_t  *fs_create(fs_node_t *dir, const char *name, fs_type_t type);
fs_node_t  *fs_mkdir_p(const char *abspath);          /* make dirs as needed */
bool        fs_remove(fs_node_t *node);               /* recursive */

/* file content helpers */
int         fs_write(fs_node_t *file, const char *buf, size_t len);  /* overwrite */
int         fs_append(fs_node_t *file, const char *buf, size_t len);

/* convenience: write a string file at an absolute path (creates parents) */
fs_node_t  *fs_put(const char *abspath, const char *contents);

/* build an absolute path string into buf */
void        fs_abspath(fs_node_t *node, char *buf, size_t bufsz);

#endif
