#include "loli_library.h"
#include "loli_alloc.h"

#ifdef _WIN32
#include <Windows.h>

void *loli_library_load(const char *path)
{
    return LoadLibraryA(path);
}

void *loli_library_get(void *source, const char *target)
{
    return GetProcAddress((HMODULE)source, target);
}

void loli_library_free(void *source)
{
    FreeLibrary((HMODULE)source);
}
#else
#include <dlfcn.h>

void *loli_library_load(const char *path)
{
    return dlopen(path, RTLD_LAZY);
}

void *loli_library_get(void *source, const char *name)
{
    return dlsym(source, name);
}

void loli_library_free(void *source)
{
    dlclose(source);
}
#endif
