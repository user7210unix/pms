/* See LICENSE file for license details. */

/* function declarations */
int dwn_sources(const Package *pkg);
int dwn_patches(const Package *pkg);

int ext_sources(const Package *pkg);
char *find_src(const Package *pkg, char *src_dir);
int app_patch(const Package *pkg);
