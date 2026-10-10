#ifndef CALCLAB_STORE_H
#define CALCLAB_STORE_H
#include "app.h"
int cl_store_load(const char *dir, CLPersistent *s);
int cl_store_save(const char *dir, const CLPersistent *s);
int cl_export_history(const char *dir, const CLPersistent *s, char *name, size_t cap);
uint32_t cl_crc32(const unsigned char *s, size_t size);
#endif
