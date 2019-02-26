#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loli.h"

char * loli_version_str = "Loli programming language, v." LOLI_VERSION "\n";
char * loli_logo = "LL             lll iii \n"
                   "LL       oooo  lll    \n"
                   "LL      oo  oo lll iii\n"
                   "LL      oo  oo lll iii\n"
                   "LLLLLLL  oooo  lll iii\n\n"
                   "Loli programming language, v." LOLI_VERSION "\n"
                   "(c) Loli Foundation\n\n";

static void usage()
{
    printf(loli_logo);
    
    fputs("Usage: loli [option] [file]\n"
          "Options:\n"
          "  -h [--help]            Print this message and exit.\n"
          "  -r [--render-mode]     Render-mode\n"
          "  -i [--input] <string>  Execute the inputed string\n"
          "  -v [--version]         Get curent loli version and exit.\n"
          "  <file>                 Execute the inputed file\n", stderr);
    exit(EXIT_FAILURE);
}

int is_file;
int do_tags = 0;
int gc_start = -1;
int gc_multiplier = -1;
char *to_process = NULL;

static void process_args(int argc, char **argv, int *argc_offset)
{
    int i;
    for (i = 1;i < argc;i++) {
        char *arg = argv[i];
        if (strcmp("--help", arg) == 0 || strcmp("-h", arg) == 0) usage();
        else if (strcmp("--render-mode", arg) == 0 || strcmp("-r", arg) == 0) do_tags = 1;
        else if (strcmp("--input", arg) == 0 || strcmp("-i", arg) == 0) {
            i++;
            if (i == argc)
                usage();

            is_file = 0;
            break;
        } else if (strcmp("--version", arg) == 0 || strcmp("-v", arg) == 0) {
            printf(loli_version_str);
            exit(EXIT_SUCCESS);
        } else {
            is_file = 1;
            break;
        }
    }

    to_process = argv[i];
    *argc_offset = i;
}

void repl(void) 
{
    char expr[256];
    loli_config config;
    loli_config_init(&config);
    loli_state *state = loli_new_state(&config);
    int result;
    char *output;

    printf(loli_logo);

    for (;;) {
        output = NULL;
        printf("loli> ");

        fgets(expr, 256, stdin);
        if(strcmp(expr,"") == 0 
            || strcmp(expr,"\n") == 0 
            || strcmp(expr,"\r\n") == 0)
            continue;

        result = loli_load_string(state, "<repl>", expr);

        if (result) {
            result = loli_parse_expr(state, &output);
        } else { 
            fputs(loli_error_message(state), stderr);
            continue;
        }
        if (result && output) {
            printf("%s\n", output);
            continue;
        }

        result = loli_load_string(state, "<repl>", expr);
        if (result) {
            result = loli_parse_content(state);
        }
				
        if (result == 0) {
            fputs(loli_error_message(state), stderr);
            continue;
        }
    }
}

int main(int argc, char **argv)
{
    int argc_offset;
    process_args(argc, argv, &argc_offset);

    if (to_process == NULL) repl();
    
    loli_config config;
    loli_config_init(&config);

    if (gc_start != -1) config.gc_start = gc_start;
    if (gc_multiplier != -1) config.gc_multiplier = gc_multiplier;

    config.argc = argc - argc_offset;
    config.argv = argv + argc_offset;

    loli_state *state = loli_new_state(&config);

    int result;

    if (is_file == 1) result = loli_load_file(state, to_process);
    else result = loli_load_string(state, "<cli>", to_process);

    if (result){
        if (do_tags == 0) result = loli_parse_content(state);
        else result = loli_render_content(state);
    }

    if (result == 0) fputs(loli_error_message(state), stderr);
    
    loli_free_state(state);

    if (result == 0) exit(EXIT_FAILURE);

    exit(EXIT_SUCCESS);
}
