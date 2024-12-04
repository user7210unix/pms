#ifndef REPO_H
#define REPO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "config.h"

// Structure for memory handling
typedef struct {
    char *data;
    size_t size;
} Buffer;

// Main functions for repository interaction
char* fetch_package_build(const char *package_name, const char *repo_url);
char* fetch_package_info(const char *package_name, const char *repo_url);
int search_package(const char *package_name, const char *repo_url);
int search_all_repos(const char *package_name);

// URL formatting helpers
char* format_github_url(const char *repo_url, const char *package_name, const char *file_name);
char* get_repo_url(int repo_index);

// Repository management
int init_repos(void);
void cleanup_repos(void);

// Cleanup function
void repo_cleanup(char *data);

#endif // REPO_H
