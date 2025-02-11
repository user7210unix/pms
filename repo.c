#include "repo.h"

#include "config.h"
#include <curl/curl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Repository *repositories = NULL; // Array of repositories
static size_t repo_count = 0;

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
  size_t git_suffix_len = strlen(".git");
  char *clean_repo_name;

  if (name_len > git_suffix_len &&
      strncmp(repo_name + name_len - git_suffix_len, ".git", git_suffix_len) ==
          0) {
    clean_repo_name = malloc(name_len - git_suffix_len + 1);
    if (!clean_repo_name) {
      fprintf(stderr, "Failed to allocate memory for repo name\n");
      return 1;
    }
    strncpy(clean_repo_name, repo_name, name_len - git_suffix_len);
    clean_repo_name[name_len - git_suffix_len] = '\0';
    name_len = name_len - git_suffix_len;
  } else {
    clean_repo_name = strdup(repo_name);
    if (!clean_repo_name) {
      fprintf(stderr, "Failed to allocate memory for repo name\n");
      return 1;
    }
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
  for (size_t i = 0; i < repo_count; i++) {
    Repository *repo = &repositories[i];

    // Get the repo base directory
    size_t repo_base_len = strlen(repo_dir) + strlen(repo->name) + 2;
    char *repo_base = malloc(repo_base_len);
    if (!repo_base) {
      fprintf(stderr, "Memory allocation failed: repo_base\n");
      return 1;
    }
    snprintf(repo_base, repo_base_len, "%s/%s", repo_dir, repo->name);

    // Open the repo directory to list categories
    DIR *repo_dir_handle = opendir(repo_base);
    if (!repo_dir_handle) {
      fprintf(stderr, "Failed to open repo directory: %s\n", repo_base);
      free(repo_base);
      return 1;
    }

    struct dirent *category_entry;
    while ((category_entry = readdir(repo_dir_handle)) != NULL) {
      if (category_entry->d_type != DT_DIR ||
          strcmp(category_entry->d_name, ".") == 0 ||
          strcmp(category_entry->d_name, "..") == 0) {
        continue;
      }

      // Construct path to package within category
      size_t pkg_dir_len = repo_base_len + strlen(category_entry->d_name) +
                           strlen(package_name) + 3;
      char *pkg_dir = malloc(pkg_dir_len);
      if (!pkg_dir) {
        fprintf(stderr, "Memory allocation failed: pkg_dir\n");
        free(repo_base);
        closedir(repo_dir_handle);
        return 1;
      }
      snprintf(pkg_dir, pkg_dir_len, "%s/%s/%s", repo_base,
               category_entry->d_name, package_name);

      char latest_version_file[FILENAME_MAX];
      if (!version) {
        if (!find_latest_version_file(package_name, latest_version_file,
                                      sizeof(latest_version_file), pkg_dir)) {

          size_t filepath_len =
              strlen(pkg_dir) + strlen(latest_version_file) + 2;
          char *filepath = malloc(filepath_len);
          if (!filepath) {
            fprintf(stderr, "Memory allocation failed: filepath\n");
            free(pkg_dir);
            free(repo_base);
            closedir(repo_dir_handle);
            return 1;
          }

          snprintf(filepath, filepath_len, "%s/%s", pkg_dir,
                   latest_version_file);

          int result = parse_pkgbuild(filepath, pkg);
          if (result == 0) {
            free(filepath);
            free(pkg_dir);
            free(repo_base);
            closedir(repo_dir_handle);
            return 0; // Successfully found and parsed
          }
          free(filepath);
        }
      } else {
        size_t filename_len = strlen(package_name) + strlen(version) + 7;
        char *filename = malloc(filename_len);
        if (!filename) {
          fprintf(stderr, "Memory allocation failed: filename\n");
          free(pkg_dir);
          free(repo_base);
          closedir(repo_dir_handle);
          return 1;
        }

        snprintf(filename, filename_len, "%s-%s.json", package_name, version);

        size_t filepath_len = strlen(pkg_dir) + strlen(filename) + 2;
        char *filepath = malloc(filepath_len);
        if (!filepath) {
          fprintf(stderr, "Memory allocation failed: filepath\n");
          free(pkg_dir);
          free(filename);
          free(repo_base);
          closedir(repo_dir_handle);
          return 1;
        }

        snprintf(filepath, filepath_len, "%s/%s", pkg_dir, filename);

        int result = parse_pkgbuild(filepath, pkg);
        if (result == 0) {
          free(filepath);
          free(filename);
          free(pkg_dir);
          free(repo_base);
          closedir(repo_dir_handle);
          return 0; // Successfully found and parsed
        }

        free(filepath);
        free(filename);
      }
      free(pkg_dir);
    }

    free(repo_base);
    closedir(repo_dir_handle);
  }

  return 1; // Package not found
}
