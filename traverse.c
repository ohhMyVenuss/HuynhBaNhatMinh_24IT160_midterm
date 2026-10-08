/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * traverse.c - Implementation of directory reading, entry filtering (-a, -A, -f),
 *              and recursive traversal (-R).
 */

#include "ls.h"
#include "util.h"
#include "sort.h"
#include "display.h"
#include "traverse.h"

/*
 * Initialize dynamic entry list.
 */
void
entry_list_init(entry_list_t *list)
{
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

/*
 * Add a new entry to the dynamic list.
 */
void
entry_list_add(entry_list_t *list, const char *name, const char *path,
               const struct stat *sb, int stat_ok)
{
    if (list->count >= list->capacity) {
        size_t new_cap = (list->capacity == 0) ? 16 : (list->capacity * 2);
        list->entries = (file_entry_t *)xrealloc(list->entries, new_cap * sizeof(file_entry_t));
        list->capacity = new_cap;
    }

    file_entry_t *e = &list->entries[list->count++];
    e->name = xstrdup(name);
    e->path = xstrdup(path);
    e->stat_ok = stat_ok;
    e->link_target = NULL;

    if (stat_ok && sb != NULL) {
        e->sb = *sb;
        e->is_dir = S_ISDIR(sb->st_mode);
    } else {
        memset(&e->sb, 0, sizeof(struct stat));
        e->is_dir = 0;
    }
}

/*
 * Free all memory associated with the entry list.
 */
void
entry_list_free(entry_list_t *list)
{
    if (list == NULL || list->entries == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->entries[i].name);
        free(list->entries[i].path);
        if (list->entries[i].link_target != NULL) {
            free(list->entries[i].link_target);
        }
    }
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

/*
 * Check whether a directory entry should be displayed according to -a, -A, and -f.
 */
static int
should_include_entry(const char *name, const options_t *opts)
{
    /* -a or -f includes all entries without exception */
    if (opts->opt_a || opts->opt_f) {
        return 1;
    }

    /* -A includes dot entries except '.' and '..' */
    if (opts->opt_A) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            return 0;
        }
        return 1;
    }

    /* Default: skip entries starting with '.' */
    if (name[0] == '.') {
        return 0;
    }

    return 1;
}

/*
 * Read, sort, and display the contents of a directory.
 * If -R is enabled, recursively traverse subdirectories.
 */
int
list_directory(const char *dirpath, const options_t *opts,
               int print_header, int *p_first_printed, int is_top_level)
{
    (void)is_top_level;
    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        fprintf(stderr, "ls: %s: %s\n", dirpath, strerror(errno));
        g_exit_status = 1;
        return -1;
    }

    entry_list_t dir_entries;
    entry_list_init(&dir_entries);

    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        if (!should_include_entry(dp->d_name, opts)) {
            continue;
        }

        char *full_path = path_join(dirpath, dp->d_name);
        struct stat sb;
        int stat_ok = (lstat(full_path, &sb) == 0);
        if (!stat_ok) {
            /* lstat failed */
            fprintf(stderr, "ls: %s: %s\n", full_path, strerror(errno));
            g_exit_status = 1;
        }

        entry_list_add(&dir_entries, dp->d_name, full_path, &sb, stat_ok);
        free(full_path);
    }
    closedir(dir);

    /* Sort directory entries */
    sort_entry_list(&dir_entries, opts);

    /* Print directory header if required */
    if (print_header) {
        if (*p_first_printed > 0) {
            putchar('\n');
        }
        printf("%s:\n", dirpath);
        *p_first_printed = 1;
    } else if (dir_entries.count > 0) {
        *p_first_printed = 1;
    }

    /* Display sorted entries */
    display_entry_list(&dir_entries, opts, 1);

    /* If -R (recursive) is requested, recurse into subdirectories */
    if (opts->opt_R) {
        for (size_t i = 0; i < dir_entries.count; i++) {
            const file_entry_t *e = &dir_entries.entries[i];

            /*
             * Do not recurse into '.' or '..' to avoid infinite recursion.
             * Also do not recurse into symlinks (lstat ensures S_ISDIR is false for symlinks).
             */
            if (e->is_dir && strcmp(e->name, ".") != 0 && strcmp(e->name, "..") != 0) {
                list_directory(e->path, opts, 1, p_first_printed, 0);
            }
        }
    }

    entry_list_free(&dir_entries);
    return 0;
}
