#ifndef UTIL_H_
#define UTIL_H_

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <getopt.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// Structure to hold package information
typedef struct {
  char *pkgname;
  char *version;
  char **source;
  char **patches;
  char **build;
  char **depends;

  size_t source_count;
  size_t patch_count;
  size_t build_count;
  size_t depends_count;
} Package;

int parse_json_array(cJSON *root, Package *pkg, const char *field_name, char ***field_ptr, size_t *count_ptr);
int parse_pkgbuild(const char *filename, Package *pkg);

#endif // UTIL_H_
