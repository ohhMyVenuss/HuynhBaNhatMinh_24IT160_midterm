/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * traverse.h - Declarations for directory reading and recursive traversal.
 */

#ifndef TRAVERSE_H
#define TRAVERSE_H

#include "ls.h"

void entry_list_init(entry_list_t *list);
void entry_list_add(entry_list_t *list, const char *name, const char *path, const struct stat *sb, int stat_ok);
void entry_list_free(entry_list_t *list);
int list_directory(const char *dirpath, const options_t *opts, int print_header, int *p_first_printed, int is_top_level);

#endif /* TRAVERSE_H */
