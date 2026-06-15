/* config.h - human-readable key/value configuration (text, ramfs-backed) */
#ifndef M1KE_CONFIG_H
#define M1KE_CONFIG_H
#include <stdbool.h>

#define CONFIG_PATH "/etc/m1ke/system.conf"

void        config_init(void);                       /* load defaults + file */
const char *config_get(const char *key, const char *def);
int         config_get_int(const char *key, int def);
bool        config_set(const char *key, const char *val);
bool        config_set_int(const char *key, int val);
void        config_save(void);                        /* write text to ramfs */
void        config_dump(const char *prefix);          /* print keys (prefix or NULL) */
int         config_count(void);

#endif
