/* ramfs.c - in-memory hierarchical filesystem backed by the kernel heap */
#include "ramfs.h"
#include "../mm/heap.h"
#include "../lib/string.h"

static fs_node_t *root;

static fs_node_t *new_node(const char *name, fs_type_t type) {
    fs_node_t *n = (fs_node_t *)kcalloc(1, sizeof(fs_node_t));
    if (!n) return 0;
    strncpy(n->name, name, FS_NAME_MAX - 1);
    n->type = type;
    return n;
}

void ramfs_init(void) {
    root = new_node("", FS_DIR);
}

fs_node_t *fs_root(void) { return root; }

static fs_node_t *find_child(fs_node_t *dir, const char *name, size_t len) {
    if (!dir || dir->type != FS_DIR) return 0;
    for (fs_node_t *c = dir->children; c; c = c->next) {
        if (strlen(c->name) == len && strncmp(c->name, name, len) == 0) return c;
    }
    return 0;
}

fs_node_t *fs_create(fs_node_t *dir, const char *name, fs_type_t type) {
    if (!dir || dir->type != FS_DIR) return 0;
    fs_node_t *existing = find_child(dir, name, strlen(name));
    if (existing) return existing;
    fs_node_t *n = new_node(name, type);
    if (!n) return 0;
    n->parent = dir;
    n->next = dir->children;
    dir->children = n;
    return n;
}

fs_node_t *fs_resolve(fs_node_t *cwd, const char *path) {
    fs_node_t *cur = (path[0] == '/') ? root : (cwd ? cwd : root);
    const char *p = path;
    while (*p == '/') p++;

    while (*p) {
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);

        if (len == 1 && start[0] == '.') {
            /* stay */
        } else if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (cur->parent) cur = cur->parent;
        } else {
            fs_node_t *child = find_child(cur, start, len);
            if (!child) return 0;
            cur = child;
        }
        while (*p == '/') p++;
    }
    return cur;
}

/* split abspath into parent dir + final component */
fs_node_t *fs_mkdir_p(const char *abspath) {
    fs_node_t *cur = root;
    const char *p = abspath;
    while (*p == '/') p++;
    while (*p) {
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        char name[FS_NAME_MAX];
        if (len >= FS_NAME_MAX) len = FS_NAME_MAX - 1;
        memcpy(name, start, len);
        name[len] = 0;
        if (len) {
            fs_node_t *child = find_child(cur, name, strlen(name));
            if (!child) child = fs_create(cur, name, FS_DIR);
            if (!child) return 0;
            cur = child;
        }
        while (*p == '/') p++;
    }
    return cur;
}

static void unlink_from_parent(fs_node_t *node) {
    fs_node_t *par = node->parent;
    if (!par) return;
    fs_node_t **pp = &par->children;
    while (*pp) {
        if (*pp == node) { *pp = node->next; break; }
        pp = &(*pp)->next;
    }
}

bool fs_remove(fs_node_t *node) {
    if (!node || node == root) return false;
    /* recursively free children */
    fs_node_t *c = node->children;
    while (c) {
        fs_node_t *nxt = c->next;
        c->parent = node;          /* ensure unlink works */
        fs_remove(c);
        c = nxt;
    }
    unlink_from_parent(node);
    if (node->data) kfree(node->data);
    kfree(node);
    return true;
}

int fs_write(fs_node_t *file, const char *buf, size_t len) {
    if (!file || file->type != FS_FILE) return -1;
    if (len + 1 > file->cap) {
        char *nb = (char *)krealloc(file->data, len + 1);
        if (!nb) return -1;
        file->data = nb;
        file->cap = len + 1;
    }
    memcpy(file->data, buf, len);
    file->data[len] = 0;
    file->size = len;
    return (int)len;
}

int fs_append(fs_node_t *file, const char *buf, size_t len) {
    if (!file || file->type != FS_FILE) return -1;
    size_t need = file->size + len + 1;
    if (need > file->cap) {
        char *nb = (char *)krealloc(file->data, need);
        if (!nb) return -1;
        file->data = nb;
        file->cap = need;
    }
    memcpy(file->data + file->size, buf, len);
    file->size += len;
    file->data[file->size] = 0;
    return (int)len;
}

fs_node_t *fs_put(const char *abspath, const char *contents) {
    /* separate dir part and filename */
    char dirpart[256];
    const char *slash = strrchr(abspath, '/');
    if (!slash) return 0;
    size_t dlen = (size_t)(slash - abspath);
    if (dlen >= sizeof(dirpart)) return 0;
    memcpy(dirpart, abspath, dlen);
    dirpart[dlen] = 0;
    fs_node_t *dir = (dlen == 0) ? root : fs_mkdir_p(dirpart);
    if (!dir) return 0;
    fs_node_t *file = fs_create(dir, slash + 1, FS_FILE);
    if (!file) return 0;
    fs_write(file, contents, strlen(contents));
    return file;
}

void fs_abspath(fs_node_t *node, char *buf, size_t bufsz) {
    if (!node || !node->parent) { strncpy(buf, "/", bufsz); return; }
    /* collect names up to root */
    const char *parts[32];
    int n = 0;
    for (fs_node_t *c = node; c && c->parent && n < 32; c = c->parent)
        parts[n++] = c->name;
    size_t pos = 0;
    if (n == 0) { strncpy(buf, "/", bufsz); return; }
    for (int i = n - 1; i >= 0 && pos + 1 < bufsz; i--) {
        buf[pos++] = '/';
        const char *s = parts[i];
        while (*s && pos + 1 < bufsz) buf[pos++] = *s++;
    }
    buf[pos] = 0;
}
