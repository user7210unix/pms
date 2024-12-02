#define VERSION "0.0.1-beta"

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <sys/stat.h>

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
    for (size_t i = 0; i < pkg->source_count; i++) free(pkg->source[i]);
    for (size_t i = 0; i < pkg->patch_count; i++) free(pkg->source[i]);
    for (size_t i = 0; i < pkg->build_count; i++) free(pkg->build[i]);
    for (size_t i = 0; i < pkg->depends_count; i++) free(pkg->depends[i]);
    free(pkg->source);
    free(pkg->patches);
    free(pkg->build);
    free(pkg->depends);
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
    pkg->pkgname = item && cJSON_IsString(item) ? strdup(item->valuestring) : NULL;

    // Extract build scripts, array handling - Rewrite, trying something new and seeing if it works
    #define PARSE_ARRAY(field, count) \
        item = cJSON_GetObjectItemCaseSensitive(root, #field); \
        if (item && cJSON_IsArray(item)) { \
            pkg->count = cJSON_GetArraySize(item); \
            pkg->field = calloc(pkg->count, sizeof(char *)); \
            if (pkg->field) { \
                for (size_t i = 0; i < pkg->count; i++) { \
                    cJSON *array_item = cJSON_GetArrayItem(item, i); \
                    if (cJSON_IsString(array_item)) { \
                        pkg->field[i] = strdup(array_item->valuestring); \
                    } \
                } \
            } \
        } else { \
            pkg->count = 0; \
            pkg->field = NULL; \
        } // I basically added it so that if the field is empty, it doesn't throw a tantrum

    PARSE_ARRAY(source, source_count);
    PARSE_ARRAY(patches, patch_count);
    PARSE_ARRAY(build, build_count);
    PARSE_ARRAY(depends, depends_count);

    cJSON_Delete(root); // Free cJSON data structure
    return 0;
}


// Function to execute build commands : UPDATE - it now changes directory to the download directory
// extra note: It can also now finally change directories to the extracted tarball if you specify a cd
//             However I recommend from this really dumb way of doing it, you better use .. and . a lot... :(
int execute_build(const Package *pkg, int quiet) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd)))
        return (perror("getcwd"), 1);

    if (chdir(download_dir))
        return (perror("chdir"), 1);

    size_t i;
    int ret = 0;
    for (i = 0; i < pkg->build_count && !ret; i++) {
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

// Renamed fetch_tarball to pull_files, mostly because we are using this for the patches too
int pull_files(const char *url, const char *filename, int quiet) {
    if(quiet == 0){printf("Fetching Sources...\n");};
    CURL *curl = curl_easy_init();
    CURLcode res = CURLE_FAILED_INIT; // Initialize to an error value

    if(!curl){
        return 1; // Or return res directly since it's already an error
    }

    if(curl) {
        FILE *source = fopen(filename, "w");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, source);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(source);
    }
    if(quiet==0){printf(" Done!\n");}
    return res != CURLE_OK;
}

int fetch_sources(const Package *pkg, int quiet) {
    if (quiet == 1) {printf("Fetching sources... ");}
    if (!pkg || !pkg->source || pkg->source_count == 0) {
        return 0; // Return early if no sources
    }

    // Check if download_dir exists and create if needed | Segment fault if it doesn't exist :sob:
    struct stat st = {0};
    if (stat(download_dir, &st) == -1) {
        if (mkdir(download_dir, 0744) == -1) { // Read-only access for normal users
            fprintf(stderr, "Error creating download directory %s\n", download_dir);
            return 1;
        }
    }

    const char* filename;
    char full_path[PATH_MAX];
    for(size_t i = 0; i < pkg->source_count; i++) {
        if (!pkg->source[i]) continue; // Skip if null entry

        filename = strrchr(pkg->source[i], '/');
        filename = filename ? filename + 1 : pkg->source[i];

        snprintf(full_path, sizeof(full_path), "%s/%s", download_dir, filename);

        if(access(full_path, F_OK) == 0) {
            printf("Skipping download: %s already downloaded!\n", filename);
            continue;
        }

        if(quiet==0){printf("Download %s to %s\n", pkg->source[i], full_path);}
        if (pull_files(pkg->source[i], full_path, quiet)) {
            fprintf(stderr, "Error downloading %s\n", pkg->source[i]);
            return 1;
        }

        if(quiet==0){printf("%s Completed!", filename);}
    }
    if(quiet == 1){printf(" Done!\n");}
    return 0;
}

int fetch_patches(const Package *pkg, int quiet){
    if(quiet == 1){printf("Fetching patches... ");}
    const char* filename;
    char full_path[PATH_MAX];
    for(size_t i = 0; i < pkg->patch_count; i++) {
        filename = strrchr(pkg->patches[i], '/');
        filename = filename ? filename + 1 : pkg->patches[i];

        snprintf(full_path, sizeof(full_path), "%s/%s", download_dir, filename);

        if(access(full_path, F_OK) == 0) {
            printf("Skipping download: %s already downloaded!\n", filename);
            continue;
        }

        if(quiet==0){printf("Download %s to %s\n", pkg->patches[i], full_path);}
        if (pull_files(pkg->patches[i], full_path, quiet)) {
            fprintf(stderr, "Error downloading %s\n", pkg->patches[i]);
            return 1;
        }

        pull_files(pkg->patches[i], full_path, quiet);

        if(quiet==0){printf("%s Completed!", filename);}
    }
    if(quiet == 1){printf(" Done!\n");}
    return 0;
}

// Check for root privileges
int check_root() {
    uid_t uid = getuid();
    if (uid != 0) {
        fprintf(stderr, "PMS requires root privileges to install packages.\n");
        fprintf(stderr, "Please run with sudo/doas.\n");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    // Long name variants of commands | Moving it down here for ez of access
    static const struct option long_options[] = {
        {"help",    no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {"box",     no_argument, 0, 'B'},
        {"quiet",   no_argument, 0, 'q'},
        {0, 0, 0, 0} // Null terminator for the long_options array
    };
    int option_index = 0;

    int quiet = 0;

    for (int opt; (opt = getopt_long(argc, argv, "h::", long_options, &option_index)) != -1;) {

    switch (opt) {
        case 'h': // Help option
            printf("Usage: %s [options] <pkgbuild.json>\n", argv[0]);
            printf("Options:\n");
            printf("    -h, --help      Display this help message\n");
            printf("    -v, --version   Display version information\n");
            printf("    -q, --quiet,    Display less information about package downloading");
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
            fprintf(stderr, "Unknown option: %s\n", argv[optind-1]);
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
    fetch_patches(&pkg, quiet);

    // Implement dependency resolution here later, way too lazy for that for now

    int ret = execute_build(&pkg, quiet);
    free_package(&pkg);
    return ret;
}
