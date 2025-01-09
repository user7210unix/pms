#ifndef REPO_H_
#define REPO_H_

#include "package.h"
#include <stddef.h>
#include <unistd.h>

// Structure to represent a single repository.
typedef struct {
  const char *name; // Repository name
  const char *url;  // Repository URL
} Repository;

// Function declarations
int search_all_repos(const char *package_name, const char *version,
                     Package *pkg);

#endif // REPO_H_
