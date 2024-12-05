#include "repo.h"
#include "config.h"

#include <curl/curl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Repository *repositories = NULL; // Array of repositories
static size_t repo_count = 0;

int init_repositories(void) {
  // Check if repo_urls has any elements
  if (repo_urls[0] == NULL) {
    fprintf(stderr, "Repository URLs are not defined in config.h.\n");
    return 1; // Error: Configuration issue
  }

  // Count the number of repositories
  size_t count = 0;
  while (repo_urls[count] != NULL) {
    count++;
  }

  if (count == 0) {
    fprintf(stderr, "No repositories configured in config.h.\n");
    return 1; // Error: No repositories defined
  }

  // Allocate memory for repositories
  Repository *repos = malloc(count * sizeof(Repository));
  if (!repos) {
    fprintf(stderr, "Memory allocation failed for repositories.\n");
    return 1; // Error: Allocation failure
  }

  // Populate repository data
  for (size_t i = 0; i < count; i++) {
    repos[i].url = repo_urls[i];
  }

  // Assign global state only after successful initialization
  repositories = repos;
  repo_count = count;

  fprintf(stdout, "Initialized %zu repositories successfully.\n", repo_count);
  return 0; // Success
}

// Free resources used by repositories
void cleanup_repositories(void) {
  if (repositories) {
    free(repositories);
    repositories = NULL;
  }
  repo_count = 0;
}

// Compare versions (used in finding the latest version of the package)
int compare_version(const char *v1, const char *v2) { return strcmp(v1, v2); }

// Clone repository into local repo directory
int clone_repository(const char *repo_url, const char *repo_dir) {
  struct stat st = {0};
  const char *repo_name;
  char *cmd = NULL;
  size_t cmd_len;

  // Extract repo name from URL (after last /)
  repo_name = strrchr(repo_url, '/');
  if (!repo_name || strlen(repo_name) <= 1) {
    fprintf(stderr, "Invalid repository URL format: %s\n", repo_url);
    return 1;
  }
  repo_name++; // Skip the '/'

  // Remove .git extension if present
  size_t name_len = strlen(repo_name);
  char *clean_repo_name = malloc(name_len + 1);
  if (!clean_repo_name) {
    fprintf(stderr, "Failed to allocate memory for repo name\n");
    return 1;
  }
  memcpy(clean_repo_name, repo_name, name_len + 1);
  char *git_ext = strstr(clean_repo_name, ".git");
  if (git_ext) {
    *git_ext = '\0';
    name_len = strlen(clean_repo_name);
  }

  // Construct full repo path
  size_t dir_len = strlen(repo_dir);
  size_t path_len = dir_len + name_len + 2; // +2 for / and \0
  char *repo_path = malloc(path_len);
  if (!repo_path) {
    fprintf(stderr, "Failed to allocate memory for repo path\n");
    free(clean_repo_name);
    return 1;
  }

  snprintf(repo_path, path_len, "%s/%s", repo_dir, clean_repo_name);
  free(clean_repo_name);

  // Check if directory already exists using stat
  if (stat(repo_path, &st) == 0) {
    free(repo_path);
    return 0;
  }

  // Create directory and clone repo
  cmd_len = strlen(repo_url) + strlen(repo_path) + 15; // "git clone " + urls
  cmd = malloc(cmd_len);
  if (!cmd) {
    fprintf(stderr, "Failed to allocate memory for command\n");
    free(repo_path);
    return 1;
  }

  snprintf(cmd, cmd_len, "git clone %s %s", repo_url, repo_path);
  free(repo_path);

  // Execute git command
  int result = system(cmd);
  free(cmd);

  if (result != 0) {
    fprintf(stderr, "Failed to clone/update repository: %s\n", repo_url);
    return 1;
  }

  return 0;
}

// This is just to compare the version of packages in that repo category
int find_latest_version_file(const char *package_name,
                             char *latest_version_file, size_t max_len,
                             char *pkg_dir) {
  // Open the current directory
  DIR *dir = opendir(pkg_dir);
  if (!dir) {
    fprintf(stderr, "Failed to open repo directory\n");
    return 1; // Error: unable to open directory
  }

  struct dirent *entry;
  char *latest_version = NULL; // Store the latest version as a string
  char *filename = NULL;       // To store full filename

  while ((entry = readdir(dir)) != NULL) {
    // Check if the file name matches the pattern {package_name}-*.json
    if (strncmp(entry->d_name, package_name, strlen(package_name)) == 0 &&
        strstr(entry->d_name, ".json") != NULL) {
      // Extract the version part from the filename
      // {package_name}-{version}.json
      const char *version_start =
          entry->d_name + strlen(package_name) + 1; // Skip {package_name}-
      const char *version_end = strstr(version_start, ".json");
      if (version_end != NULL) {
        size_t version_length = version_end - version_start;
        char *current_version = malloc(version_length + 1);
        strncpy(current_version, version_start, version_length);
        current_version[version_length] = '\0';

        // Compare current version with the latest one
        if (strlen(latest_version) == 0 ||
            compare_version(current_version, latest_version) > 0) {
          // Update the latest version and filename
          strncpy(latest_version, current_version, sizeof(latest_version) - 1);
          snprintf(filename, sizeof(filename), "%s", entry->d_name);
        }
      }
    }
  }

  closedir(dir);

  // If we found a latest version file, copy it to latest_version_file
  if (strlen(latest_version) > 0) {
    strncpy(latest_version_file, filename, max_len - 1);
    latest_version_file[max_len - 1] = '\0';
    return 0; // Success
  }

  fprintf(stderr,
          "No versioned files found for package '%s' in current directory.\n",
          package_name);
  return 1; // Error: No versioned files found
}

// Search for a package across all repositories
int search_all_repos(const char *package_name, const char *version,
                     Package *pkg) {

  // Plan: if version == NULL, we find the latest version, if version is given
  // (i.e. pkgname::version), we find the version and parse it as normal

  return 0; // To be implemented in a search alg
}
