#ifdef _WIN32
# define LOLI_PATH_CHAR '\\'
# define LOLI_PATH_SLASH "\\"
# define LOLI_LIB_SUFFIX "dll"
#else
# define LOLI_PATH_CHAR '/'
# define LOLI_PATH_SLASH "/"
# define LOLI_LIB_SUFFIX "so"
#endif
