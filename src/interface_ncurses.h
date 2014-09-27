#include <curses.h>
#include "globals.h"

extern WINDOW *main_win;

/* percentage positions of status columns (for ncurses only)
 * For example: Line is 80 characters long. If you have DELAY_POS_RATIO
 * 0.3, so 80*0.3=24. Delay column will be positioned from character 24
 * for each line. */
#define HOST_POS_RATIO 0
#define DELAY_POS_RATIO 0.23
#define NAME_POS_RATIO 0.41
#define STAT_POS_RATIO 0.62
#define SR_POS_RATIO 0.74
#define AVAIL_POS_RATIO 0.91
#define LASTOK_POS_RATIO 1.0

/* maximum line size. If real is longer, text will be displayed in this size
 * anyway. */
#define MAX_LINE_WIDTH 90
#define MAX_LINE_WIDTH_NO_BONUS 80
/* minimum line size. */
#define MIN_LINE_WIDTH 20

/* from which line to display hosts status */
#define NC_STATUS_LINE_FROM 3
/* during window resize sometimes the header ends up on the next line,
 * which is normally not cleared. So this define says which additional
 * line will be cleared during the window resize */
#define LINE_UNDER_HEADER 2

/* define color pairs id for ncurses */
enum ncurses_color_pairs_id
{
    COL_NOCOLOR,
    COL_ONLINE,
    COL_OFFLINE,
    COL_NWUNREACH,
    COL_DELAYED
};

/* color for ncurses */
#define NC_OFFLINE_COLPAIR COLOR_RED, COLOR_BLACK
#define NC_ONLINE_COLPAIR COLOR_CYAN, COLOR_BLACK
#define NC_NW_UNREACH_COLPAIR COLOR_MAGENTA, COLOR_BLACK
#define NC_LONG_REPLY_COLPAIR COLOR_YELLOW, COLOR_BLACK

/* from which position show header in first line */
#define HDR_LINE_X_START 2

/* init interface */
void ncurses_interface_init(int *argc, char **argv[], char *title,
                            hosts_data * hosts);

/* interface done */
void ncurses_interface_done(char *error_str);

void count_line_pos(void **curseswin, int *host_pos, int *delay_pos,
                    int *name_pos, int *stat_pos, int *sr_pos, int *avail_pos,
                    int *lastok_pos);

/* ncurses show single host status */
void show_host_status(host_data * host, int colpair, int attr, char *statstr,
                      char *delaystr, char *srstr, char *availstr,
                      char *lastokstr, int host_set_nr);

/* get single host status and display it */
int ncurses_show_status(host_data * host);

void print_header(char *title);

void ncurses_gui_loop(hosts_data * hosts, int *stop_loop);
