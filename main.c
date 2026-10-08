/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * main.c - Program entry point, command-line argument parsing,
 *          operand separation, and execution coordination.
 */

#include "ls.h"
#include "util.h"
#include "sort.h"
#include "display.h"
#include "traverse.h"

/* Global exit status: 0 on success, >0 if any error occurs */
int g_exit_status = 0;

/*
 * Print usage message and exit with failure.
 */
static void
usage(void)
{
    fprintf(stderr, "usage: ls [-AacdFfhiklnqRrSstuw] [file ...]\n");
    exit(EXIT_FAILURE);
}

/*
 * Determine if an argument path has a trailing slash.
 */
static int
has_trailing_slash(const char *path)
{
    size_t len = strlen(path);
    return (len > 0 && path[len - 1] == '/');
}

int
main(int argc, char *argv[])
{
    options_t opts;
    memset(&opts, 0, sizeof(opts));

    /* Check whether stdout is connected to a terminal */
    opts.stdout_isatty = isatty(STDOUT_FILENO);

    /*
     * Default non-printable characters formatting:
     * -q is default when output is to a terminal;
     * -w is default when output is not to a terminal.
     */
    if (opts.stdout_isatty) {
        opts.opt_q = 1;
        opts.opt_w = 0;
    } else {
        opts.opt_w = 1;
        opts.opt_q = 0;
    }

    opts.sort_type = SORT_NAME;
    opts.time_type = TIME_MODIFICATION;

    /* Track explicitly requested options to handle overrides */
    int ch;
    while ((ch = getopt(argc, argv, "AacdFfhiklnqRrSstuw")) != -1) {
        switch (ch) {
        case 'A':
            opts.opt_A = 1;
            break;
        case 'a':
            opts.opt_a = 1;
            break;
        case 'c':
            /* -c and -u override each other; last one specified wins */
            opts.opt_c = 1;
            opts.opt_u = 0;
            opts.time_type = TIME_STATUS_CHANGE;
            break;
        case 'd':
            /* -R and -d override each other; last one specified wins */
            opts.opt_d = 1;
            opts.opt_R = 0;
            break;
        case 'F':
            opts.opt_F = 1;
            break;
        case 'f':
            /* -f: output is not sorted, implies -a */
            opts.opt_f = 1;
            opts.opt_a = 1;
            opts.sort_type = SORT_NONE;
            break;
        case 'h':
            /* -k and -h override each other; last one specified wins */
            opts.opt_h = 1;
            opts.opt_k = 0;
            break;
        case 'i':
            opts.opt_i = 1;
            break;
        case 'k':
            /* -k and -h override each other; last one specified wins */
            opts.opt_k = 1;
            opts.opt_h = 0;
            break;
        case 'l':
            /* -l and -n override each other; last one specified wins */
            opts.opt_l = 1;
            opts.opt_n = 0;
            break;
        case 'n':
            /* -l and -n override each other; last one specified wins */
            opts.opt_n = 1;
            opts.opt_l = 0;
            break;
        case 'q':
            /* -w and -q override each other; last one specified wins */
            opts.opt_q = 1;
            opts.opt_w = 0;
            break;
        case 'R':
            /* -R and -d override each other; last one specified wins */
            opts.opt_R = 1;
            opts.opt_d = 0;
            break;
        case 'r':
            opts.opt_r = 1;
            break;
        case 'S':
            /* -S and -t: last sort criterion specified wins */
            if (opts.sort_type != SORT_NONE) {
                opts.sort_type = SORT_SIZE;
            }
            opts.opt_S = 1;
            opts.opt_t = 0;
            break;
        case 's':
            opts.opt_s = 1;
            break;
        case 't':
            /* -S and -t: last sort criterion specified wins */
            if (opts.sort_type != SORT_NONE) {
                opts.sort_type = SORT_TIME;
            }
            opts.opt_t = 1;
            opts.opt_S = 0;
            break;
        case 'u':
            /* -c and -u override each other; last one specified wins */
            opts.opt_u = 1;
            opts.opt_c = 0;
            opts.time_type = TIME_ACCESS;
            break;
        case 'w':
            /* -w and -q override each other; last one specified wins */
            opts.opt_w = 1;
            opts.opt_q = 0;
            break;
        default:
            usage();
            break;
        }
    }

    /*
     * Super-user default:
     * -A is always set for the super-user unless -a is set.
     */
    if (geteuid() == 0 && !opts.opt_a) {
        opts.opt_A = 1;
    }

    /* Initialize block size from -k, -h, or BLOCKSIZE environment */
    init_blocksize(&opts);

    int num_operands = argc - optind;
    int first_printed = 0;

    /*
     * Case 1: No operands specified -> list current directory "."
     */
    if (num_operands == 0) {
        if (opts.opt_d) {
            /* With -d, display "." as an entry itself */
            entry_list_t single_list;
            entry_list_init(&single_list);
            struct stat sb;
            int stat_ok = (lstat(".", &sb) == 0);
            entry_list_add(&single_list, ".", ".", &sb, stat_ok);
            display_entry_list(&single_list, &opts, 0);
            entry_list_free(&single_list);
        } else {
            list_directory(".", &opts, 0, &first_printed, 1);
        }
        return g_exit_status;
    }

    /*
     * Case 2: One or more operands given.
     * "If more than one operand is given, non-directory operands are
     *  displayed first; directory and non-directory operands are sorted
     *  separately and in lexicographical order."
     */
    entry_list_t file_operands;
    entry_list_t dir_operands;
    entry_list_init(&file_operands);
    entry_list_init(&dir_operands);

    for (int i = optind; i < argc; i++) {
        const char *arg = argv[i];
        struct stat sb;
        int stat_ok = 0;
        int is_directory = 0;

        if (opts.opt_d) {
            /*
             * With -d: directories are treated as plain files and symbolic links
             * in argument list are not indirected through.
             */
            stat_ok = (lstat(arg, &sb) == 0);
            if (!stat_ok) {
                fprintf(stderr, "ls: %s: %s\n", arg, strerror(errno));
                g_exit_status = 1;
                continue;
            }
            is_directory = 0;
        } else {
            /*
             * Without -d:
             * If trailing slash is present, kernel dereferences symlink.
             * If -l or -F is set (without trailing slash), lstat is checked first
             * so symbolic links are listed as symlink files rather than directories.
             * Otherwise, stat is checked first to indirect through symlinks to directories.
             */
            int use_lstat_first = (opts.opt_l || opts.opt_n || opts.opt_F) && !has_trailing_slash(arg);

            if (use_lstat_first) {
                if (lstat(arg, &sb) == 0) {
                    stat_ok = 1;
                    if (S_ISDIR(sb.st_mode)) {
                        is_directory = 1;
                    } else {
                        is_directory = 0;
                    }
                } else {
                    fprintf(stderr, "ls: %s: %s\n", arg, strerror(errno));
                    g_exit_status = 1;
                    continue;
                }
            } else {
                if (stat(arg, &sb) == 0) {
                    stat_ok = 1;
                    if (S_ISDIR(sb.st_mode)) {
                        is_directory = 1;
                    } else {
                        is_directory = 0;
                    }
                } else if (lstat(arg, &sb) == 0) {
                    /* stat failed (e.g. broken symlink), but lstat succeeded */
                    stat_ok = 1;
                    is_directory = 0;
                } else {
                    fprintf(stderr, "ls: %s: %s\n", arg, strerror(errno));
                    g_exit_status = 1;
                    continue;
                }
            }
        }

        if (is_directory) {
            entry_list_add(&dir_operands, arg, arg, &sb, stat_ok);
        } else {
            entry_list_add(&file_operands, arg, arg, &sb, stat_ok);
        }
    }

    /* Sort non-directory and directory operands separately */
    sort_entry_list(&file_operands, &opts);
    sort_entry_list(&dir_operands, &opts);

    /* Display non-directory operands first */
    if (file_operands.count > 0) {
        display_entry_list(&file_operands, &opts, 0);
        first_printed = 1;
    }

    /*
     * Display directory operands.
     * Print directory header if:
     * - More than 1 operand was provided overall, OR
     * - -R is enabled.
     */
    int print_header = (num_operands > 1 || opts.opt_R);
    for (size_t i = 0; i < dir_operands.count; i++) {
        const file_entry_t *dentry = &dir_operands.entries[i];
        list_directory(dentry->path, &opts, print_header, &first_printed, 1);
    }

    entry_list_free(&file_operands);
    entry_list_free(&dir_operands);

    return g_exit_status;
}
