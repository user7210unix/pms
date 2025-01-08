#include "config.h"
#include "package.h"

#if REPO_SUPPORT
#include "repo.h"
#endif

// Free everything :P
void free_package(Package *pkg) {
  free(pkg->pkgname);
  free(pkg->version);
  for (size_t i = 0; i < pkg->source_count; i++)
    free(pkg->source[i]);
  for (size_t i = 0; i < pkg->patch_count; i++)
    free(pkg->patches[i]);
  for (size_t i = 0; i < pkg->build_count; i++)
    free(pkg->build[i]);
  for (size_t i = 0; i < pkg->depends_count; i++)
    free(pkg->depends[i]);
  free(pkg->source);
  free(pkg->patches);
  free(pkg->build);
  free(pkg->depends);
}

// Extract build scripts, array handling
int parse_json_array(cJSON *root, Package *pkg, const char *field_name,
                     char ***field_ptr, size_t *count_ptr) {
  cJSON *item = cJSON_GetObjectItemCaseSensitive(root, field_name);
  if (!item || !cJSON_IsArray(item)) {
    *count_ptr = 0;
    *field_ptr = NULL;
    return 0;
  }
  *count_ptr = cJSON_GetArraySize(item);
  if (*count_ptr == 0) {
    *field_ptr = NULL;
    return 0;
  }
  *field_ptr = malloc(*count_ptr * sizeof(char *));
  if (*field_ptr == NULL) {
    return 1;
  }
  for (size_t i = 0; i < *count_ptr; i++) {
    cJSON *array_item = cJSON_GetArrayItem(item, i);
    if (!cJSON_IsString(array_item)) {
      fprintf(stderr, "Error: Non-string value found in array\n");
      for (size_t j = 0; j < i; j++) {
        free((*field_ptr)[j]);
      }
      free(*field_ptr);
      *field_ptr = NULL;
      return 1;
    }
    (*field_ptr)[i] = strdup(array_item->valuestring);
    if ((*field_ptr)[i] == NULL) {
      for (size_t j = 0; j < i; j++) {
        free((*field_ptr)[j]);
      }
      free(*field_ptr);
      *field_ptr = NULL;
      return 1;
    }
  }

  return 0;
}

// Function to parse the JSON PKGBUILD
int parse_pkgbuild(const char *filename, Package *pkg) {
  int ret = 1;

  FILE *fp = fopen(filename, "rb"); // Read in binary mode for portability
  if (!fp) {
    perror("fopen");
    goto exit1;
  }

  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  rewind(fp); // Use rewind instead of fseek(fp, 0, SEEK_SET)

  char *buffer = malloc(file_size + 1);
  if (!buffer) {
    perror("malloc");
    goto exit2;
  }

  if (fread(buffer, 1, file_size, fp) != file_size) {
    perror("fread");
    goto exit3;
  }
  buffer[file_size] = '\0';

  cJSON *root = cJSON_Parse(buffer);
  if (!root) {
    fprintf(stderr, "cJSON_Parse Error Parsing: %s\n", cJSON_GetErrorPtr());
    goto exit3;
  }

  // Extract sources, array handling
  cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "pkgname");
  pkg->pkgname = cJSON_IsString(item) ? strdup(item->valuestring) : NULL;
  // Extract version
  item = cJSON_GetObjectItemCaseSensitive(root, "version");
  pkg->version = cJSON_IsString(item) ? strdup(item->valuestring) : NULL;

  // ...
  ret = 0;
  cJSON_Delete(root);
exit3:
  free(buffer);
exit2:
  fclose(fp);
exit1:
  return ret;
}

// Function to execute build commands : UPDATE - it now changes directory to the
// download directory extra note: It can also now finally change directories to
// the extracted tarball if you specify a cd
//             However I recommend from this really dumb way of doing it, you
//             better use .. and . a lot... :(
int execute_build(const Package *pkg, int quiet) {
  char cwd[PATH_MAX];
  if (!getcwd(cwd, sizeof(cwd)))
    return (perror("getcwd"), 1);

  if (chdir(download_dir))
    return (perror("chdir"), 1);

  if (quiet == 1) {
    struct stat st = {0}; // yes this is copied from download_dir
    int ret = stat(log_dir, &st);
    if (ret == -1 && mkdir(log_dir, 0755) == -1) {
      fprintf(stderr, "Error creating log directory %s\n", log_dir);
      return 1;
    }
  }

  int ret = 0;
  for (size_t i = 0; i < pkg->build_count && !ret; i++) {
    if (!quiet) {
      printf("Executing: %s\n", pkg->build[i]);
    }

    if (!strncmp(pkg->build[i], "cd ", 3)) {
      ret = chdir(pkg->build[i] + 3) ? (perror("chdir"), 1) : 0;
      continue;
    }

    // guhh it's so messy for now, but the thing now has a funny ... loading
    // thing while it's executing stuff. I'll optimize it later, maybe ewheeler
    // could help me?
    pid_t pid;
    int status;
    char cmd[PATH_MAX * 2];

    if (quiet) {
      int dots = 0;
      snprintf(cmd, sizeof(cmd), "%s >> %s/%s-%s-build.log-%zu 2>&1",
               pkg->build[i], log_dir, pkg->pkgname, pkg->version, i);

      if ((pid = fork()) == 0) {
        _exit(system(cmd) ? 1 : 0);
      }

      while ((ret = waitpid(pid, &status, WNOHANG)) == 0) {
        printf("\rExecuting%.*s   \r", dots + 1, "...");
        fflush(stdout);
        dots = (dots + 1) % 3;
        usleep(500000);
      }

      if (ret == -1) {
        perror("waitpid");
        break;
      }

      printf("\r                \r");

    } else {
      snprintf(cmd, sizeof(cmd), "%s", pkg->build[i]);

      if ((pid = fork()) == 0) {
        _exit(system(cmd) ? 1 : 0);
      }

      if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid"); // Add error checking for waitpid
        break;
      }
    }

    ret = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  }

  printf("Done!\n");
  return chdir(cwd) ? (perror("chdir"), 1) : ret;
}

int pull_files(const char *url, const char *filename, int quiet) {
  if (quiet == 0) {
    printf("Fetching Sources...\n");
  };
  CURL *curl = curl_easy_init();

  if (!curl) {
    return 1;
  }

  FILE *source = fopen(filename, "w");
  if (!source) {
    perror("fopen");
    curl_easy_cleanup(curl);
    return 1;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, source);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  fclose(source);
  if (quiet == 0) {
    printf(" Done!\n");
  }
  return res != CURLE_OK;
}

int fetch_sources(const Package *pkg, int quiet) {
  if (quiet == 1) {
    printf("Fetching sources... ");
  }
  if (!pkg || !pkg->source || pkg->source_count == 0) {
    return 0;
  }

  // Check if download_dir exists and create if needed | Segment fault if it
  // doesn't exist :sob:
  struct stat st = {0};
  int ret = stat(download_dir, &st); // less indents
  if (ret == -1 && mkdir(download_dir, 0755) == -1) {
    fprintf(stderr, "Error creating download directory %s\n", download_dir);
    return 1;
  }

  const char *filename;
  char full_path[PATH_MAX];
  for (size_t i = 0; i < pkg->source_count; i++) {
    if (!pkg->source[i])
      continue;

    filename = strrchr(pkg->source[i], '/');
    filename = filename ? filename + 1 : pkg->source[i];

    snprintf(full_path, sizeof(full_path), "%s/%s", download_dir, filename);

    if (access(full_path, F_OK) == 0) {
      printf("Skipping download: %s already downloaded!\n", filename);
      continue;
    }

    if (quiet == 0) {
      printf("Download %s to %s\n", pkg->source[i], full_path);
    }
    if (pull_files(pkg->source[i], full_path, quiet)) {
      fprintf(stderr, "Error downloading %s\n", pkg->source[i]);
      return 1;
    }

    if (quiet == 0) {
      printf("%s Completed!\n", filename);
    }
  }

  return 0;
}

int fetch_patches(const Package *pkg, int quiet) {
  if (pkg->patch_count == 0) { // if there are no patches, don't pull anything
    return 0;
  }

  if (quiet == 1) {
    printf("Fetching patches... ");
  }
  const char *filename;
  char full_path[PATH_MAX];
  for (size_t i = 0; i < pkg->patch_count; i++) {
    filename = strrchr(pkg->patches[i], '/');
    filename = filename ? filename + 1 : pkg->patches[i];

    snprintf(full_path, sizeof(full_path), "%s/%s", download_dir, filename);

    if (access(full_path, F_OK) == 0) {
      printf("Skipping download: %s already downloaded!\n", filename);
      continue;
    }

    if (quiet == 0) {
      printf("Download %s to %s\n", pkg->patches[i], full_path);
    }
    if (pull_files(pkg->patches[i], full_path, quiet)) {
      fprintf(stderr, "Error downloading %s\n", pkg->patches[i]);
      return 1;
    }

    if (quiet == 0) {
      printf("%s Completed!\n", filename);
    }
  }
  if (quiet == 1) {
    printf(" Done!\n");
  }
  return 0;
}

int extract_sources(const Package *pkg, int quiet) {
  if (quiet == 1) {
    printf("Extracting sources...\n");
  }

  const char *filename;
  char full_path[PATH_MAX];
  char cmd[PATH_MAX * 2];

  for (size_t i = 0; i < pkg->source_count; i++) { // First untar/unzip sources
    filename = strrchr(pkg->source[i], '/');
    filename = filename ? filename + 1 : pkg->source[i];

    snprintf(full_path, sizeof(full_path), "%s/%s", download_dir, filename);

    static const struct {
      const char *ext;
      const char *fmt;
    } cmds[] = {{".tar.gz", "tar xzf"},  {".tgz", "tar xzf"},
                {".tar.xz", "tar xJf"},  {".txz", "tar xJf"},
                {".tar.bz2", "tar xjf"}, {".zip", "unzip"}};

    const char *fmt = NULL;
    for (size_t j = 0; j < sizeof(cmds) / sizeof(cmds[0]);
         j++) { // Changed i to j to avoid shadowing
      if (strstr(filename, cmds[j].ext)) {
        fmt = cmds[j].fmt;
        break;
      }
    }

    if (!fmt) {
      if (quiet == 0) {
        printf("Skipping extraction for unsupported format: %s\n", filename);
      }
      continue;
    }

    snprintf(cmd, sizeof(cmd), "cd %s && %s %s", download_dir, fmt, filename);

    if (quiet == 0) {
      printf("Extracting %s\n", filename);
    }

    if (system(cmd) != 0) {
      fprintf(stderr, "Failed to extract %s\n", filename);
      return 1;
    }
  }
  return 0;
}

// Function to find (extracted) source directory (using glob, I wasn't sure on
// how else to do this)
char *find_source_directory(const Package *pkg, char *source_dir) {
  glob_t glob_result;

  // Try with version first
  snprintf(source_dir, PATH_MAX, "%s/%s-%s*", download_dir, pkg->pkgname,
           pkg->version);

  if (!glob(source_dir, GLOB_ERR, NULL, &glob_result) &&
      glob_result.gl_pathc > 0) {
    snprintf(source_dir, PATH_MAX, "%s",
             strrchr(glob_result.gl_pathv[0], '/') + 1);
  } else {
    // Fallback if no exact version match
    snprintf(source_dir, PATH_MAX, "%s/%s*", download_dir, pkg->pkgname);
    if (!glob(source_dir, GLOB_ERR, NULL, &glob_result) &&
        glob_result.gl_pathc > 0) {
      snprintf(source_dir, PATH_MAX, "%s",
               strrchr(glob_result.gl_pathv[0], '/') + 1);
    }
  }
  globfree(&glob_result);
  return source_dir;
}

int apply_patches(const Package *pkg, int quiet) {
  if (pkg->patch_count == 0) { // There is no need to do any of this if there
                               // isn't any patches in the first place
    return 0;
  }

  const char *filename;
  char cmd[PATH_MAX * 2];

  for (size_t i = 0; i < pkg->patch_count; i++) {
    filename = strrchr(pkg->patches[i], '/');
    filename = filename ? filename + 1 : pkg->patches[i];

    if (quiet == 0) {
      printf("Applying patch... %s\n", filename);
    }

    char source_dir[PATH_MAX];
    find_source_directory(pkg, source_dir);

    snprintf(cmd, sizeof(cmd), "cd %s/%s && patch -Np1 -i ../%s", download_dir,
             source_dir, filename);
    if (system(cmd)) {
      fprintf(stderr, "Failed to apply patch %s\n", filename);
      return 1;
    }
  }

  if (quiet == 1) {
    printf("Patching...Done!\n");
  }

  return 0;
}

// Clean up the sources and stuff depending on the
// level of clean you want
int cleanup_crap(Package pkg, int level) {
  if (level < 0 || level > 2) {
    fprintf(stderr, "Error: Invalid clean level specified (must be 0-2)\n");
    return 1;
  }

  if (level == 0) {
    return 0;
  }

  char cmd[PATH_MAX];
  if (level == 1) {
    char source_dir[PATH_MAX];
    find_source_directory(&pkg, source_dir);
    snprintf(cmd, sizeof(cmd), "rm -rf %s/%s", download_dir, source_dir);
    return system(cmd);
  }

  if (level == 2) {
    snprintf(cmd, sizeof(cmd), "rm -rf %s/*", download_dir);
    return system(cmd);
  }

  return 1;
}

// Check for root privileges
int check_root(void) {
  if (getuid()) {
    fputs("PMS requires root privileges. Run with sudo/doas.\n", stderr);
    exit(1);
  }
  return 0;
}

int main(int argc, char *argv[]) {
  // Long name variants of commands | Moving it down here for ez of access
  static const struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"version", no_argument, 0, 'V'},
      {"box", no_argument, 0, 'B'},
      {"quiet", no_argument, 0, 'q'},
      {0, 0, 0, 0} // Null terminator for the long_options array
  };
  int option_index = 0;

  for (int opt;
       (opt = getopt_long(argc, argv, "hVBq::",
                          long_options, // Added V, B, q to getopt string
                          &option_index)) != -1;) {

    switch (opt) {
    case 'h': // Help option
      printf("Usage: %s [options] <pkgbuild.json>\n", argv[0]);
      printf("Options:\n");
      printf("    -h, --help      Display this help message\n");
      printf("    -V, --version   Display version information\n");
      printf(
          "    -B, --box       Display a box\n"); // Added documentation for -B
      printf("    -q, --quiet     Display less information about package "
             "downloading\n");
      return 0;
    case 'V': // Version option
      printf("pms - Pack My Sh*t version: %s\n", VERSION);
      return 0;
    case 'B':
      printf(" _____\n|     |\n|     |\n|     | <== GO INSIDE\n\\-----/\n");
      return 0;
    case 'q':
      quiet = 1;
      break;
    case '?': // Invalid option
      fprintf(stderr, "Unknown option: %s\n", argv[optind - 1]);
      return 1;
    default: // Unexpected cases
      abort();
    }
  }

  Package pkg = {0};

#if !REPO_SUPPORT
  if (optind >= argc) { // Check if a pkgbuild.json file was provided
    fprintf(stderr, "Missing pkgbuild.json argument\n");
    return 1;
  }

  if (parse_pkgbuild(argv[optind], &pkg) != 0) {
    fprintf(stderr, "Error parsing pkgbuild.json\n");
    return 1;
  }
#endif

#if REPO_SUPPORT
  // Check if repo urls has been configured - exit if none are set (just in
  // case)
  if (!repo_urls[0]) {
    fprintf(stderr, "Error: repo_urls is not set\n");
    return 1;
  }

  // Handle the case where user passed a package name with version
  char *pkg_name = argv[optind];
  char *pkg_version = NULL;

  // Check if the package name contains a version (pkgname::version format)
  char *version_delim = strstr(pkg_name, "::");
  if (version_delim) {
    *version_delim = '\0';           // Terminate the pkgname string
    pkg_version = version_delim + 2; // Version part starts after "::"
  }

  // Search for the package in the repositories
  if (pkg_version) {
    // User provided a version, search for that specific version
    if (search_all_repos(pkg_name, pkg_version, &pkg) != 0) {
      fprintf(stderr, "Package '%s' version '%s' not found in repositories.\n",
              pkg_name, pkg_version);
      return 1;
    }
  } else {
    // No version specified, search for the latest version
    if (search_all_repos(pkg_name, NULL, &pkg) != 0) {
      fprintf(stderr, "Package '%s' not found in any repository.\n", pkg_name);
      return 1;
    }
  }
#endif

  check_root();

  fetch_sources(&pkg, quiet);
  extract_sources(&pkg, quiet);
  fetch_patches(&pkg, quiet);
  apply_patches(&pkg, quiet);

  // Implement dependency resolution here later, way too lazy for that for now

  int ret = execute_build(&pkg, quiet);
  cleanup_crap(pkg, clean_sources);
  free_package(&pkg);
  return ret;
}
