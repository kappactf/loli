#ifndef LOLI_API_ALLOC_H
# define LOLI_API_ALLOC_H

# include <stdlib.h>

void *loli_malloc(size_t);
void *loli_realloc(void *, size_t);
void loli_free(void *);

#endif
