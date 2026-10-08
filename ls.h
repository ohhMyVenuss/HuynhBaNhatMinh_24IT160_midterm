/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * ls.h - Core header file containing data structures, macros,
 *        and function declarations for the ls program.
 */

#ifndef LS_H
#define LS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>

/* Time type to use for sorting and displaying */
typedef enum {
    TIME_MODIFICATION = 0, /* st_mtime (default) */
    TIME_ACCESS,           /* st_atime (-u) */
    TIME_STATUS_CHANGE     /* st_ctime (-c) */
} time_type_t;

/* Sort criterion */
typedef enum {
    SORT_NAME = 0, /* Lexicographical (default) */
    SORT_TIME,     /* By time (-t) */
    SORT_SIZE,     /* By file size (-S) */
    SORT_NONE      /* Unsorted (-f) */
} sort_type_t;

/* Command-line options structure */
typedef struct {
    int opt_A;             /* -A: list all except . and .. */
    int opt_a;             /* -a: include entries starting with . */
    int opt_c;             /* -c: use status change time */
    int opt_d;             /* -d: list directories as plain files */
    int opt_F;             /* -F: display classification symbols */
    int opt_f;             /* -f: unsorted output, enables -a */
    int opt_h;             /* -h: human-readable sizes (bytes) */
    int opt_i;             /* -i: print inode number */
    int opt_k;             /* -k: sizes/blocks in kilobytes */
    int opt_l;             /* -l: long listing format */
    int opt_n;             /* -n: long listing with numeric UID/GID */
    int opt_q;             /* -q: force '?' for non-printable characters */
    int opt_R;             /* -R: recursively list subdirectories */
    int opt_r;             /* -r: reverse sort order */
    int opt_S;             /* -S: sort by size (largest first) */
    int opt_s;             /* -s: print block count */
    int opt_t;             /* -t: sort by time (newest first) */
    int opt_u;             /* -u: use access time */
    int opt_w;             /* -w: force raw non-printable characters */

    time_type_t time_type; /* Active time type (mtime, atime, or ctime) */
    sort_type_t sort_type; /* Active sort type (name, time, size, none) */
    long blocksize;        /* Calculated block size (512, 1024, or BLOCKSIZE) */
    int stdout_isatty;     /* 1 if stdout is a terminal, 0 otherwise */
} options_t;

/* Information about a single file or directory entry */
typedef struct {
    char *name;            /* Display name (e.g. "foo" or "path/foo") */
    char *path;            /* Full path used for stat/lstat */
    struct stat sb;        /* Stat buffer */
    int stat_ok;           /* 1 if stat succeeded, 0 otherwise */
    char *link_target;     /* Symlink target if entry is a symlink, else NULL */
    int is_dir;            /* 1 if entry is a directory, 0 otherwise */
} file_entry_t;

/* Dynamic list of file entries */
typedef struct {
    file_entry_t *entries;
    size_t count;
    size_t capacity;
} entry_list_t;

/* Column widths used for aligned long output */
typedef struct {
    int max_inode_w;
    int max_blocks_w;
    int max_nlink_w;
    int max_owner_w;
    int max_group_w;
    int max_size_w;
    int max_major_w;
    int max_minor_w;
    int has_device;
} col_widths_t;

/* Global exit status variable (0 on success, >0 if any error occurred) */
extern int g_exit_status;

/* Function prototypes from util.c */
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

/* Function prototypes from sort.c */
void sort_entry_list(entry_list_t *list, const options_t *opts);

/* Function prototypes from display.c */
void display_entry_list(const entry_list_t *list, const options_t *opts, int is_dir_contents);

/* Function prototypes from traverse.c */
void entry_list_init(entry_list_t *list);
void entry_list_add(entry_list_t *list, const char *name, const char *path, const struct stat *sb, int stat_ok);
void entry_list_free(entry_list_t *list);
int list_directory(const char *dirpath, const options_t *opts, int print_header, int *p_first_printed, int is_top_level);

#endif /* LS_H */
