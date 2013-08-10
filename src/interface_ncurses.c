#include "interface_ncurses.h"
#include "timefunc.h"
#include "ping.h"
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>
#include <signal.h>

WINDOW *main_win;
int set_nr = 0; /* number of set currently displayed */
int host_displayed_cnt; /* number of hosts displayed in actual set.
                           It is reset before every diplay procedure. */
int win_sizex, win_sizey; /* actual window sizes */
int new_cols, new_rows;


#if defined(SIGWINCH) && defined(TIOCGWINSZ)
#define CAN_RESIZE 1
#else
#define CAN_RESIZE 0
#endif

#if CAN_RESIZE
void adjust_size();
int size_changed = 0;
#endif

/* These are helper function, because distributing another parameter into
 * show_status is difficult. */
int get_curr_set_nr() {
  return set_nr;
}

void set_set_nr(int nr) {
  set_nr = nr;
}

#if CAN_RESIZE
void update_window_sizes(void)
{
  int maxx, maxy;

  getmaxyx(main_win, maxy, maxx);
  win_sizex = maxx;
  win_sizey = maxy;
}
#endif

/* interface init */
void ncurses_interface_init(int *argc, char **argv[], char *title, hosts_data *hosts)
{

#if CAN_RESIZE
  signal(SIGWINCH, adjust_size);
#endif
  main_win = initscr();
  if (has_colors()) {
    start_color();
    init_pair(COL_ONLINE, NC_ONLINE_COLPAIR);
    init_pair(COL_OFFLINE, NC_OFFLINE_COLPAIR);
    init_pair(COL_NWUNREACH, NC_NW_UNREACH_COLPAIR);
    init_pair(COL_DELAYED, NC_LONG_REPLY_COLPAIR);
  }

  print_header(title);
  refresh();

  cbreak();
  keypad(stdscr, TRUE);
  nodelay(main_win, TRUE);
  intrflush(main_win, TRUE);
  timeout(0);

}

void ncurses_interface_done(char *error_str)
{
  endwin();
	if ((error_str != NULL) && (strlen(error_str)))
		fprintf(stderr, "%s\n", error_str);
}


void count_line_pos(void **curseswin, int *host_pos, int *delay_pos, int *name_pos, int *stat_pos, int *sr_pos, int *avail_pos, int *lastok_pos)
{
  int cols;
  int bonus_cols = 0; /* only on terminals with more than MAX_LINE_WIDTH characters */
  int maxx, maxy;

  /* warning: main_win is global */
  getmaxyx(main_win, maxy, maxx);
  *curseswin = main_win;

  if (maxx < MAX_LINE_WIDTH) {
    cols = maxx;
  } else {
    if (maxx > MAX_LINE_WIDTH_NO_BONUS)
      bonus_cols = 1;
    if (maxx > MAX_LINE_WIDTH)
      cols = MAX_LINE_WIDTH;
    else
      cols = maxx;
  }

  *host_pos = HOST_POS_RATIO;
  *delay_pos = cols * DELAY_POS_RATIO;
  *name_pos  = cols * NAME_POS_RATIO;
  *stat_pos  = cols * STAT_POS_RATIO;
  *sr_pos  = cols * SR_POS_RATIO;
  *avail_pos  = cols * AVAIL_POS_RATIO;
  if (bonus_cols) {
     *lastok_pos = cols * LASTOK_POS_RATIO;
  } else {
     *lastok_pos = -1;
  }
}


/* ncurses show single host status */
void show_host_status(host_data * host, int colpair, int attr, char *statstr, 
    char *delaystr, char *srstr, char *availstr, char *lastokstr, int host_set_nr) {
  
  int host_pos, delay_pos, name_pos, stat_pos, sr_pos, avail_pos, lastok_pos, y_pos;
  WINDOW *win;
  void *curseswin;
  char time_str[9];

  if ((host_displayed_cnt < (win_sizey - 3)) && (host->set_nr == host_set_nr)) {
    host_displayed_cnt++;
    count_line_pos(&curseswin, &host_pos, &delay_pos, &name_pos, &stat_pos, &sr_pos, &avail_pos, &lastok_pos);
    win = (WINDOW *)curseswin;
    y_pos = host->id_in_set + NC_STATUS_LINE_FROM;
    move(y_pos, host_pos);
    clrtoeol();
    if (has_colors()) {
      attron(COLOR_PAIR(colpair));
    }
    attron(attr);
    printw("%s", host->name);
    if (has_colors()) 
      attroff(COLOR_PAIR(colpair));

    mvprintw(y_pos, delay_pos, "%s", delaystr);
    mvprintw(y_pos, name_pos, "%s", get_ip_str(host->addr));
    mvprintw(y_pos, stat_pos, "%s", statstr);
    mvprintw(y_pos, sr_pos, "%s", srstr);
    mvprintw(y_pos, avail_pos, "%s", availstr);
    if (lastok_pos > 0) mvprintw(y_pos, lastok_pos, "%s", lastokstr);
    attroff(attr);
    /* print time */
    get_currtime_str(NULL, time_str);
    mvprintw(0, COLS - 8, time_str);
    refresh();
  }
}

/* get single host status and display it */
int ncurses_show_status(host_data *host) {

  char number_str[13];
  char sr_str[15];
  char avail_str[7];
  int  avail_ratio;
  char lastok_str[15];
  struct tm timeok, timenow, *timetmp;
  struct timeval now_tv;

  /* This is important and should be in each whatever_show_status function.
   * It allows to display results of substitute host when current host was
   * resolved with the same IP like existing one in the list. Then results
   * of existing one are used instead. This is indicated by like variable in
   * host_data structure and function set_virt_host will set the virtual
   * host to the 'like' one or keep the current with all necessary changes. */
  host_data virt_host, *h;
  
  h = &virt_host;

  set_virt_host(host, h);
    
  /* number_str = string containing host delay in miliseconds */
  if ((h->delay > MAX_DELAY) || (h->delay < 0)) sprintf(number_str, "%s", _("a lot!"));
  else snprintf(number_str, sizeof(number_str) - 1, "%4.2f", h->delay);
  number_str[sizeof(number_str) - 1] = 0;
  if ((strlen(number_str) + 3) < ( sizeof(number_str) - 1))
    strcat(number_str, " ms");
  sprintf(sr_str, "%d/%d", h->nr_sent, h->nr_recv);
  if (h->nr_recv > 0) {
    avail_ratio = (int)100*h->nr_recv/h->nr_sent;
    if (avail_ratio < 10)
      snprintf(avail_str, sizeof(avail_str) - 1, "  %d %%", avail_ratio);
    else if (avail_ratio < 100)
      snprintf(avail_str, sizeof(avail_str) - 1, " %d %%", avail_ratio);
    else 
      snprintf(avail_str, sizeof(avail_str) - 1, "%d %%", avail_ratio);
    avail_str[sizeof(avail_str) - 1] = 0;
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
    if (mode == NCURSES) sprintf(avail_str, "  0 %%");
    strcpy(lastok_str, "-");
  }

  switch (h->status) {
    case ECHO_OK:
      if (mode == NCURSES) show_host_status(h, COL_ONLINE, A_BOLD, "Online", number_str, sr_str, avail_str, lastok_str, get_curr_set_nr());
      break;
    case DELAYED:
      if (mode == NCURSES) show_host_status(h, COL_DELAYED, A_BOLD, "Online", number_str, sr_str, avail_str, lastok_str, get_curr_set_nr());
      break;
    case NO_REPLY:
      if (mode == NCURSES) show_host_status(h, COL_OFFLINE, A_NORMAL, "Offline", number_str, sr_str, avail_str, lastok_str, get_curr_set_nr());
      break;
    case NW_UNREACH:
      if (mode == NCURSES) show_host_status(h, COL_NWUNREACH, A_NORMAL, _("NwUnrch"), number_str,
          sr_str, avail_str, lastok_str, get_curr_set_nr());
      break;
    case UNRESOLVED:
      if (mode == NCURSES) show_host_status(h, COL_NWUNREACH, A_NORMAL, _("Resolving"), number_str,
          sr_str, avail_str, lastok_str, get_curr_set_nr());
      break;
  }

  return 0;
}

void print_header(char *title) {
  char *line = NULL;
  int host_pos, delay_pos, name_pos, stat_pos, sr_pos, avail_pos, lastok_pos;
  int maxx, maxy;
  WINDOW *win;
  void *curseswin;
  int title_start, title_maxlen, title_len;
  char *title_with_brackets = NULL;
  int idx;    /* index for replacing disallowed character '%' in title */
  
  count_line_pos(&curseswin, &host_pos, &delay_pos, &name_pos, &stat_pos, &sr_pos, &avail_pos, &lastok_pos);
  win = (WINDOW *)curseswin;
  move(0,0);
  getmaxyx(win, maxy, maxx);
  line = malloc(maxx+1);
  memset(line, ' ', maxx);
  *(line+maxx) = 0;
  attron(A_BOLD | A_REVERSE);
  printw("%s", line);
  move(0,HDR_LINE_X_START);
  sprintf(line, "Pinger v%s", VERSION);
  printw(line);
  title_start = strlen(line) + HDR_LINE_X_START + 1;
  sprintf(line, _("'q' key to quit"));
  move(0, COLS - strlen(line) - 9); /* 1 + clock width */
  printw(line);
  title_maxlen = COLS - strlen(line) - 9 - title_start;
  if ((title_maxlen > 4) && (title != NULL)) {
    title_len = strlen(title);
    if ((title_len + 2) > title_maxlen) { 
      title_with_brackets = malloc(title_maxlen + 1);
      strcpy(title_with_brackets, "[");
      strncat(title_with_brackets, title, title_maxlen - 4);
      strcat(title_with_brackets, "..]");
    } else {
      title_with_brackets = malloc(title_len + 3);
      strcpy(title_with_brackets, "[");
      strncat(title_with_brackets, title, title_len);
      strcat(title_with_brackets, "]");
    }
    move(0, title_start);
    for (idx = 0; idx < strlen(title_with_brackets); idx++)
      if (title_with_brackets[idx] == '%')
        title_with_brackets[idx] = ':';
    printw(title_with_brackets);
  }
//if (title != NULL) free(title);
  if (title_with_brackets != NULL) free(title_with_brackets);
  attroff(A_BOLD | A_REVERSE);
  attron(A_BOLD);
  move(0, COLS - 8);
  get_currtime_str(NULL, line);
  printw(line);
  free(line);
  
  move(1, 0);
  clrtoeol();
  mvprintw(1, host_pos, _("Host"));
  mvprintw(1, delay_pos, _("Delay [ms]"));
  mvprintw(1, name_pos, _("IP"));
  mvprintw(1, stat_pos, _("Status"));
  mvprintw(1, sr_pos, _("Snt/Rcv"));
  mvprintw(1, avail_pos, _("Avail"));
  if (lastok_pos > 0) mvprintw(1, lastok_pos, _("Last OK"));
  attroff(A_BOLD);
  refresh();
}
 
void clear_under_header(void) {
  move(LINE_UNDER_HEADER, 0);
  clrtoeol();
}

void clear_status_win(hosts_data *hosts)
{
  host_data *host_info;

  host_info = hosts->host_info;
  
  /* Clear the status */
  move(NC_STATUS_LINE_FROM, 0);
  clrtobot();

  while (host_info != NULL) {
    ncurses_show_status(host_info); 
    host_info = host_info->next;
  }
  refresh();
}

/* gets gui refresh interval for the given host set */
int get_set_refreshint(int set_nr, titlelist *tlist) {
  int refresh_int;
  titlelist *tl = tlist;
  
  while (tl != NULL) {
    if (tl->nr == set_nr) break;
    tl = tl->next;
  }
  if (tl == NULL) refresh_int = DEFAULT_REFRESH_INTERVAL;
  else refresh_int = tl->refresh_int;

  return refresh_int;
}

/* print ncurses header according to current set_nr and return true if set found
 * or false if not found and used the last in the row */
int update_header(hosts_data* hosts)
{
  int set_nr;
  titlelist *titles;
  
  set_nr = get_curr_set_nr();
  titles = hosts->titles;
  while ((titles->next != NULL) && (titles->nr != set_nr)) titles = titles->next;
  print_header(titles->title);
  if (titles->nr != set_nr) return FALSE;
  else return TRUE;
}

/* If window size has beed changed, this functions
 * adjusts the screen data to reflect this change */
void win_resize_update(hosts_data *hosts)
{
  resize_term(new_rows, new_cols);
  clearok(main_win, TRUE);

  update_header(hosts);
  clear_under_header();
  clear_status_win(hosts);
  size_changed = 0;
  update_window_sizes();
}

void ncurses_gui_loop(hosts_data *hosts, int *stop_loop) 
{
  struct timeval cur_timeout;
  int selected;
  int key;  /* ncurses key */
  int set_nr;
  int refresh_int;
  fd_set fdset;
  host_data *host_info;
  titlelist *tlist;

  host_info = hosts->host_info;
  tlist = hosts->titles;

  win_sizex = COLS;
  win_sizey = LINES;
  
  set_nr = get_curr_set_nr();
  refresh_int = get_set_refreshint(set_nr, tlist);

  /* gui main loop */
  while (!*stop_loop) {
    /* show actual status */
    host_displayed_cnt = 0;
    while (host_info != NULL) {
      ncurses_show_status(host_info);
      host_info = host_info->next;
    }
    host_info = hosts->host_info;
    
    /* refresh interval default or from config file */
    ms_to_tv(refresh_int, &cur_timeout);
    FD_SET(0, &fdset);

    if ((selected = select(1, &fdset, NULL, NULL, &cur_timeout)) < 0 ) {
      if (errno == EINTR) {
        /* WINCH interrupt called */
#if CAN_RESIZE
        if (size_changed != 0)
          win_resize_update(hosts);
#endif
      } else {
        fprintf(stderr, "%s %s %d\n", _("Cannot select:"), strerror(errno), errno);
        exit(1);
      }
    }

    /* key handler */
    if (FD_ISSET(0, &fdset)) {
      key = getch();
      if (key == ERR) {
        if (errno != EINTR) break;
      } else if (key == KEY_NPAGE) {
        set_set_nr(get_curr_set_nr() + 1);
        refresh_int = get_set_refreshint(get_curr_set_nr(), tlist);
        if (update_header(hosts) == FALSE) set_set_nr(get_curr_set_nr() - 1);
        clear_status_win(hosts);
      } else if (key == KEY_PPAGE) {
        if (get_curr_set_nr() > 0) {
          set_set_nr(get_curr_set_nr() - 1);
          refresh_int = get_set_refreshint(get_curr_set_nr(), tlist);
        }
        update_header(hosts);
        clear_status_win(hosts);
      } else if (key == (int)'q') {
        break;
      }
    } 

  }
}

/* Resize terminal if necessary */
#if CAN_RESIZE
void adjust_size()
{
    struct winsize size;

    if ((!size_changed) && 
        (ioctl(fileno(stdout), TIOCGWINSZ, &size) == 0)) {
        new_cols = (size.ws_col < MIN_LINE_WIDTH) ? MIN_LINE_WIDTH : size.ws_col;
        new_rows = size.ws_row;
        size_changed = 1;
    } 
}
#endif /* CAN_RESIZE */
  

