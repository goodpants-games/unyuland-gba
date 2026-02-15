// malloc in external work ram

#ifndef EMALLOC_H
#define EMALLOC_H

#include <stddef.h>

void* emalloc(size_t sz);
void efree(void *ptr);

#endif