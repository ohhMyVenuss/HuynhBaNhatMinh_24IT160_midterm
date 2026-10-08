/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * display.c - Implementation of output formatting: default single-column,
 *            long listing (-l, -n), inode (-i), blocks (-s), humanized sizes (-h),
 *            and classification symbols (-F).
 */

#include "ls.h"
#include "util.h"
#include "display.h"

#ifndef major
#define major(dev) ((int)(((dev) >> 8) & 0xff))
#endif
#ifndef minor
#define minor(dev) ((int)((dev) & 0xff))
#endif

/*
 * Calculate column widths and total blocks across the entry list.
 */
static void
calculate_widths(const entry_list_t *list, const options_t *opts,
                 col_widths_t *w, int64_t *total_blocks_512)
{
    memset(w, 0, sizeof(col_widths_t));
    *total_blocks_512 = 0;

    for (size_t i = 0; i < list->count; i++) {
        const file_entry_t *entry = &list->entries[i];
        if (!entry->stat_ok) {
            continue;
        }

        *total_blocks_512 += entry->sb.st_blocks;

        /* Inode width */
        if (opts->opt_i) {
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "%llu", (unsigned long long)entry->sb.st_ino);
            if (len > w->max_inode_w) {
                w->max_inode_w = len;
            }
        }

        /* Block count width */
        if (opts->opt_s) {
            char buf[32];
            if (opts->opt_h) {
                format_human_number(buf, sizeof(buf), (int64_t)entry->sb.st_blocks * 512);
            } else {
                int64_t blks = compute_blocks(entry->sb.st_blocks, opts->blocksize);
                snprintf(buf, sizeof(buf), "%" PRId64, blks);
            }
            int len = (int)strlen(buf);
            if (len > w->max_blocks_w) {
                w->max_blocks_w = len;
            }
        }

        /* Number of links */
        {
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "%u", (unsigned int)entry->sb.st_nlink);
            if (len > w->max_nlink_w) {
                w->max_nlink_w = len;
            }
        }

        /* Owner width */
        {
            char buf[64];
            if (opts->opt_n) {
                snprintf(buf, sizeof(buf), "%u", (unsigned int)entry->sb.st_uid);
            } else {
                struct passwd *pw = getpwuid(entry->sb.st_uid);
                if (pw != NULL) {
                    snprintf(buf, sizeof(buf), "%s", pw->pw_name);
                } else {
                    snprintf(buf, sizeof(buf), "%u", (unsigned int)entry->sb.st_uid);
                }
            }
            int len = (int)strlen(buf);
            if (len > w->max_owner_w) {
                w->max_owner_w = len;
            }
        }

        /* Group width */
        {
            char buf[64];
            if (opts->opt_n) {
                snprintf(buf, sizeof(buf), "%u", (unsigned int)entry->sb.st_gid);
            } else {
                struct group *gr = getgrgid(entry->sb.st_gid);
                if (gr != NULL) {
                    snprintf(buf, sizeof(buf), "%s", gr->gr_name);
                } else {
                    snprintf(buf, sizeof(buf), "%u", (unsigned int)entry->sb.st_gid);
                }
            }
            int len = (int)strlen(buf);
            if (len > w->max_group_w) {
                w->max_group_w = len;
            }
        }

        /* Size or device width */
        if (S_ISCHR(entry->sb.st_mode) || S_ISBLK(entry->sb.st_mode)) {
            w->has_device = 1;
            char maj_buf[32], min_buf[32];
            int maj_len = snprintf(maj_buf, sizeof(maj_buf), "%d", major(entry->sb.st_rdev));
            int min_len = snprintf(min_buf, sizeof(min_buf), "%d", minor(entry->sb.st_rdev));
            if (maj_len > w->max_major_w) {
                w->max_major_w = maj_len;
            }
            if (min_len > w->max_minor_w) {
                w->max_minor_w = min_len;
            }
        } else {
            char buf[64];
            if (opts->opt_h) {
                format_human_number(buf, sizeof(buf), (int64_t)entry->sb.st_size);
            } else {
                snprintf(buf, sizeof(buf), "%" PRId64, (int64_t)entry->sb.st_size);
            }
            int len = (int)strlen(buf);
            if (len > w->max_size_w) {
                w->max_size_w = len;
            }
        }
    }

    if (w->has_device) {
        /* Align size width with device width (major + ", " + minor) */
        int dev_w = w->max_major_w + 2 + w->max_minor_w;
        if (dev_w > w->max_size_w) {
            w->max_size_w = dev_w;
        }
    }

    if (opts->opt_h) {
        if (w->max_size_w < 4) {
            w->max_size_w = 4;
        }
        if (w->max_blocks_w < 4) {
            w->max_blocks_w = 4;
        }
    }
}

/*
 * Print a single entry in long format (-l or -n).
 */
static void
print_entry_long(const file_entry_t *entry, const options_t *opts, const col_widths_t *w)
{
    /* 1. Inode number (-i) */
    if (opts->opt_i) {
        if (entry->stat_ok) {
            printf("%*llu ", w->max_inode_w, (unsigned long long)entry->sb.st_ino);
        } else {
            printf("%*s ", w->max_inode_w, "?");
        }
    }

    /* 2. Block count (-s) */
    if (opts->opt_s) {
        if (entry->stat_ok) {
            char blk_buf[32];
            if (opts->opt_h) {
                format_human_number(blk_buf, sizeof(blk_buf), (int64_t)entry->sb.st_blocks * 512);
            } else {
                int64_t blks = compute_blocks(entry->sb.st_blocks, opts->blocksize);
                snprintf(blk_buf, sizeof(blk_buf), "%" PRId64, blks);
            }
            printf("%*s ", w->max_blocks_w, blk_buf);
        } else {
            printf("%*s ", w->max_blocks_w, "?");
        }
    }

    if (!entry->stat_ok) {
        /* Stat failed on entry */
        printf("??????????  ? ? ? ? ???????????? ");
        print_safe_filename(entry->name, opts);
        putchar('\n');
        return;
    }

    /* 3. Mode string (10 chars + 2 spaces) */
    char mode_str[16];
    format_mode_string(entry->sb.st_mode, mode_str);
    printf("%s  ", mode_str);

    /* 4. Link count */
    printf("%*u ", w->max_nlink_w, (unsigned int)entry->sb.st_nlink);

    /* 5. Owner name or numeric UID */
    char owner_buf[64];
    if (opts->opt_n) {
        snprintf(owner_buf, sizeof(owner_buf), "%u", (unsigned int)entry->sb.st_uid);
    } else {
        struct passwd *pw = getpwuid(entry->sb.st_uid);
        if (pw != NULL) {
            snprintf(owner_buf, sizeof(owner_buf), "%s", pw->pw_name);
        } else {
            snprintf(owner_buf, sizeof(owner_buf), "%u", (unsigned int)entry->sb.st_uid);
        }
    }
    printf("%-*s  ", w->max_owner_w, owner_buf);

    /* 6. Group name or numeric GID */
    char group_buf[64];
    if (opts->opt_n) {
        snprintf(group_buf, sizeof(group_buf), "%u", (unsigned int)entry->sb.st_gid);
    } else {
        struct group *gr = getgrgid(entry->sb.st_gid);
        if (gr != NULL) {
            snprintf(group_buf, sizeof(group_buf), "%s", gr->gr_name);
        } else {
            snprintf(group_buf, sizeof(group_buf), "%u", (unsigned int)entry->sb.st_gid);
        }
    }
    printf("%-*s  ", w->max_group_w, group_buf);

    /* 7. Size or device numbers */
    if (S_ISCHR(entry->sb.st_mode) || S_ISBLK(entry->sb.st_mode)) {
        printf("%*d, %*d ",
               w->max_major_w, major(entry->sb.st_rdev),
               w->max_minor_w, minor(entry->sb.st_rdev));
    } else {
        char sz_buf[64];
        if (opts->opt_h) {
            format_human_number(sz_buf, sizeof(sz_buf), (int64_t)entry->sb.st_size);
        } else {
            snprintf(sz_buf, sizeof(sz_buf), "%" PRId64, (int64_t)entry->sb.st_size);
        }
        printf("%*s ", w->max_size_w, sz_buf);
    }

    /* 8. Date and time (12 chars + 1 space) */
    char time_str[32];
    format_entry_time(entry, opts, time_str, sizeof(time_str));
    printf("%s ", time_str);

    /* 9. Pathname */
    print_safe_filename(entry->name, opts);

    /* 10. Classification symbol (-F) */
    if (opts->opt_F) {
        char suff = get_classification_char(entry->sb.st_mode);
        if (suff != '\0') {
            putchar(suff);
        }
    }

    /* 11. Symlink destination (-> target) */
    if (S_ISLNK(entry->sb.st_mode)) {
        char link_target[PATH_MAX];
        ssize_t len = readlink(entry->path, link_target, sizeof(link_target) - 1);
        if (len >= 0) {
            link_target[len] = '\0';
            printf(" -> ");
            print_safe_filename(link_target, opts);
        }
    }

    putchar('\n');
}

/*
 * Print a single entry in default short format.
 */
static void
print_entry_short(const file_entry_t *entry, const options_t *opts, const col_widths_t *w)
{
    /* Inode (-i) */
    if (opts->opt_i) {
        if (entry->stat_ok) {
            printf("%*llu ", w->max_inode_w, (unsigned long long)entry->sb.st_ino);
        } else {
            printf("%*s ", w->max_inode_w, "?");
        }
    }

    /* Block count (-s) */
    if (opts->opt_s) {
        if (entry->stat_ok) {
            char blk_buf[32];
            if (opts->opt_h) {
                format_human_number(blk_buf, sizeof(blk_buf), (int64_t)entry->sb.st_blocks * 512);
            } else {
                int64_t blks = compute_blocks(entry->sb.st_blocks, opts->blocksize);
                snprintf(blk_buf, sizeof(blk_buf), "%" PRId64, blks);
            }
            printf("%*s ", w->max_blocks_w, blk_buf);
        } else {
            printf("%*s ", w->max_blocks_w, "?");
        }
    }

    /* Filename */
    print_safe_filename(entry->name, opts);

    /* Classification symbol (-F) */
    if (opts->opt_F && entry->stat_ok) {
        char suff = get_classification_char(entry->sb.st_mode);
        if (suff != '\0') {
            putchar(suff);
        }
    }

    putchar('\n');
}

/*
 * Display list of entries, including "total" line when appropriate.
 */
void
display_entry_list(const entry_list_t *list, const options_t *opts, int is_dir_contents)
{
    if (list == NULL || list->count == 0) {
        return;
    }

    col_widths_t widths;
    int64_t total_blocks_512 = 0;
    calculate_widths(list, opts, &widths, &total_blocks_512);

    /*
     * "In addition, for each directory whose contents are displayed,
     *  the total number of blocks in units of 512 bytes or BLOCKSIZE
     *  (see ENVIRONMENT) used by the files in the directory is displayed
     *  on a line by itself immediately before the information for the files."
     *
     * "-s: If the output is to a terminal, a total sum for all the file sizes
     *  is output on a line before the listing."
     */
    if (is_dir_contents) {
        int show_total = (opts->opt_l || opts->opt_n || (opts->opt_s && opts->stdout_isatty));
        if (show_total) {
            if (opts->opt_h) {
                char total_buf[32];
                format_human_number(total_buf, sizeof(total_buf), total_blocks_512 * 512);
                printf("total %s\n", total_buf);
            } else {
                int64_t tot = compute_blocks(total_blocks_512, opts->blocksize);
                printf("total %" PRId64 "\n", tot);
            }
        }
    }

    int is_long = (opts->opt_l || opts->opt_n);
    for (size_t i = 0; i < list->count; i++) {
        if (is_long) {
            print_entry_long(&list->entries[i], opts, &widths);
        } else {
            print_entry_short(&list->entries[i], opts, &widths);
        }
    }
}
