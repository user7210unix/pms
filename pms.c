/* See LICENSE file for license details.
 * 
 * pms (or Pack My Sh*t) is a minimal package manager for building packages from
 * source code. It is designed to be simple and easy to use, with a focus on
 * simplicity and minimalism. It functions in a similar way to the Arch Linux
 * PKGBUILD system and Gentoo's ebuild system, but with a much simpler and more 
 * minimal implementation.
 *
 * pms is designed to be used with a TOML package build file, check README.md for
 * the example package build file on how to use it.
 */
#include <curl/curl.h>
#include <getopt.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <toml.h>

#include "config.h"
#include "pms.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

static void
die(const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        exit(1);
}

static void
cleanup_pkg(Toml *t)
{
        size_t i;

        if (!t)
                return;

        free(t->pkg.name);
        free(t->pkg.version);
        free(t->pkg.author);
        free(t->pkg.type);

        for (i=0; i < t->pkg.nsrc; i++)
                free(t->pkg.src[i]);
        free(t->pkg.src);

        for (i=0; i < t->deps.ndeps; i++)
                free(t->deps.deps[i]);
        free(t->deps.deps);

        for (i=0; i < t->build.ncompile; i++)
                free(t->build.compile[i]);
        free(t->build.compile);

        for (i=0; i < t->build.ninstall; i++)
                free(t->build.install[i]);
        free(t->build.install);

        for (i=0; i < t->build.nuninstall; i++)
                free(t->build.uninstall[i]);
        free(t->build.uninstall);

}

static int
tomlgetarray(toml_array_t *arr, char ***dest, size_t *n)
{
        int i, len;
        toml_value_t val;

        if (!arr)
                return 0;

        len = toml_array_len(arr);
        if (len < 0)
                return -1;

        *n = len;
        *dest = calloc(*n, sizeof(char *));
        if (!*dest)
                return -1;

        for (i = 0; i < len; i++) {
                val = toml_array_string(arr, i);
                if (!val.ok) {
                        while (--i >= 0)
                                free((*dest)[i]);
                        free(*dest);
                        return -1;
                }
                (*dest)[i] = strdup(val.u.s);
                free(val.u.s);
                if (!(*dest)[i]) {
                        while (--i >= 0)
                                free((*dest)[i]);
                        free(*dest);
                        return -1;
                }
        }
        return 0;
}

int
parsetoml(const char *file, Toml *t)
{
        FILE *fp;
        char errbuf[200];
        toml_table_t *conf;
        toml_table_t *pkg;
        toml_table_t *build;
        toml_array_t *arr;
        toml_value_t val;

        if (!(fp = fopen(file, "r")))
                return -1;

        conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
        fclose(fp);
        if (!conf) {
                fprintf(stderr, "error parsing TOML: %s\n", errbuf);
                return -1;
        }

        /* parse package section */
        pkg = toml_table_table(conf, "package");
        if (!pkg)
                goto error;

        /* get package details */
        val = toml_table_string(pkg, "name");
        if (!val.ok)
                goto error;
        t->pkg.name = strdup(val.u.s);
        free(val.u.s);

        val = toml_table_string(pkg, "version");
        if (!val.ok)
                goto error;
        t->pkg.version = val.u.s;

        val = toml_table_string(pkg, "author");
        if (val.ok) {
                t->pkg.author = val.u.s;
        }

        val = toml_table_string(pkg, "type");
        if (val.ok) {
                t->pkg.type = val.u.s;
        }

        /* parse arrays */
        arr = toml_table_array(pkg, "source");
        if (!arr || tomlgetarray(arr, &t->pkg.src, &t->pkg.nsrc) < 0)
                goto error;

        arr = toml_table_array(pkg, "depends");
        if (!arr || tomlgetarray(arr, &t->deps.deps, &t->deps.ndeps) < 0)
                goto error;

        /* parse build section */
        if ((build = toml_table_table(conf, "build"))) {
                arr = toml_table_array(build, "compile");
                if (!arr || tomlgetarray(arr, &t->build.compile, &t->build.ncompile) < 0)
                        goto error;

                arr = toml_table_array(build, "install");
                if (!arr || tomlgetarray(arr, &t->build.install, &t->build.ninstall) < 0)
                        goto error;

                /* optional uninstall parameter */
                arr = toml_table_array(build, "uninstall");
                if (arr) {
                        if (tomlgetarray(arr, &t->build.uninstall, &t->build.nuninstall) < 0)
                                goto error;
                } else {
                        t->build.uninstall = NULL;
                        t->build.nuninstall = 0;
                }
        }

        toml_free(conf);
        return 0;

error:
        cleanup_pkg(t);
        toml_free(conf);
        return -1;
}

static int
execute(const char *cmd)
{
        int status;

        status = system(cmd);
        return (status == -1 || !WIFEXITED(status)) ?
                        -1 : WEXITSTATUS(status);
}

int
main(int argc, char *argv[])
{
        /* long name variants */
        static const struct option long_options[] = {
                {"help", no_argument, 0, 'h'},
                {"version", no_argument, 0, 'V'},
                {"box", no_argument, 0, 'B'},
                {"quiet", no_argument, 0, 'q'},
                {0, 0, 0, 0} /* null terminator */
        };
        int option_index = 0;

        for (int opt;
                (opt = getopt_long(argc, argv, "hVBq::",
                        long_options,
                        &option_index)) != -1;) {

                switch (opt) {
                        case 'h': /* help option */
                                printf("usage: %s [-hVqB] [package.toml]\n", argv[0]);
                                printf("Options:\n");
                                printf("    -h, --help      Display this help message\n");
                                printf("    -V, --version   Display version information\n");
                                printf("    -B, --box       Display a box\n");
                                printf("    -q, --quiet     Display less information about package "
                                        "downloading\n");
                                return 0;
                        case 'V': /* version */
                                printf("pms - Pack My Sh*t version: %s\n", VERSION);
                                return 0;
                        case 'B': /* box? */
                                printf(" _____\n|     |\n|     |\n|     | <== GO INSIDE\n\\-----/\n");
                                return 0;
                        case 'q': /* quiet flag */
                                quiet = 1;
                                break;
                        case '?': /* invalid option */
                                fprintf(stderr, "Unknown option: %s\n", argv[optind - 1]);
                                return 1;
                        default: /* unexpected cases */
                                abort();
                }
        }
        
        Toml toml = {0};

        if (optind >= argc)
                die("missing package.toml argument\n");

        if (parsetoml(argv[optind], &toml) < 0)
                die("failed to parse TOML file\n");

        if (getuid())
                die("must be run as root\n");

        /* TODO: Add build steps here */

        cleanup_pkg(&toml);
        return 0;
}
