/* See LICENSE file for license details.
 *
 * repo.h
 */

/* repository struct */
typedef struct {
        const char *name;       /* Repository name */
        const char *url;        /* Repository URL */
} Repository;

int search_all_repos(const char *package_name, const char *version, Package *pkg);
