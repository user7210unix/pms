/* See LICENSE file for license details. 
 *
 * dwn.c
 *
 * helps pms to download files, extract, and apply patches
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "pms.h"
#include "config.h"
#include "dwn.h"

static const struct {
        const char *ext;
        const char *fmt;
} cmds[] =      {{".tar.gz", "tar xzf"},  {".tgz", "tar xzf"},
                {".tar.xz", "tar xJf"},  {".txz", "tar xJf"},
                {".tar.bz2", "tar xjf"}, {".zip", "unzip"}}; 

int
dwn(const char *url, const char *file)
{
        if (!quiet) 
                printf("Downloading %s to %s\n", url, dest);
        
        CURL *curl = curl_easy_init();
        if (!curl) {
                fprintf(stderr, "Failed to initialize curl\n");
                return -1;
        }
        
        FILE *source = fopen(file, "w");
        if (!source) {
                fprintf(stderr, "Failed to open file %s\n", file);
                return -1;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, source);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, maxretries);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(source);
        if (quiet == 0)
                printf("Downloaded %s\n", url);

        return res != CURLE_OK;
}

int
dwn_sources(const Package *pkg)
{
        size_t i;
        char *filename;
        const char *url;
        char path[PATH_MAX];

        if (!pkg->src || !pkg->nsrc)
                return 0;

        mkdir(cachedir, 0755);

        for (i=0; i < pkg->nsrc; i++) {
                url = pkg->src[i];
                filename = strrchr(url, '/');
                if(!filename)
                        return -1;
                filename++;

                if (!quiet)
                        printf("Downloading source %s\n", filename);

                snprintf(path, sizeof(path), "%s/%s", cachedir, filename);

                if (!force && access(path, F_OK) == 0) {
                        if (!quiet)
                                printf("File %s already exists, skipping...\n", filename);
                        continue;
                }

                if (dwn(url, path) < 0)
                        return -1;

        }
        return 0;
}

int
dwn_patches(const Package *pkg)
{
        
}

int
ext_sources(const Package *pkg)
{
        
}

char *
find_src(const Package *pkg, char *src_dir)
{
        
}

int
app_patch(const Package *pkg)
{
        
}
