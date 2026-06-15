/* registry.c - linked list of kernel objects */
#include "registry.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../drivers/console.h"

static kobject_t *head;

void reg_register(kobject_t *obj) {
    obj->next = head;
    head = obj;
}

kobject_t *reg_find(const char *name) {
    for (kobject_t *o = head; o; o = o->next)
        if (strcmp(o->name, name) == 0) return o;
    return 0;
}

int reg_count(void) {
    int n = 0;
    for (kobject_t *o = head; o; o = o->next) n++;
    return n;
}

void reg_list(const char *kind) {
    for (kobject_t *o = head; o; o = o->next) {
        if (kind && strcmp(o->kind, kind) != 0) continue;
        console_set_color(theme_accent(), COL_BLACK);
        kprintf("  %-14s", o->name);
        console_set_color(COL_GRAY, COL_BLACK);
        kprintf(" [%-9s] ", o->kind);
        console_set_color(COL_WHITE, COL_BLACK);
        kprintf("%s\n", o->summary ? o->summary : "");
    }
}
