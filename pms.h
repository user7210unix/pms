/* see LICENSE file for license details */

/* Package information structure */
typedef struct {
        char *name;
        char *version;
        char *author;
        char *type;
        char **src;
        size_t nsrc;            /* number of source files */
} Package;

/* Patch information structure */
typedef struct {
        char **patches;
        size_t npatches;        /* number of patches */
} Patches;

/* Build instructions structure */
typedef struct {
        char **compile;
        size_t ncompile;        /* number of compile commands */
        char **install;
        size_t ninstall;        /* number of install commands */
        char **uninstall;
        size_t nuninstall;      /* number of uninstall commands */
} Build;

/* Dependencies structure */
typedef struct {
        char **deps;
        size_t ndeps;   /* number of dependencies */
} Dependencies;

/* Main TOML configuration structure */
typedef struct {
        Package pkg;
        Patches patches;
        Build build;
        Dependencies deps;
} Toml;

/* Function prototypes */
int parsetoml(const char *filename, Toml *toml);
void freetoml(Toml *toml);

static int execute(const char *cmd);
