#ifndef REPO_H_
#define REPO_H_

#include "config.h"
#include "package.h"
#include <stddef.h>
#include <unistd.h>

// Structure to represent a single repository.
typedef struct {
  const char *name; // Repository name
  const char *url; // Repository URL
  char *repo_dir;
  const char *category;
} Repository;

// Function declarations
int search_all_repos(const char *package_name, const char *version,
                     Package *pkg);

#endif // REPO_H_
