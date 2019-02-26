#ifndef LOLI_LIBRARY_H
# define LOLI_LIBRARY_H

# include "loli_core_types.h"

void *loli_library_load(const char *);
void *loli_library_get(void *, const char *);
void loli_library_free(void *);

#endif
