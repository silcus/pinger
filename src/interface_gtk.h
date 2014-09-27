#include "globals.h"


/* colors for GTK */
#define OFFLINE_COLOR "red"
#define ONLINE_COLOR "blue"
#define NW_UNREACH_COLOR "gray"
#define LONG_REPLY_COLOR "brown"

#define FONT_FAMILY "Arial"

#define MAX_STATUS_BAR_TEXT_LEN 50

/* FIXME: maybe extern is not necessary */
/*extern GtkWidget *window;
extern GtkWidget *list;
*/
/* interface init */
void gtk_interface_init(int *argc, char **argv[], char *title,
                        hosts_data * hosts);

/* interface done */
void gtk_interface_done(char *error_str);

/* get single host status and display it */
int gtk_show_status(host_data * host);

void gtk_gui_loop(hosts_data * hosts, int *stop_loop);
