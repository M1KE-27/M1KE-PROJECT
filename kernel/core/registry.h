/* registry.h - kernel object registry: every subsystem is observable here */
#ifndef M1KE_REGISTRY_H
#define M1KE_REGISTRY_H

typedef struct kobject {
    const char *name;
    const char *kind;        /* "subsystem" | "service" | "driver" ... */
    const char *summary;
    void (*inspect)(void);                 /* print detailed state */
    int  (*ctl)(int argc, char **argv);    /* handle "m1kectl <name> <args>" */
    struct kobject *next;
} kobject_t;

void       reg_register(kobject_t *obj);
kobject_t *reg_find(const char *name);
void       reg_list(const char *kind);     /* kind == NULL -> all */
int        reg_count(void);

#endif
