/* See LICENSE file for license details.
 *
 * pmslog.c:
 *         - compile into a shared library
 *         - override syscalls related to installation when running make install
 *         - log all syscalls to a file
 *
 * pms will then use the files that are saved to automate an uninstall
 *
 * compile steps: cc -shared -fPIC -o pmslog.so pmslog.c -ldl
 * cc - c compiler (any c compiler will work)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

// #include "../config.h"

/* A static file pointer to our log file */
static FILE *log_file = NULL;

/* Called automatically when the shared library is loaded */
__attribute__((constructor))
static void
init_logger(void)
{
        log_file = fopen("/tmp/install_log.txt", "a"); /* test file */
        if (!log_file)
                log_file = stderr;  /* fallback if log file can’t be opened */
}

/* Called automatically when the shared library is unloaded */
__attribute__((destructor))
static void
close_logger(void)
{
        if (log_file && log_file != stderr)
                fclose(log_file);
}

/* Override for open(2) */
int
open(const char *pathname, int flags, ...)
{
        va_list args;
        mode_t mode = 0;
        if (flags & O_CREAT) {
                va_start(args, flags);
                mode = va_arg(args, int);
                va_end(args);
        }
        if (log_file) {
                fprintf(log_file, "open: %s, flags: %d\n", pathname, flags);
                fflush(log_file);
        }
        /* Get the pointer to the real open() function */
        typedef int (*orig_open_t)(const char *, int, ...);
        orig_open_t real_open = (orig_open_t)dlsym(RTLD_NEXT, "open");
        if (flags & O_CREAT)
                return real_open(pathname, flags, mode);
        else
                return real_open(pathname, flags);
}

