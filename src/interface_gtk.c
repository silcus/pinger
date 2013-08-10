#include "interface_gtk.h"
#include "timefunc.h"
#include "ping.h"
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <string.h>

GtkWidget *window;
/* lists is one window containing several rows */
GtkWidget *list;
GtkWidget *scrolled_list;
/* tab has name and contains widget (list for example) */
GtkWidget *tabs;
/* status bar for clock */  
GtkWidget *status_bar;
guint status_bar_context;
/* Box where store tabs and status bar */
GtkWidget *vbox;

/* each list has model which says how lists looks like. I need these models */
typedef struct models_t {
  GtkListStore *model;
  struct models_t* next;
  int id;
} gtk_models;

gtk_models *lists_models;

struct regular_disp_data {
	hosts_data *hosts;
	int *stop_loop;
  guint disp_timeout_id;
};

/* Interface display data */
struct regular_disp_data * int_disp_data;

/* define columns */
enum
{
   HOSTNAME_COLUMN,
   HOSTIP_COLUMN,
   DELAY_COLUMN,
   STATUS_COLUMN,
   SENREC_COLUMN,
   AVAIL_COLUMN,
   LASTOK_COLUMN,
   COLOR_COLUMN,
   N_COLUMNS
};

GtkWidget *create_list(hosts_data * hosts, int set_nr, GtkListStore **model);
gboolean display_pinger_status(gpointer data);

void tab_refresh_rate(GtkWidget *widget, GdkEvent  *event,
                       gpointer   data )
{
  g_source_remove(int_disp_data->disp_timeout_id);
  int_disp_data->disp_timeout_id = g_timeout_add( (guint32) *(int *)data, display_pinger_status, 
			(gpointer) int_disp_data);
}

static void key_pressed(GtkWidget* win, GdkEventKey *event)
{
  if ((event->keyval == GDK_q) || (event->keyval == GDK_Q))
    gtk_main_quit();
}

void gtk_interface_init(int *argc, char **argv[], char *title, hosts_data *hosts)
{
  char *titlestr;
  int a;
  titlelist *titles;
  gtk_models *models;

  gtk_init(argc, argv);
  /* Create new window with title */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  titlestr = malloc(strlen(VERSION) + strlen("Pinger v") + 
      strlen(_("'q' key to quit")) + 3 + 1);
  strcpy(titlestr, "Pinger v");
  strcat(titlestr, VERSION);
  strcat(titlestr, " - ");
  strcat(titlestr, _("'q' key to quit"));
  gtk_window_set_title (GTK_WINDOW (window), my_locale_to_utf8(titlestr, __FILE__, __LINE__));
  free(titlestr);

  /* hook delete_event function to exit state, this will free allocated sockets */
  g_signal_connect (G_OBJECT (window), "delete_event",
          G_CALLBACK (gtk_delete_event), (gpointer) hosts);

  /* any key causes immediate quit */
  g_signal_connect(G_OBJECT(window), "key_press_event",
          G_CALLBACK (key_pressed), NULL);
  
  /* create tabs */
  tabs = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
  gtk_notebook_set_scrollable(GTK_NOTEBOOK(tabs), TRUE);
  
  /* create lists and its models from hosts structure */
  models = NULL;
  titles = hosts->titles;
  for (a = 0; titles != NULL; a++) {
    
    if (a == 0) {
      models = malloc(sizeof(gtk_models));
      lists_models = models;
    } else {
      models->next = malloc(sizeof(gtk_models));
      models = models->next;
    }
    models->next = NULL;
    models->id = titles->nr;
    
    list = (GtkWidget *)create_list(hosts, a, &models->model);
    g_signal_connect (G_OBJECT (list), "visibility-notify-event",
                  G_CALLBACK (tab_refresh_rate), (gpointer)(&(titles->refresh_int)));

    /* make it scrollable */
    scrolled_list = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_add_with_viewport((GtkScrolledWindow *)scrolled_list, list);
    gtk_scrolled_window_set_policy((GtkScrolledWindow *)scrolled_list, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), scrolled_list, \
        gtk_label_new(my_locale_to_utf8(titles->title, __FILE__, __LINE__)));
    gtk_widget_show (list);
    gtk_widget_show (scrolled_list);
    /* now list is assigned and we can use list variable for another list */
    titles = titles->next;
  }

  /* create status bar */
  status_bar = gtk_statusbar_new();
  status_bar_context = gtk_statusbar_get_context_id(GTK_STATUSBAR (status_bar), "Actual time");

  /* create vbox for tabs and status bar */
  vbox = gtk_vbox_new(FALSE, 0);

  /* add list to window and show it */
  /*
  gtk_container_add (GTK_CONTAINER (window), tabs);
  gtk_container_add (GTK_CONTAINER (window), status_bar);
  */
  gtk_container_add (GTK_CONTAINER (vbox), tabs);
  gtk_container_add (GTK_CONTAINER (vbox), status_bar);

  gtk_box_set_child_packing((GtkBox *)vbox, tabs, TRUE, TRUE, 0, GTK_PACK_START);
  gtk_box_set_child_packing((GtkBox *)vbox, status_bar, FALSE, FALSE, 0, GTK_PACK_END);

  gtk_container_add (GTK_CONTAINER(window), vbox);

  gtk_window_set_default_size((GtkWindow *)window, -1, 300);

  gtk_widget_show (tabs);
  gtk_widget_show (status_bar);
  gtk_widget_show (vbox);
  gtk_widget_show (window);

}

void gtk_interface_done(char *error_str)
{
}

/* locale_to_utf8 conversion with error handling */
gchar *my_locale_to_utf8(char *text, char *file, int line)
{
  /* helper variables for conversion to UTF8 */
  gsize nr,nw;
  GError *utferr = NULL;
  gchar *res;

  res = g_locale_to_utf8(text, -1, &nr, &nw, &utferr);
  if (utferr != NULL) fprintf(stderr, "%s %s:%d\n",
    _("Headers cannot be displayed due to bad conversion from locale to UTF8 in"),
    file, line);

  return res;
}


/* get single host status and display it */
int gtk_show_status(host_data * host) {

  char number_str[11];
  char sr_str[15];
  char avail_str[6];
  int  avail_ratio;
  char lastok_str[15];
  struct tm timeok, timenow, *timetmp;
  struct timeval now_tv;
  GtkListStore *model;
  gtk_models *models;
  GtkTreeIter iter;

  /* This is important and should be in each whatever_show_status function.
   * It allows to display results of substitute host when current host was
   * resolved with the same IP like existing one in the list. Then results
   * of existing one are used instead. This is indicated by like variable in
   * host_data structure and function set_virt_host will set the virtual
   * host to the 'like' one or keep the current with all necessary changes. */
  host_data virt_host, *h;
  
  h = &virt_host;

  set_virt_host(host, h);


  models = lists_models;
  while (models != NULL) {
    if ((models->id) == (h->set_nr)) break;
    else models = models->next;
  }
#ifdef DEBUG
  if (models == NULL) printf(stderr, "%s %d\n", 
      _("Cannot find model with number"), h->set_nr);
#endif
  if (models == NULL) return 1;
  else model = models->model;
    
  /* number_str = string containing number of row */
  sprintf(number_str, "%u", h->id_in_set);

  gtk_tree_model_get_iter_from_string ((GtkTreeModel *)model, &iter, number_str);

  /* number_str = string containing host delay in miliseconds */
  if ((h->delay > MAX_DELAY) || (h->delay < 0)) 
    sprintf(number_str, "%s", my_locale_to_utf8(_("a lot!"), __FILE__, __LINE__));
  else sprintf(number_str, "%4.2f", h->delay);
  strcat(number_str, " ms");
  sprintf(sr_str, "%d/%d", h->nr_sent, h->nr_recv);
  if (h->nr_recv > 0) {
    avail_ratio = (int)100*h->nr_recv/h->nr_sent;
    sprintf(avail_str, "%d %%", avail_ratio);
    if (h->lastok_tv.tv_sec > 0) {
      timetmp = localtime(&(h->lastok_tv.tv_sec));
      memcpy(&timeok, timetmp, sizeof(struct tm));
      get_actual_time(&now_tv);
      timetmp = localtime(&(now_tv.tv_sec));
      memcpy(&timenow, timetmp, sizeof(struct tm));
      /* change 'H:M:S' to 'Month Day H:M' when different day from now or
       * time difference bigger than one day in seconds (86400) */
      if ((timenow.tm_mday != timeok.tm_mday) || 
           ((now_tv.tv_sec - h->lastok_tv.tv_sec) > 86400))
        strftime(lastok_str, sizeof(lastok_str), "%b %d %H:%M", &timeok);
      else    
        sprintf(lastok_str, "%.2d:%.2d:%.2d", timeok.tm_hour, timeok.tm_min, timeok.tm_sec);
    } else strcpy(lastok_str, "-");
  } else {
    sprintf(avail_str, "0 %%");
    strcpy(lastok_str, "-");
  }
  switch (h->status) {
    case ECHO_OK:
      gtk_list_store_set (model, &iter,
        DELAY_COLUMN, number_str,
        STATUS_COLUMN, my_locale_to_utf8(_("Online"), __FILE__, __LINE__),
        SENREC_COLUMN, sr_str,
        AVAIL_COLUMN, avail_str,
        LASTOK_COLUMN, lastok_str,
        COLOR_COLUMN, ONLINE_COLOR,
        -1);
      break;
    case DELAYED:
      gtk_list_store_set (model, &iter,
        DELAY_COLUMN, number_str,
        STATUS_COLUMN, my_locale_to_utf8(_("Delayed"), __FILE__, __LINE__),
        SENREC_COLUMN, sr_str,
        AVAIL_COLUMN, avail_str,
        LASTOK_COLUMN, lastok_str,
        COLOR_COLUMN, LONG_REPLY_COLOR,
        -1);
      break;
    case NO_REPLY:
      gtk_list_store_set (model, &iter,
        DELAY_COLUMN, "---",
        STATUS_COLUMN, my_locale_to_utf8(_("Offline"), __FILE__, __LINE__),
        SENREC_COLUMN, sr_str,
        AVAIL_COLUMN, avail_str,
        LASTOK_COLUMN, lastok_str,
        COLOR_COLUMN, OFFLINE_COLOR,
        -1);
      break;
    case NW_UNREACH:
      gtk_list_store_set (model, &iter,
        DELAY_COLUMN, "---",
        STATUS_COLUMN, my_locale_to_utf8(_("NwUnrch"), __FILE__, __LINE__),
        SENREC_COLUMN, sr_str,
        AVAIL_COLUMN, avail_str,
        LASTOK_COLUMN, lastok_str,
        COLOR_COLUMN, NW_UNREACH_COLOR,
        -1);
      break;
    case UNRESOLVED:
      gtk_list_store_set (model, &iter,
        DELAY_COLUMN, "---",
        STATUS_COLUMN, my_locale_to_utf8(_("Resolving"), __FILE__, __LINE__),
        SENREC_COLUMN, sr_str,
        AVAIL_COLUMN, avail_str,
        LASTOK_COLUMN, lastok_str,
        COLOR_COLUMN, NW_UNREACH_COLOR,
        -1);
      break;
  }

  return 0;
}

/* Create the list of "messages" */
GtkWidget *create_list(hosts_data * hosts, int set_nr, GtkListStore **model)
{
  host_data * host_info;

  GtkWidget *tree_view;
  GtkTreeIter iter;
  GtkCellRenderer *r_left, *r_right, *r_center;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;

  host_info = hosts->host_info;

  /* Create new model (description of column types and total count of columns) */
  *model = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL (*model));
 
  /* fill up the rows with data from host_info and set all offline */
  while (host_info != NULL) {
    if (host_info->set_nr == set_nr) {
      gtk_list_store_insert(*model, &iter, host_info->id_in_set);
      gtk_list_store_set (*model, &iter,
            HOSTNAME_COLUMN, host_info->name,
            HOSTIP_COLUMN, get_ip_str(host_info->addr),
            DELAY_COLUMN, "---",
            STATUS_COLUMN, _("Offline"),
            SENREC_COLUMN, "0/0",
            AVAIL_COLUMN, "0",
            LASTOK_COLUMN, "-",
            COLOR_COLUMN, OFFLINE_COLOR,
                    -1);
    }
    host_info = host_info->next;
  }
  host_info = hosts->host_info;
  
  /* Create cell renderer shared with all cells */
  r_left = gtk_cell_renderer_text_new();
  r_right = gtk_cell_renderer_text_new();
  r_center = gtk_cell_renderer_text_new();
  g_object_set(r_right, "xalign", (gfloat)1.0, NULL);
  g_object_set(r_center, "xalign", (gfloat)0.5, NULL);
  

  /* Set default font family */
#ifdef USE_FONT_FAMILY
  g_object_set(G_OBJECT(r_left), "family", FONT_FAMILY , NULL);
  g_object_set(G_OBJECT(r_right), "family", FONT_FAMILY , NULL);
  g_object_set(G_OBJECT(r_center), "family", FONT_FAMILY , NULL);
#endif

  /* Create columns one by one and set their types */
  column = gtk_tree_view_column_new_with_attributes (my_locale_to_utf8(_("Host"),
                          __FILE__, __LINE__), r_center,
                           "text", HOSTNAME_COLUMN,
                           "foreground", COLOR_COLUMN,
                                                     NULL);
  gtk_tree_view_column_set_alignment(column, 0.5);
  /* They also need to be added to tree_view */
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));
  column = gtk_tree_view_column_new_with_attributes (my_locale_to_utf8(_("Delay [ms]"),
                          __FILE__, __LINE__), r_center,
                           "text", DELAY_COLUMN,
                           "foreground", COLOR_COLUMN,
                                                     NULL);
  gtk_tree_view_column_set_alignment(column, 0.5);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));
  column = gtk_tree_view_column_new_with_attributes (my_locale_to_utf8(_("IP"),
                          __FILE__, __LINE__), r_center,
                           "text", HOSTIP_COLUMN,
                           "foreground", COLOR_COLUMN,
                                                     NULL);
  gtk_tree_view_column_set_alignment(column, 0.5);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));
  column = gtk_tree_view_column_new_with_attributes (my_locale_to_utf8(_("Status"),
                          __FILE__, __LINE__), r_center,
                           "text", STATUS_COLUMN,
                           "foreground", COLOR_COLUMN,
                                                     NULL);
  gtk_tree_view_column_set_alignment(column, 0.5);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));
  column = gtk_tree_view_column_new_with_attributes (my_locale_to_utf8(_("Snt/Rcv"),
                          __FILE__, __LINE__), r_center,
                           "text", SENREC_COLUMN,
                           "foreground", COLOR_COLUMN,
                                                     NULL);
  gtk_tree_view_column_set_alignment(column, 0.5);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));
  column = gtk_tree_view_column_new_with_attributes (my_locale_to_utf8(_("Avail"),
                          __FILE__, __LINE__), r_right,
                           "text", AVAIL_COLUMN,
                           "foreground", COLOR_COLUMN,
                                                     NULL);
  gtk_tree_view_column_set_alignment(column, 0.5);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));
  column = gtk_tree_view_column_new_with_attributes (my_locale_to_utf8(_("Last OK"),
                          __FILE__, __LINE__), r_center,
                           "text", LASTOK_COLUMN,
                           "foreground", COLOR_COLUMN,
                                                     NULL);
  gtk_tree_view_column_set_alignment(column, 0.5);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));

  /* Get the current selection and set it to none. We don't want to be selectable. */
  selection = gtk_tree_view_get_selection((GtkTreeView *)tree_view);
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

  /* Return created widget */
  return tree_view;
}

/* delete_event callback function. data is (hosts_data *) */
gint gtk_delete_event( GtkWidget *widget,
                   GdkEvent  *event,
                   gpointer   data )
{
  if (free_sockets((hosts_data *)data)) 
		fprintf(stderr, "%s\n", _("Unsuccessful exit, canot release sockets."));
  else 
    qprint("%s %s\n", _("Thank you for use."), _("Successful exit."));
  
	gtk_main_quit();

	return FALSE;
}

gboolean display_pinger_status(gpointer data)
{
	struct regular_disp_data *disp_data;
  hosts_data *hosts;
  host_data *host_info;
  char time_str[9];
  char status_bar_text[MAX_STATUS_BAR_TEXT_LEN+1];
  int l;

	disp_data = (struct regular_disp_data *) data;
  hosts = disp_data->hosts;
  host_info = hosts->host_info;
	if (*disp_data->stop_loop) {
		fprintf(stderr, "%s\n", 
				_("Parent is commiting suicide because its only child died unexpectedly."));
		gtk_delete_event(NULL, NULL, (gpointer) hosts);
	}
  
  while (host_info != NULL) {
    /* FIXME handle return value of this */
    gtk_show_status(host_info);
    host_info = host_info->next;
  }

  /* update status bar */
  gtk_statusbar_pop(GTK_STATUSBAR(status_bar), status_bar_context);
  get_currtime_str(NULL, time_str);
  l = strlen(_("Actual time"));
  if (l > (MAX_STATUS_BAR_TEXT_LEN - 2 - 9)) {
    l = MAX_STATUS_BAR_TEXT_LEN - 2 - 9;
    if (l > 3) {
      strncpy(status_bar_text, _("Actual time"), l-3);
      strcat(status_bar_text, ".. ");
    } else strncpy(status_bar_text, _("Actual time"), l);
  } else strcpy(status_bar_text, _("Actual time"));
  strcat(status_bar_text, ": ");
  strcat(status_bar_text, time_str);

  gtk_statusbar_push(GTK_STATUSBAR(status_bar), 
      status_bar_context, my_locale_to_utf8(status_bar_text, __FILE__, __LINE__));

  return TRUE;
}

void gtk_gui_loop(hosts_data *hosts, int *stop_loop)
{
	struct regular_disp_data disp_data;

  int_disp_data = &disp_data;
	disp_data.hosts = hosts;
	disp_data.stop_loop = stop_loop;
	
  disp_data.disp_timeout_id = g_timeout_add( (guint32) hosts->titles->refresh_int, display_pinger_status, 
			(gpointer) &disp_data);
  display_pinger_status((gpointer)&disp_data);

  gtk_main();
}

