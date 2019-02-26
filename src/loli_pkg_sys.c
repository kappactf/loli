
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h> 

#include "loli.h"
#include "loli_vm.h"

#ifdef _WIN32
#ifndef setenv
int setenv(char *name, char *value, int overwrite)
{
    char *env = getenv(name);
    if (env && !overwrite) {
        return 0;
    }
    size_t size = strlen(name)+strlen(name)+2;
    char *buffer = (char *)malloc(size);
    if (!buffer) {
        return 0;
    }
    sprintf(buffer, "%s=%s", name, value);
    int result = putenv(buffer);
    free(buffer);
    return result;
}
#endif
#endif

const char *loli_sys_info_table[] = {
    "\0\0"
    ,"F\0listdir\0(String): List[String]"
    ,"F\0rmdir\0(String): Boolean"
    ,"F\0remove\0(String): Boolean"
    ,"F\0mkdir\0(String): Boolean"
    ,"F\0exists\0(String): Boolean"
    ,"F\0is_dir\0(String): Boolean"
    ,"F\0is_file\0(String): Boolean"
    ,"F\0exit\0(*Integer)"
    ,"F\0getenv\0(String): Option[String]"
    ,"F\0setenv\0(String, String): Boolean"
    ,"F\0recursion_limit\0: Integer"
    ,"F\0set_recursion_limit\0(Integer)"
    ,"R\0argv\0List[String]"
    ,"Z"
};
void loli_sys__listdir(loli_state *s);
void loli_sys__rmdir(loli_state *s);
void loli_sys__remove(loli_state *s);
void loli_sys__mkdir(loli_state *s);
void loli_sys__exists(loli_state *s);
void loli_sys__is_dir(loli_state *s);
void loli_sys__is_file(loli_state *s);
void loli_sys__exit(loli_state *s);
void loli_sys__getenv(loli_state *);
void loli_sys__setenv(loli_state *);
void loli_sys__recursion_limit(loli_state *);
void loli_sys__set_recursion_limit(loli_state *);
void loli_sys_var_argv(loli_state *);
loli_call_entry_func loli_sys_call_table[] = {
    NULL,
    loli_sys__listdir,
    loli_sys__rmdir,
    loli_sys__remove,
    loli_sys__mkdir,
    loli_sys__exists,
    loli_sys__is_dir,
    loli_sys__is_file,
    loli_sys__exit,
    loli_sys__getenv,
    loli_sys__setenv,
    loli_sys__recursion_limit,
    loli_sys__set_recursion_limit,
    loli_sys_var_argv,
};

void loli_sys__listdir(loli_state *s)
{
    char *path = loli_arg_string_raw(s, 0);
    
    DIR *dp;
    struct dirent *dir;
    
    dp = opendir(path);
    
    if (!dp) {
        char buffer[128];
#ifdef _WIN32
        strerror_s(buffer, sizeof(buffer), errno);
#else
        strerror_r(errno, buffer, sizeof(buffer));
#endif
        loli_IOError(s, "Errno %d: %s (%s).", errno, buffer, path);
    }
    
    int lv_size = 0;
    int i = 0;
    
    while ((dir = readdir(dp)) != NULL) {
        lv_size++;
    }
    rewinddir(dp);
    
    loli_container_val *lv = loli_push_list(s, lv_size);
    
    while ((dir = readdir(dp)) != NULL && i < lv_size) {
        loli_push_string(s, dir->d_name);
        loli_con_set_from_stack(s, lv, i); 
        
        i++;       
    }
    closedir(dp);
    
    if (errno) {
        char buffer[128];
#ifdef _WIN32
        strerror_s(buffer, sizeof(buffer), errno);
#else
        strerror_r(errno, buffer, sizeof(buffer));
#endif
        loli_IOError(s, "Errno %d: %s (%s).", errno, buffer, path);    
    }
    
    loli_return_top(s);
}
void loli_sys__rmdir(loli_state *s)
{
    char *path = loli_arg_string_raw(s, 0);
    
    if (rmdir(path) != 0) {
        char buffer[128];
#ifdef _WIN32
        strerror_s(buffer, sizeof(buffer), errno);
#else
        strerror_r(errno, buffer, sizeof(buffer));
#endif
        loli_IOError(s, "Errno %d: %s (%s).", errno, buffer, path);
        loli_return_boolean(s, 0); // hmmmmmmmm
    }
    
    loli_return_boolean(s, 1);
}

void loli_sys__remove(loli_state *s)
{
    char *path = loli_arg_string_raw(s, 0);
    
    if (remove(path) != 0) {
        char buffer[128];
#ifdef _WIN32
        strerror_s(buffer, sizeof(buffer), errno);
#else
        strerror_r(errno, buffer, sizeof(buffer));
#endif
        loli_IOError(s, "Errno %d: %s (%s).", errno, buffer, path);
        loli_return_boolean(s, 0); // hmmmmmmmm
    }
    
    loli_return_boolean(s, 1);
}

void loli_sys__mkdir(loli_state *s)
{
    char *path = loli_arg_string_raw(s, 0);
    
#ifdef _WIN32
    if (mkdir(path) != 0) {
#else
    if (mkdir(path, 777) != 0) {
#endif
        char buffer[128];
#ifdef _WIN32
        strerror_s(buffer, sizeof(buffer), errno);
#else
        strerror_r(errno, buffer, sizeof(buffer));
#endif
        loli_IOError(s, "Errno %d: %s (%s).", errno, buffer, path);
        loli_return_boolean(s, 0); // hmmmmmmmm
    }
    
    loli_return_boolean(s, 1);
}

void loli_sys__exists(loli_state *s)
{
    char *path = loli_arg_string_raw(s, 0);
    
    if (access(path, F_OK) == -1) {
        loli_return_boolean(s, 0);
    } else {
        loli_return_boolean(s, 1);
    }
}

void loli_sys__is_dir(loli_state *s)
{
    
    struct stat statbuf;
    char *path = loli_arg_string_raw(s, 0);
    
    if (stat(path, &statbuf) != 0) {
        char buffer[128];
#ifdef _WIN32
        strerror_s(buffer, sizeof(buffer), errno);
#else
        strerror_r(errno, buffer, sizeof(buffer));
#endif
        loli_IOError(s, "Errno %d: %s (%s).", errno, buffer, path);
    }
    
    if (S_ISDIR(statbuf.st_mode)) {
        loli_return_boolean(s, 1);
    } else {
        loli_return_boolean(s, 0);
    }
}

void loli_sys__is_file(loli_state *s)
{
    
    struct stat statbuf;
    char *path = loli_arg_string_raw(s, 0);
    
    if (stat(path, &statbuf) != 0) {
        char buffer[128];
#ifdef _WIN32
        strerror_s(buffer, sizeof(buffer), errno);
#else
        strerror_r(errno, buffer, sizeof(buffer));
#endif
        loli_IOError(s, "Errno %d: %s (%s).", errno, buffer, path);
    }
    
    if (S_ISREG(statbuf.st_mode)) {
        loli_return_boolean(s, 1);
    } else {
        loli_return_boolean(s, 0);
    }
}

void loli_sys_var_argv(loli_state *s)
{
    loli_config *config = loli_config_get(s);
    int opt_argc = config->argc;
    char **opt_argv = config->argv;
    loli_container_val *lv = loli_push_list(s, opt_argc);

    int i;
    for (i = 0;i < opt_argc;i++) {
        loli_push_string(s, opt_argv[i]);
        loli_con_set_from_stack(s, lv, i);
    }
}

void loli_sys__getenv(loli_state *s)
{
    char *env = getenv(loli_arg_string_raw(s, 0));

    if (env) {
        loli_container_val *variant = loli_push_some(s);
        loli_push_string(s, env);
        loli_con_set_from_stack(s, variant, 0);
        loli_return_top(s);
    }
    else
        loli_return_none(s);
}

void loli_sys__setenv(loli_state *s)
{
    char *env = loli_arg_string_raw(s, 0);  
    char *value = loli_arg_string_raw(s, 1);
    
    if (setenv(env, value, 1) == 0) {
        loli_return_boolean(s, 1);
    }
    else
        loli_return_boolean(s, 0);
}

void loli_sys__recursion_limit(loli_state *s)
{
    loli_return_integer(s, s->depth_max);
}

void loli_sys__set_recursion_limit(loli_state *s)
{
    int64_t limit = loli_arg_integer(s, 0);

    if (limit < 1 || limit > INT32_MAX)
        loli_ValueError(s, "Limit value (%ld) is not reasonable.", limit);

    if (limit < s->call_depth)
        loli_ValueError(s,
            "Limit value (%ld) is lower than the current recursion depth.",
            limit);

    s->depth_max = (int32_t)limit;
    loli_return_unit(s);
}

void loli_sys__exit(loli_state *s)
{
    int64_t exit_code = 0;
    if (loli_arg_count(s) == 1)
        exit_code = loli_arg_integer(s, 0);
    
    exit(exit_code);    
    loli_return_unit(s); // should never be reached, but who knows...
}

