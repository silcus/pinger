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
void gtk_interface_init(int *argc, char **argv[], char *title, hosts_data *hosts);

/* interface done */
void gtk_interface_done(char *error_str);

/* locale_to_utf8 conversion with error handling */
gchar *my_locale_to_utf8(char *text, char *file, int line);

/* get single host status and display it */
int gtk_show_status(host_data * host);

/* delete_event callback function */
gint gtk_delete_event( GtkWidget *widget,
                   GdkEvent  *event,
                   gpointer   data );

void gtk_gui_loop(hosts_data *hosts, int *stop_loop);

