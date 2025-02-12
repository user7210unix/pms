/* See LICENSE file for copyright and license details. */

/* build directories */
static const char *builddir = "/var/pms/build";    /* build directory */
static const char *logdir = "/var/pms/logs";       /* log directory */
static const char *cachedir = "/var/cache/pms";    /* cache directory */

/* build options */
static const int clean = 0;      /* 0: keep all, 1: clean extracted, 2: clean all */
static const int force = 0;      /* 0: skip existing, 1: force redownload */
static int quiet = 0;            /* 0: verbose output, 1: minimal output */
static int ask = 1;              /* 0: no confirmation, 1: confirm actions */
enum {
        CLEAN_NONE,     /* keep all */
        CLEAN_EXTRACT,  /* clean extracted */
        CLEAN_ALL       /* clean all */
};

/* repository configuration */
static const int userepo = 0;    /* 0: local only, 1: enable repositories */
static const char *repodir = "/var/pms/repos";     /* repository directory */

/* official repositories - NULL terminated list */
static const char *repos[] = {
        "https://github.com/LearnixOS/lxos-repo",
        NULL
};

/* download options */
static const int maxretries = 3;        /* max download retries */
static const int timeout = 30;          /* connection timeout in seconds */

/* paths */
static const char *shell = "/bin/sh";   /* shell for executing commands */
static const char *patch = "patch";     /* patch command */
static const char *git = "git";         /* git command */
