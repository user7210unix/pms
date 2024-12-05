#ifndef REPO_H
#define REPO_H

#include <stddef.h>
#include <unistd.h>
#include "util.h"

// Structure to represent a single repository.
typedef struct {
  const char *url; // Repository URL
} Repository;

// Function declarations
int init_repositories(void);     // Initialize repositories from config.h
void cleanup_repositories(void); // Free resources used by repositories
int search_all_repos(const char *package_name, const char *version, Package *pkg); // Search for a package across all repos

#endif // REPO_H
