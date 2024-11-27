#define VERSION "0.0.1-beta"
#define BOX " _____\n|     |\n|     |\n|     | <== GO INSIDE\n\\-----/\n"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <unistd.h>
#include <getopt.h>

// Long name variants of commands
static const struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"box", no_argument, 0, 'B'},
    {0, 0, 0, 0} // Null terminator for the long_options array
};

// Structure to hold package information
typedef struct {
    char *pkgname;
    char *version;
    char **source;
    char **build;
    char **depends;

    size_t source_count;
    size_t build_count;
    size_t depends_count;
} Package;

// Function to parse the JSON PKGBUILD
int parse_pkgbuild(const char *filename, Package *pkg) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening PKGBUILD file");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buffer = malloc(file_size + 1);
    fread(buffer, 1, file_size, fp);
    buffer[file_size] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);  // Free the file buffer
    if (!root) {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        return 1;
    }

    // Extract pkgname
    cJSON *pkgname_json = cJSON_GetObjectItemCaseSensitive(root, "pkgname");
    if (cJSON_IsString(pkgname_json) && (pkgname_json->valuestring != NULL)) {
        pkg->pkgname = strdup(pkgname_json->valuestring);
    }

    //Extract sources, array handling
    cJSON *source_json = cJSON_GetObjectItemCaseSensitive(root, "source");
    if (cJSON_IsArray(source_json)) {
        pkg->source_count = cJSON_GetArraySize(source_json);
        pkg->source = (char **)malloc(pkg->source_count * sizeof(char *));
        for (size_t i = 0; i < pkg->source_count; i++) {
            cJSON *source_item = cJSON_GetArrayItem(source_json, i);
            if (cJSON_IsString(source_item) && (source_item->valuestring != NULL)) {
                pkg->source[i] = strdup(source_item->valuestring);
            }
        }
    }

     //Extract build scripts, array handling
    cJSON *build_json = cJSON_GetObjectItemCaseSensitive(root, "build");
    if (cJSON_IsArray(build_json)) {
        pkg->build_count = cJSON_GetArraySize(build_json);
        pkg->build = (char **)malloc(pkg->build_count * sizeof(char *));
        for (size_t i = 0; i < pkg->build_count; i++) {
            cJSON *build_item = cJSON_GetArrayItem(build_json, i);
            if (cJSON_IsString(build_item) && (build_item->valuestring != NULL)) {
                pkg->build[i] = strdup(build_item->valuestring);
            }
        }
    }

    cJSON_Delete(root); // Free cJSON data structure
    return 0;
}


// Function to execute build commands
int execute_build(const Package *pkg) {
    for (size_t i = 0; i < pkg->build_count; i++) {
        printf("Executing: %s\n", pkg->build[i]);

        if (system(pkg->build[i]) != 0) {
            fprintf(stderr, "Error executing command: %s\n", pkg->build[i]);
            return 1;
        }

    }
    return 0;
}

int main(int argc, char *argv[]) {
    int option_index = 0;    

    for (int opt; (opt = getopt_long(argc, argv, "h::", long_options, &option_index)) != -1;) {

    switch (opt) {
        case 'h': // Help option
            printf("Usage: %s [options] <pkgbuild.json>\n", argv[0]);
            printf("Options:\n");
            printf("    -h, --help      Display this help message\n");
            printf("    -v, --version   Display version information\n");
            return 0;  
        case 'V': // Version option
            printf("pms - Pack My Sh*t version: %s\n", VERSION);
            return 0;
        case 'B':
            printf(BOX);
            return 0;
        case '?': // Invalid option
            fprintf(stderr, "Unknown option: %s\n", argv[optind-1]);
            return 1;
        default: // Unexpected cases
            abort();  
        }
    }

    /* // Old stuff, we adding parsing args now so this is not needed anymore
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pkg.json>\n", argv[0]);
        return 1;
    }

    Package *pkg = parse_pkgbuild(argv[1]);
    if (!pkg) {
        fprintf(stderr, "Error parsing pkg file.\n");
        return 1;

    }*/

    if (optind < argc) { // Check if a pkgbuild.json file was provided
        char *filename = argv[optind];

        Package pkg = {0};
        if (parse_pkgbuild(filename, &pkg) != 0) {
            fprintf(stderr, "Error parsing PKG file.\n");
            return 1;
        }
        // ... (rest of the processing using the filename)


    } else{
        printf("Usage: %s [options] <pkgbuild.json>\n\n", argv[0]);
        fprintf(stderr, "Missing pkg.json argument\n");
        return 1;

    }

        // Implement dependency resolution here later, way too lazy for that for now

    return 0;
}
