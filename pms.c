#define VERSION "0.9.9-PREVIEW"

#include "config.h"

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Structure to hold package information
typedef struct {
  char *pkgname;
  char **source;
  char **patches;
  char **build;
  char **depends;

  size_t source_count;
  size_t patch_count;
  size_t build_count;
  size_t depends_count;
} Package;

// Free everything :P
void free_package(Package *pkg) {
  free(pkg->pkgname);
  for (size_t i = 0; i < pkg->source_count; i++)
    free(pkg->source[i]);
  for (size_t i = 0; i < pkg->patch_count; i++)
    free(pkg->source[i]);
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
  if (item && cJSON_IsArray(item)) {
    *count_ptr = cJSON_GetArraySize(item);
    *field_ptr = malloc(
        *count_ptr * sizeof(char *)); // saving cycles instead of zeroing memory
    if (*field_ptr) {
      for (size_t i = 0; i < *count_ptr; i++) {
        cJSON *array_item = cJSON_GetArrayItem(item, i);
        if (cJSON_IsString(array_item)) {
          (*field_ptr)[i] = strdup(array_item->valuestring);
        } else {
          fprintf(stderr, "Error: Non-string value found in array\n");
          return 1;
        }
      }
    }
  } else {
    *count_ptr = 0;
    *field_ptr = NULL;
  }
  return 0;
}

// Function to parse the JSON PKGBUILD
int parse_pkgbuild(const char *filename, Package *pkg) {
  FILE *fp = fopen(filename, "rb"); // Read in binary mode for portability
  if (!fp) {
    perror("fopen");
    return 1;
  }

  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  rewind(fp); // Use rewind instead of fseek(fp, 0, SEEK_SET)

  char *buffer = malloc(file_size + 1);
  if (!buffer) {
    perror("malloc");
    fclose(fp);
    return 1;
  }

  if (fread(buffer, 1, file_size, fp) != file_size) {
    perror("fread");
    fclose(fp);
    free(buffer);
    return 1;
  }
  fclose(fp);
  buffer[file_size] = '\0';

  cJSON *root = cJSON_Parse(buffer);
  free(buffer); // Free the file buffer
  if (!root) {
    fprintf(stderr, "cJSON_Parse: %s\n", cJSON_GetErrorPtr());
    return 1;
  }

  // Extract sources, array handling
  cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "pkgname");
  pkg->pkgname =
      item && cJSON_IsString(item) ? strdup(item->valuestring) : NULL;

  parse_json_array(root, pkg, "source", &pkg->source, &pkg->source_count);
  parse_json_array(root, pkg, "patches", &pkg->patches, &pkg->patch_count);
  parse_json_array(root, pkg, "build", &pkg->build, &pkg->build_count);
  parse_json_array(root, pkg, "depends", &pkg->depends, &pkg->depends_count);

  cJSON_Delete(root); // Free cJSON data structure
  return 0;
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

  int ret = 0;
  for (size_t i = 0; i < pkg->build_count && !ret; i++) {
    if (quiet == 0) {
      printf("Executing: %s\n", pkg->build[i]);
    }

    if (!strncmp(pkg->build[i], "cd ", 3)) {
      ret = chdir(pkg->build[i] + 3) ? (perror("chdir"), 1) : 0;
      continue;
    }
    ret = system(pkg->build[i]) ? 1 : 0;
  }

  return chdir(cwd) ? (perror("chdir"), 1) : ret;
}

// Renamed fetch_tarball to pull_files, mostly because we are using this for the
// patches too
int pull_files(const char *url, const char *filename, int quiet) {
  if (quiet == 0) {
    printf("Fetching Sources...\n");
  };
  CURL *curl = curl_easy_init();

  if (!curl) {
    return 1;
  }

  FILE *source = fopen(filename, "w");
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
  if (stat(download_dir, &st) == -1) {
    if (mkdir(download_dir, 0755) ==
        -1) { // Read+execute access for normal users
      fprintf(stderr, "Error creating download directory %s\n", download_dir);
      return 1;
    }
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

    pull_files(pkg->patches[i], full_path, quiet);

    if (quiet == 0) {
      printf("%s Completed!", filename);
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

    struct {
      const char *ext;
      const char *fmt;
    } cmds[] = {{".tar.gz", "tar xzf"},  {".tgz", "tar xzf"},
                {".tar.xz", "tar xJf"},  {".txz", "tar xJf"},
                {".tar.bz2", "tar xjf"}, {".zip", "unzip"}};

    const char *fmt = NULL;
    for (int i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
      if (strstr(filename, cmds[i].ext)) {
        fmt = cmds[i].fmt;
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

    snprintf(cmd, sizeof(cmd), "cd %s/%s* && patch -Np1 -i ../%s", download_dir, pkg->pkgname,
             filename);
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

  int quiet = 0;

  for (int opt; (opt = getopt_long(argc, argv, "h::", long_options,
                                   &option_index)) != -1;) {

    switch (opt) {
    case 'h': // Help option
      printf("Usage: %s [options] <pkgbuild.json>\n", argv[0]);
      printf("Options:\n");
      printf("    -h, --help      Display this help message\n");
      printf("    -v, --version   Display version information\n");
      printf("    -q, --quiet,    Display less information about package "
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
      continue;
    case '?': // Invalid option
      fprintf(stderr, "Unknown option: %s\n", argv[optind - 1]);
      return 1;
    default: // Unexpected cases
      abort();
    }
  }

  if (optind >= argc) { // Check if a pkgbuild.json file was provided
    fprintf(stderr, "Missing pkgbuild.json argument\n");
    return 1;
  }

  Package pkg = {0};
  parse_pkgbuild(argv[optind], &pkg);
  if (parse_pkgbuild(argv[optind], &pkg) != 0) {
    fprintf(stderr, "Error parsing pkgbuild.json\n");
    return 1;
  }

  check_root();

  fetch_sources(&pkg, quiet);
  extract_sources(&pkg, quiet);
  fetch_patches(&pkg, quiet);
  apply_patches(&pkg, quiet);

  // Implement dependency resolution here later, way too lazy for that for now

  int ret = execute_build(&pkg, quiet);
  free_package(&pkg);
  return ret;
}
