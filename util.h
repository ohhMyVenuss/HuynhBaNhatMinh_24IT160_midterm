/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * util.h - Declarations for utility helper functions.
 */

#ifndef UTIL_H
#define UTIL_H

#include "ls.h"

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);
char *path_join(const char *dir, const char *file);
void init_blocksize(options_t *opts);
int64_t compute_blocks(int64_t st_blocks, long blocksize);
void format_mode_string(mode_t mode, char *buf);
void format_entry_time(const file_entry_t *entry, const options_t *opts, char *buf, size_t size);
void format_human_number(char *buf, size_t len, int64_t num);
char get_classification_char(mode_t mode);
void print_safe_filename(const char *name, const options_t *opts);

#endif /* UTIL_H */
