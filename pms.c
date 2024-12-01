#define VERSION "0.0.1-beta"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <cjson/cJSON.h>
#include <curl/curl.h>

// Structure to hold package information
typedef struct {
    char *pkgname;
    char **source;
    char **build;
    char **depends;

    size_t source_count;
    size_t build_count;
    size_t depends_count;
} Package;

// Free everything :P
void free_package(Package *pkg) {
    free(pkg->pkgname);
    for (size_t i = 0; i < pkg->source_count; i++) free(pkg->source[i]);
    for (size_t i = 0; i < pkg->build_count; i++) free(pkg->build[i]);
    for (size_t i = 0; i < pkg->depends_count; i++) free(pkg->depends[i]);
    free(pkg->source);
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
        if (cJSON_IsArray(item)) { \
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
        }

    PARSE_ARRAY(source, source_count);
    PARSE_ARRAY(build, build_count);
    PARSE_ARRAY(depends, depends_count);
    
    cJSON_Delete(root); // Free cJSON data structure
    return 0;
}


// Function to execute build commands
int execute_build(const Package *pkg) {
    for (size_t i = 0; i < pkg->build_count; i++) {
        printf("Executing: %s\n", pkg->build[i]);
        if (system(pkg->build[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

int fetch_tarball(const char *url, const char *filename){
    printf("fetching source...");
    CURL *curl = curl_easy_init();
    if(curl) {
        CURLcode res;
        FILE *source = fopen(filename, "w");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, source);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        res = curl_easy_perform(curl);
        if(!res){
            printf(" FETCH FAILED\n");
            return 1;
        }
        curl_easy_cleanup(curl);
        fclose(source);
    }
    printf(" Done!\n");
    return 0;

}

int main(int argc, char *argv[]) {
    // Long name variants of commands | Moving it down here for ez of access
    static const struct option long_options[] = {
        {"help",    no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {"box",     no_argument, 0, 'B'},
        {0, 0, 0, 0} // Null terminator for the long_options array
    };
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
            printf(" _____\n|     |\n|     |\n|     | <== GO INSIDE\n\\-----/\n");
            return 0;
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
    if (parse_pkgbuild(argv[optind], &pkg) == 0) {
        fprintf(stderr, "Error parsing pkgbuild.json\n");
        return 1;
    }
    int r = fetch_tarball(pkg.source[0], pkg.pkgname);
    if(r == 1){
        return r;
    }

    // Implement dependency resolution here later, way too lazy for that for now


    // NO way CHAT WE ARE RUNNING BUILD SCRIPTS NOW!!!! (still need to pull the sources)
    int ret = execute_build(&pkg);
    free_package(&pkg);
    return ret;
}
