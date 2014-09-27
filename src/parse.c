/*
    pinger -- GTK+/ncurses multi-ping utility
    Copyright (C) 2002 Petr Koloros

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#define DNS_LOCAL
#include "dns.h"
#include "log.h"
#include "parse.h"
#include "ping.h"

/* Parse one line of config file ("name = value" format)
 * and store values into "name" and "value" */
int parse_line(char *line, char *name, char *value)
{
    char *value_part;
    int value_part_index;

    value_part = strchr(line, (int) '=');
    if (value_part == NULL) {
        log_event(NULL, err_log, -1, "%s: %s", _("Incomplete line"), line);
        return -1;
    }
    strcpy(value, 1 + value_part);
    value_part_index = strlen(line) - strlen(value) - 1;
    if (value_part_index >= 1) {
        strncpy(name, line, (size_t) value_part_index);
        name[value_part_index] = '\0';
    } else
        value[0] = '\0';

    if (value[strlen(value) - 1] == '\n')
        value[strlen(value) - 1] = '\0';
    return 0;
}

/* Check if byte (in string form) is real byte value (0-255) */
int check_byte(char *byte)
{
    int byte_val;

    byte_val = atoi(byte);
    if ((byte_val < 0) || (byte_val > 255))
        return 1;
    else
        return 0;
}

/* Check if valid option value */
int check_opt_value(char *value)
{
    int intval;

    intval = atoi(value);
    if ((intval < 0) || (intval > MAX_OPT_VALUE))
        return 1;
    else
        return 0;

}

int valid_dname(char *name)
{
    int cnt;
    int found_dot;
    char ch;
    int a;

    found_dot = 0;
    if ((name == NULL) || (name[0] == 0))
        return 1;

    for (cnt = 0; cnt < strlen(name); cnt++) {
        ch = name[cnt];
        if (ch == '.') {
            found_dot++;
            continue;
        }

        /* 0-9, A-Z, a-z */
        if (!(((ch >= '0') && (ch <= '9')) || ((ch >= 'a') && (ch <= 'z')) ||
              ((ch >= 'A') && (ch <= 'Z')))) {
            /* different character from above. Check for extra allowed chars */
            for (a = 0;
                 (a < sizeof(dns_allowed_chars))
                 && (ch != dns_allowed_chars[a]); a++);
            /* if not alpha-num or allowed, return error */
            if (a == sizeof(dns_allowed_chars))
                return 1;
        }
    }

    /* ban non-alphanum chars at the begining or the end */
    for (a = 0; a < sizeof(dns_allowed_chars); a++)
        if ((name[0] == dns_allowed_chars[a]) ||
            (name[strlen(name) - 1] == dns_allowed_chars[a]))
            return 1;

    /* ban digits at the begining */
    if ((name[0] >= '0') && (name[0] <= '9'))
        return 1;

    return 0;
}

/* Check if IP ("XXX.XXX.XXX.XXX" in string form) is real IPv4 IP address */
int valid_ip(char *ip)
{

    int len, exit_now, cnt, dot_cnt, byte_cnt;
    char byte[BYTE_STR_LEN];

    byte[0] = '\0';
    byte_cnt = 0;
    if ((len = strlen(ip)) <= 15) {
        for (cnt = 0, dot_cnt = 0, byte_cnt = 0, exit_now = 0; cnt < len; cnt++) {
            if (ip[cnt] == '.') {
                if (byte_cnt == 0)
                    exit_now = 1;
                else {
                    byte[byte_cnt] = '\0';
                    if (check_byte(byte))
                        exit_now = 1;
                    byte_cnt = 0;
                }
                dot_cnt++;
            } else if (((int) ip[cnt] < (int) '0') ||
                       ((int) ip[cnt] > (int) '9'))
                exit_now = 1;
            else {
                byte[byte_cnt] = ip[cnt];
                if (++byte_cnt == 4)
                    exit_now = 1;
            }
            if (exit_now)
                cnt = len;
        }
        if (dot_cnt != 3)
            exit_now = 1;
    } else
        exit_now = 1;
    /* check last part of IP */
    if (byte_cnt < 4) {
        byte[byte_cnt] = '\0';
        if (check_byte(byte))
            exit_now = 1;
    }
    /* if exit_now evaluated, exit ! */
    if (exit_now)
        return 1;
    else
        return 0;
}

/* Strips spaces and tabs from the string */
char *strip_spaces(char *str)
{
    int r_ptr, w_ptr;
    char ch;

    if (str == NULL)
        return str;
    r_ptr = w_ptr = 0;
    while ((int) *(str + r_ptr) != 0) {
        ch = *(str + r_ptr);
        if (!((ch == ' ') || ((int) ch == 9))) {
            *(str + w_ptr) = *(str + r_ptr);
            w_ptr++;
        }
        r_ptr++;
    }
    *(str + w_ptr) = 0;
    return str;
}

int is_num(char *s)
{
    int l;

    if (s == NULL)
        return -1;
    l = strlen(s);
    while (--l >= 0)
        if (((s[l] < 48) || (s[l] > 57)) && (s[l] != ' ') && (s[l] != 9))
            return 0;
    return 1;
}

/* parse value, which should be in this format:
 * hostname,arp,log,pause_between_OK_pings,pause_between_NOREPLY_pings,pause_of_long_delay
 * It work also with only one (first) parameter (hostname). */
int parse_value(char *value, char *name, int *ok_delay, int *noreply_delay,
                int *long_delay, int *arp, int *log)
{
    char *tmp;
    char *text_param = NULL;
    static struct
    {
        int *int_ptr;
        int min_delay;
        int max_delay;
    } num_par[] = {
        {
        NULL, MIN_DELAY, MAX_DELAY}, {
        NULL, MIN_DELAY, MAX_DELAY}, {
        NULL, MIN_DELAY, MAX_DELAY}, {
    NULL}};
    int intval, nr;

    num_par[0].int_ptr = ok_delay;
    num_par[1].int_ptr = noreply_delay;
    num_par[2].int_ptr = long_delay;

    tmp = strtok(value, ",");
    if (tmp == NULL) {
        name = NULL;
        return 1;
    } else
        strcpy(name, tmp);

    *arp = 0;
    *log = LOG_DEFAULT;         /* Default means - depend on others, if some are set to enabled or disabled.
                                   It is evaluated later after parsing. */

    while (tmp != NULL) {
        tmp = strtok(NULL, ",");
        if (tmp == NULL)
            break;
        text_param = realloc(text_param, strlen(tmp) + 1);
        strcpy(text_param, tmp);
        strip_spaces(text_param);
        if (strcmp(text_param, "arp") == 0) {
            *arp = 1;
        } else if (strcmp(text_param, "nolog") == 0) {
            *log = LOG_DISABLED;
        } else if (strcmp(text_param, "log") == 0) {
            *log = LOG_ENABLED;
        } else
            break;
    }

    if (text_param != NULL) {
        free(text_param);
        text_param = NULL;
    }

    nr = 0;
    while ((tmp != NULL) && (num_par[nr].int_ptr != NULL)) {
        if (is_num(tmp) == 1) {
            intval = atoi(tmp);
            if (intval < num_par[nr].min_delay) {
                qprint("%s\n",
                       _("Value is lower than limit. Setting to minimum value."));
                intval = num_par[nr].min_delay;
            }
            if (intval > num_par[nr].max_delay) {
                qprint("%s\n",
                       _("Value is higher than limit. Setting to maximum value."));
                intval = num_par[nr].max_delay;
            }
            *num_par[nr++].int_ptr = intval;
        }
        tmp = strtok(NULL, ",");
    }

    return 0;
}

/* Read config file and fill up host structure with these data.
 * Also open raw sockets for the hosts. */
void read_config_file(hosts_data * hosts, filelist * cfg_filenames)
{

    char *homestr;
    int saved_euid;
    char cfg_path[512];
    FILE *cfg_file;
    char line[MAX_CFGLINE_LEN + 1];
    char long_line[LONG_CFGLINE_LEN + 1];   /* Line reading comes here first to decide if line too long or not */
    char name[MAX_CFGLINE_LEN + 1];
    char value[MAX_CFGLINE_LEN + 1];
    char val_name[MAX_CFGLINE_LEN + 1];
    int ok_delay, set_ok_delay;
    int noreply_delay, set_noreply_delay;
    int long_delay, set_long_delay;
    int set_refresh_interval;
    int use_arp, log;
    int host_max;
    int section;
    int int_value;
    int end_parsing = 0;
    int log_if_status_last;
    filelist *cfgfilesidx;
    int cfg_file_opened = 0;
    host_data *host_info, *host_ptr, *host_start;
    titlelist *tlist;
    int tlistcnt = 0;
    int std_title_cnt = 1;
    int title_set;
    int set_nr = 0;
    int id_in_set = 0;
    int log_ena, log_dis, log_def;
    int log_error;
    int host_ininitalized_curr_file = 0;
    int stdcntlen;
    char *recent_title = NULL;
    char tmpstr[MIN_TITLE_LEN]; /* temporary string for 2 digits, used for std title */
    int name_type;

    enum sections
    {
        INIT,
        OPTIONS,
        HOSTS
    };

    enum name_types
    {
        IP,
        DNAME
    };

    /* prevent root operations unless neccessary */
    saved_euid = geteuid();
    seteuid(getuid());

    host_max = hosts->host_max;
    cfgfilesidx = cfg_filenames;
    host_info = hosts->host_info;
    host_start = host_info;

    if (cfg_filenames == NULL) {
        homestr = getenv("HOME");
        if (!homestr) {
            qprint("%s 'HOME'.\n", _("Cannot get environment variable"));
            exit(1);
        }
        strcpy(cfg_path, homestr);
        strcat(cfg_path, "/");
        strcat(cfg_path, CFG_FILENAME);
    } else
        strncpy(cfg_path, cfgfilesidx->filename, sizeof(cfg_path) - 1);
    while (!end_parsing) {
        /* set all variables to the default values - each set should have the same start condition */

        log_if_status_last = DEFAULT_STATUS_LAST;
        set_ok_delay = DEFAULT_DELAY_BW_PINGS;
        set_long_delay = DEFAULT_LONG_DELAY;
        set_noreply_delay = DEFAULT_HOST_TIMEOUT;
        set_refresh_interval = DEFAULT_REFRESH_INTERVAL;

        qprint("%s %s\n", _("Reading configuration file"), cfg_path);
        host_ininitalized_curr_file = 0;
        cfg_file_opened = 0;
        cfg_file = fopen(cfg_path, "r");
        if (!cfg_file) {
            qprint("%s (%s)!\n", _("Cannot open config file"), cfg_path);
            if (cfgfilesidx != NULL) {
                cfgfilesidx = cfgfilesidx->next;
                if (cfgfilesidx == NULL)
                    end_parsing = 1;
                else
                    strncpy(cfg_path, cfgfilesidx->filename,
                            sizeof(cfg_path) - 1);
                break;
            }
            qprint("--\n%s\n",
                   _("Creating new one in your home directory ..."));
            cfg_file = fopen(cfg_path, "w");
            if (!cfg_file) {
                qprint("%s. %s\n", _("Cannot create file"),
                       _("Please check your create privileges for this directory!"));
                exit(1);
            } else {
                fprintf(cfg_file, "# %s\n", _("Pinger configuration file"));
                fprintf(cfg_file,
                        "# <IP or domain name>=<%s>[,arp][,[no]log][,ok_delay,noreply_delay,long_delay]\n",
                        _("Description"));
                fprintf(cfg_file, "#\n# ok_delay - %s\n",
                        _("when ping successful, wait this time before sending next ping request."));
                fprintf(cfg_file, "# noreply_delay - %s\n",
                        _("when ping failed, wait this time before sending next ping request."));
                fprintf(cfg_file, "#               - %s\n",
                        _("Also when reply exceeds this interval, host is marked as offline."));
                fprintf(cfg_file, "# long_delay - %s\n",
                        _("delay of reply after which is host marked as delayed."));
                fprintf(cfg_file, "# arp - %s\n",
                        _("Indicate that host will be pinged using ARP request instead of ICMP ECHO."));
                fprintf(cfg_file, "# log - %s\n",
                        _("Indicate that host will be logged in the host log."));
                fprintf(cfg_file, "# nolog - %s\n",
                        _("Indicate that host will not be logged in the host log."));
                fprintf(cfg_file, "\n# %s\n",
                        _("Logging - if host log specified, every hosts is logged."));
                fprintf(cfg_file, "#           %s\n",
                        _("If any of them is marked as nolog, it won't be logged."));
                fprintf(cfg_file, "#           %s\n",
                        _("If there are only 'log' at some hosts, these will be logged"));
                fprintf(cfg_file, "#           %s\n",
                        _("while the rest won't. See README for more."));

                fprintf(cfg_file, "\nOPTIONS SECTION:\n");
                fprintf(cfg_file, "\n# %s\n", _("Display refresh interval."));
                fprintf(cfg_file, "#refresh_interval_ms=1000\n");
                fprintf(cfg_file, "\n# %s\n",
                        _("Successful pings' next request interval."));
                fprintf(cfg_file, "#delay_between_pings_ms=3000\n");
                fprintf(cfg_file, "\n# %s\n",
                        _("Interval, in which if there is no reply,"));
                fprintf(cfg_file, "# %s\n",
                        _("host is marked as offline and new request is sent."));
                fprintf(cfg_file, "#host_timeout_ms=10000\n");
                fprintf(cfg_file, "\n# %s\n",
                        _("Delay of reply after which is host marked as delayed, if not specified in IP line."));
                fprintf(cfg_file, "#long_delay_ms=5000\n");
                fprintf(cfg_file, "\n# %s\n",
                        _("Hosts specified only by domain names are resolved every 10 minutes."));
                fprintf(cfg_file, "# %s\n",
                        _("Specify other interval in seconds here if required. -1 = disable."));
                fprintf(cfg_file, "#dns_check_s=600\n");
                fprintf(cfg_file, "\n# %s\n",
                        _("Filename where to log errors."));
                fprintf(cfg_file, "#err_log=filename\n");
                fprintf(cfg_file, "\n# %s\n",
                        _("Filename where to log host status changes."));
                fprintf(cfg_file, "#host_log=filename\n");
                fprintf(cfg_file, "\n# %s\n",
                        _("Minimum duration of the host status to log the change (in seconds)."));
                fprintf(cfg_file, "# %s\n",
                        _("-1 = disable logging, 0 = log all changes."));
                fprintf(cfg_file, "#log_if_status_last_s=5\n");
                fprintf(cfg_file, "\n# %s\n",
                        _("ICMP packet size (see 'man ping' for details)."));
                fprintf(cfg_file, "#icmp_packet_size=0\n");
                fprintf(cfg_file, "\n# %s %% %s\n",
                        _("Title of this set of hosts. Character"),
                        _("is not allowed here!"));
                fprintf(cfg_file, "title=standard set\n");
                fprintf(cfg_file, "\nHOSTS SECTION:\n");
                fprintf(cfg_file, "212.80.76.18=seznam\n");
                fprintf(cfg_file, "66.249.93.99=google,3000,10000,2000\n");
                fprintf(cfg_file, "#123.45.67.89=somehost,arp,nolog,3000\n");
                fprintf(cfg_file, "www.gnu.org=GNU web\n");
                fprintf(cfg_file, "147.32.119.96=MyBox\n");
                fclose(cfg_file);
                if (chown(cfg_path, getuid(), getgid()))
                    qprint("%s %s\n",
                           _("Cannot set configuration file owner to current user:"),
                           strerror(errno));
                cfg_file = fopen(cfg_path, "r");
                if (!cfg_file) {
                    qprint("%s\n",
                           _("Cannot reopen just created config file!"));
                    exit(1);
                }
                qprint("%s (%s)\n", _("Config file created successfuly"),
                       cfg_path);
            }
        } else
            qprint("%s %s\n", _("Using file"), cfg_path);
        cfg_file_opened = 1;

        /* read from cfg file */
        title_set = 0;
        while ((fgets(long_line, LONG_CFGLINE_LEN, cfg_file) != NULL)
               && (host_max < MAX_HOSTS)) {
            if (strlen(long_line) > MAX_CFGLINE_LEN) {
                strncpy(line, long_line, MAX_CFGLINE_LEN);
                line[MAX_CFGLINE_LEN] = 0;
                while (strlen(long_line) == LONG_CFGLINE_LEN) {
                    qprint("this line is too long %s", long_line);
                    fgets(long_line, LONG_CFGLINE_LEN, cfg_file);
                }
            } else {
                strncpy(line, long_line, MAX_CFGLINE_LEN);
                line[MAX_CFGLINE_LEN] = 0;
            }
            if ((!(line[0] == '\n')) && (!(line[0] == '#'))) {
                /* check SECTION */
                if (strcmp("OPTIONS SECTION:\n", line) == 0) {
                    qprint("### %s\n", _("found options section"));
                    section = OPTIONS;
                } else if (strcmp("HOSTS SECTION:\n", line) == 0) {
                    qprint("### %s\n", _("found hosts section"));
                    section = HOSTS;
                } else {
                    if (section == INIT) {
                        qprint("%s\n",
                               _("Something uncommented found before OPTIONS section!"));
                        fclose(cfg_file);
                        exit(1);
                    }
                    /* Parse one line from config file */
                    if (parse_line(line, name, value) == -1) {
                        fclose(cfg_file);
                        exit(1);
                    }

                    if (section == HOSTS) {
                        /* HOST SECTION */

                        qprint("%s: %s, %s\n", _("IP/domain name"), name,
                               value);
                        if (valid_ip(name)) {
                            if (valid_dname(name)) {
                                log_event(NULL, err_log, -1, "%s: %s\n",
                                          _("Invalid IP/domain name"), name);
                                exit(1);
                            } else {
                                if (strlen(name) > MAX_DNAME_LEN) {
                                    log_event(NULL, err_log, -1,
                                              _("Domain name is too long! Update this program source or check if domain name is OK!"));
                                    exit(1);
                                }
                                name_type = DNAME;
                            }
                        } else
                            name_type = IP;

                        /* find duplicites */
                        host_ptr = hosts->host_info;
                        if (host_ptr != NULL) {
                            /* if unresolved ip, check for duplicities in names */
                            if (name_type == DNAME) {
                                while (host_ptr != NULL) {
                                    if (strcmp(host_ptr->domain_name, name) ==
                                        0)
                                        break;
                                    host_ptr = host_ptr->next;
                                }
                            } else {
                                /* else check IPs */
                                while (host_ptr != NULL) {
                                    if (strcmp(inet_ntoa(host_ptr->addr), name)
                                        == 0)
                                        break;
                                    host_ptr = host_ptr->next;
                                }
                            }
                        }

                        /* if duplicities found, skip to another record */
                        if (host_ptr != NULL) {
                            /* qprint has condition inside. It must be braced if used in another "if"! */
                            qprint("%s %s\n", name,
                                   _("is already initialized."));
                        } else {

                            /* add new host */
                            if (host_info == NULL) {
                                host_info = malloc(sizeof(host_data));
                                hosts->host_info = host_info;
                            } else {
                                host_info->next = malloc(sizeof(host_data));
                                host_info = host_info->next;
                            }
                            host_info->next = NULL;

                            /* REMEMBER, cfg_file is in NAME = VALUE format. And when IP is
                             * specified, it is: XXX.XXX.XXX.XXX = www.something.com. So function
                             * parse_line returns variable "name" containing IP and variable
                             * "value" containing NAME of host ! */

                            if (name_type == DNAME) {
                                /* place special IP into name -> identifies unresolved IP and it will
                                 * be resolved later */
                                strcpy(host_info->domain_name, name);
                                strcpy(name, UNRESOLVED_HOST);
                            } else
                                strcpy(host_info->domain_name, "");
                            /* add address into struct */
                            if (inet_aton(name, &(host_info->addr)) == 0) {
                                log_event(NULL, err_log, -1, "%s %s!",
                                          _("Invalid host"), host_info->name);
                                exit(1);
                            }


                            /* parse line arguments and set it to the host */
                            ok_delay = set_ok_delay;
                            noreply_delay = set_noreply_delay;
                            long_delay = set_long_delay;
                            if (parse_value
                                (value, val_name, &ok_delay, &noreply_delay,
                                 &long_delay, &use_arp, &log))
                                qprint("Cannot parse line\n");

                            if (val_name != NULL)
                                qprint("%s: %s\n", _("Found name"), val_name);
                            strcpy(host_info->name, val_name);
                            if (ok_delay != 0) {
                                qprint("%s: %d ms\n", _("Setting ok delay"),
                                       ok_delay);
                                host_info->ok_delay = ok_delay;
                            }
                            if (noreply_delay != 0) {
                                qprint("%s: %d ms\n",
                                       _("Setting noreply delay"),
                                       noreply_delay);
                                host_info->noreply_delay = noreply_delay;
                            }
                            if (long_delay != 0) {
                                qprint("%s: %d ms\n", _("Setting long delay"),
                                       long_delay);
                                host_info->long_delay = long_delay;
                            }
                            if (use_arp == 1) {
                                qprint("%s\n",
                                       _("This host will use ARP request instead of ICMP ECHO"));
                                host_info->arp = 1;
                            } else
                                host_info->arp = 0;
                            if ((log != LOG_DEFAULT) && (host_log->log != NULL)) {
                                if (log == LOG_DISABLED) {
                                    qprint("%s\n",
                                           _("This host will not be logged in host log"));
                                    host_info->log_if_status_last =
                                        LOG_DISABLED;
                                } else {
                                    qprint("%s\n",
                                           _("This host will be logged in host log"));
                                    host_info->log_if_status_last =
                                        log_if_status_last;
                                }
                            } else
                                host_info->log_if_status_last = LOG_DEFAULT;


                            qprint("%s %s - %s\n", _("Adding host"), name,
                                   host_info->name);

                            /* zero packet sequence numbers */
                            host_info->last_seq_sent = 0;
                            host_info->last_seq_recv = 0;
                            /* zero sent/recv */
                            host_info->nr_sent = 0;
                            host_info->nr_recv = 0;
                            /* zero last ok time */
                            host_info->lastok_tv.tv_sec = 0;
                            host_info->lastok_tv.tv_usec = 0;

                            /* reset delay times */
                            host_info->delay = 0;

                            /* reset host status */
                            host_info->status = NO_REPLY;

                            /* reset like parameter */
                            host_info->like = NULL;

                            /* assign current set_nr to the host */
                            host_info->set_nr = set_nr;
                            host_info->id_in_set = id_in_set++;

                            host_info->id = host_max;

                            /* create socket */

                            /* enable root operations */
                            seteuid(saved_euid);
#ifdef ENABLE_ARP
                            if (host_info->arp)
                                host_info->rawfd =
                                    socket(PF_PACKET, SOCK_RAW,
                                           htons(ETH_P_ARP));
                            else
#endif
                                (host_info->rawfd =
                                 socket(PF_INET, SOCK_RAW, IPPROTO_ICMP));

                            /* restore users rights */
                            seteuid(getuid());

                            if (host_info->rawfd < 0) {
                                log_event(NULL, err_log, -1, "%s: %s\n%s",
                                          _("Initializing socket"),
                                          strerror(errno),
                                          _("Make sure you have root privileges or suid bit set!"));
                                exit(1);
                            }

                            if (host_info->rawfd <= 2) {
                                log_event(NULL, err_log, -1,
                                          _("Got free socket less than 2!"));
                                exit(1);
                            }

                            host_max++;
                            host_ininitalized_curr_file++;
                        }
                    } else if (section == OPTIONS) {
                        qprint("%s: %s, %s: %s\n", _("Name"), name, _("value"),
                               value);
                        /* Process options */
                        if ((!check_opt_value(value))
                            || (strcmp(value, "-1") == 0)) {
                            int_value = atoi(value);

                            /* options are processed here */
                            if (!strcmp(name, "dns_check_s")) {
                                if ((int_value != -1) &&
                                    (int_value >= DNS_CHECK_MIN_S)
                                    && (int_value <= DNS_CHECK_MAX_S)) {
                                    dns_check_s = int_value;
                                    qprint("%s: %s ms\n",
                                           _("Setting dns check to new value"),
                                           value);
                                } else {
                                    log_event(NULL, err_log, -1,
                                              "dns_check_s %s (%d-%d). %s %d.",
                                              _("is out of range"),
                                              DNS_CHECK_MIN_S, DNS_CHECK_MAX_S,
                                              _("Setting default value of"),
                                              dns_check_s);
                                }
                            } else if (!strcmp(name, "log_if_status_last_s")) {
                                if ((int_value < MIN_STATUS_LAST)
                                    || (int_value > MAX_STATUS_LAST)) {
                                    log_event(NULL, err_log, -1,
                                              "log_if_status_last_s %s (%d-%d). %s %d.",
                                              _("is out of range"),
                                              MIN_STATUS_LAST, MAX_STATUS_LAST,
                                              _("Setting default value of"),
                                              DEFAULT_STATUS_LAST);
                                    int_value = DEFAULT_STATUS_LAST;
                                }
                                if (int_value == 0)
                                    qprint("%s\n",
                                           _("Setting logging to log every host status change"));
                                if (int_value == -1)
                                    qprint("%s\n",
                                           _("Disabling host status logging"));
                                log_if_status_last = int_value;
                                qprint("%s: %d s\n",
                                       _("Setting logging when host status last longer than value of (seconds)"),
                                       int_value);
                            } else if (!strcmp(name, "icmp_packet_size")) {
                                if ((int_value < MIN_ICMP_DATA_LEN)
                                    || (int_value > MAX_ICMP_DATA_LEN)) {
                                    log_event(NULL, err_log, -1,
                                              "icmp_packet_size %s (%d-%d). %s %d.",
                                              _("is out of range"),
                                              MIN_ICMP_DATA_LEN,
                                              MAX_ICMP_DATA_LEN,
                                              _("Setting default value of"),
                                              DEFAULT_ICMP_DATA_LEN);
                                    int_value = DEFAULT_ICMP_DATA_LEN;
                                }
                                qprint("%s: %d\n",
                                       _("Setting ICMP packet size to value"),
                                       int_value);
                                hosts->icmp_data_len = int_value;

                            } else if (int_value == -1) {
                                qprint("%s\n",
                                       _("Value -1 is not allowed here"));
                            } else if (!strcmp(name, "refresh_interval_ms")) {
                                set_refresh_interval = int_value;
                                qprint("%s: %d ms\n",
                                       _("Setting refresh interval to new value"),
                                       int_value);
                            } else if (!strcmp(name, "host_timeout_ms")) {
                                set_noreply_delay = int_value;
                                qprint("%s: %d ms\n",
                                       _("Setting noreply interval to new value"),
                                       int_value);
                            } else if (!strcmp(name, "delay_between_pings_ms")) {
                                set_ok_delay = int_value;
                                qprint("%s: %d ms\n",
                                       _("Setting delay between pings to new value"),
                                       int_value);
                            } else if (!strcmp(name, "long_delay_ms")) {
                                set_long_delay = int_value;
                                qprint("%s: %d ms\n",
                                       _("Setting long delay to new value"),
                                       int_value);
                            }

                        } else
                            log_event(NULL, err_log, -1, "%s '%s'",
                                      _("Cannot evaluate OPTIONS value of"),
                                      value);

                        if (!strcmp(name, "title")) {
                            qprint("%s: %s\n", _("Setting title to new value"),
                                   value);
                            if (recent_title == NULL) {
                                recent_title = malloc(strlen(value) + 1);
                            } else {
                                if (strlen(recent_title) < strlen(value))
                                    if (realloc(recent_title, strlen(value) + 1)
                                        == NULL) {
                                        log_event(NULL, err_log, -1,
                                                  "%s, %s:%d",
                                                  _("Realloc failed"), __FILE__,
                                                  __LINE__);
                                    }
                            }
                            strcpy(recent_title, value);

                        }
                        if ((!strcmp(name, "err_log"))
                            || (!strcmp(name, "host_log"))) {
                            if (!strcmp(name, "err_log")) {
                                qprint("%s: %s\n",
                                       _("Setting error log to new value"),
                                       value);
                                log_error =
                                    init_log(err_log, value, _("Error log"),
                                             set_nr);
                            } else {
                                qprint("%s: %s\n",
                                       _("Setting host log to new value"),
                                       value);
                                log_error =
                                    init_log(host_log, value,
                                             _("Host status change log"),
                                             set_nr);
                            }
                            if (log_error != LOG_OK) {
                                if (log_error == LOG_BAD_FILE)
                                    log_event(NULL, err_log, -1, "%s: %s",
                                              _("Cannot open file"),
                                              strerror(errno));
                                else
                                    log_event(NULL, err_log, -1, "%s",
                                              log_error_str);
                            }
                        }

                    }
                }
            }
        }                       /* end reading current config file */


        if (host_ininitalized_curr_file > 0) {
            /* set logging where not specified yet */
            host_ptr = hosts->host_info;
            /* scan for enabled/disabled logging in current set */
            log_ena = 0;
            log_dis = 0;
            log_def = log_if_status_last;
            while (host_ptr != NULL) {
                if (host_ptr->set_nr == set_nr) {
                    if (host_ptr->log_if_status_last == LOG_DISABLED)
                        log_dis++;
                    if (host_ptr->log_if_status_last >= 0)
                        log_ena++;
                }
                host_ptr = host_ptr->next;
            }
            /* Only log disabled found - enable the rest */
            if ((log_dis > 0) && (log_ena == 0)) {
                log_def = log_if_status_last;
                qprint("%s\n",
                       _("Only log disabled host found. The rest of the hosts will be logged."));
            }
            /* Only log enabled found - disable the rest */
            else if ((log_ena > 0) && (log_dis == 0)) {
                log_def = LOG_DISABLED;
                qprint("%s\n",
                       _("Only log enabled host found. The rest of the hosts will not be logged."));
            }
            /* Otherwise use default value :-) */
            else if ((log_ena > 0) && (log_dis > 0)) {
                log_def = log_if_status_last;
                qprint("%s\n", _("Log and nolog hosts found."));
                if (log_if_status_last == DEFAULT_STATUS_LAST) {
                    qprint("%s ",
                           _("Not specified hosts will use default setting:"));
                } else {
                    qprint("%s ",
                           _("Not specified hosts will use gobal setting:"));
                }
                if (log_if_status_last == -1) {
                    qprint("%s\n", _("do not log."));
                } else if (log_if_status_last == 0) {
                    qprint("%s\n", _("log (every change)."));
                } else if (log_if_status_last > 0) {
                    qprint("%s (%s %d %s).\n", _("log"),
                           _("with status last longer than"),
                           log_if_status_last, _("seconds"));
                } else
                    qprint("%s\n", _("unknown setting"));
            }
            /* Now set every host which has LOG_DEFAULT to the new value */
            host_ptr = hosts->host_info;
            while (host_ptr != NULL) {
                if ((host_ptr->set_nr == set_nr)
                    && (host_ptr->log_if_status_last == LOG_DEFAULT))
                    host_ptr->log_if_status_last = log_def;
                host_ptr = host_ptr->next;
            }
        }

        /* if default config file read, quit parsing */
        if (cfg_filenames == NULL)
            end_parsing = 1;
        if (cfgfilesidx != NULL) {
            cfgfilesidx = cfgfilesidx->next;
            if (cfgfilesidx == NULL)
                end_parsing = 1;
            else
                strncpy(cfg_path, cfgfilesidx->filename, sizeof(cfg_path) - 1);
            /* found anything in current file (set) ? */
            if (host_ininitalized_curr_file > 0) {
                /* Prepare for next set */
                set_nr++;
                id_in_set = 0;
            }
        }

        /* set title */
        if (host_ininitalized_curr_file > 0) {
            if (hosts->titles == NULL) {
                hosts->titles = malloc(sizeof(titlelist));
                tlist = hosts->titles;
            } else {
                tlist->next = malloc(sizeof(titlelist));
                tlist = tlist->next;
            }
            tlist->next = NULL;
            /* set refresh_interval for this set */
            tlist->refresh_int = set_refresh_interval;
            stdcntlen = 0;
            /* if no title found, set standard title */
            if (recent_title == NULL) {
                if (std_title_cnt < 10)
                    stdcntlen = 1;
                /* we don't suppose to have more than 2 ciphers for std_title_nr */
                else
                    stdcntlen = MIN_TITLE_LEN;
                std_title_cnt++;
                recent_title = malloc(strlen(STD_TITLE) + stdcntlen);
                strcpy(recent_title, STD_TITLE);
            }
            if (stdcntlen) {
                stdcntlen++;    /* if there is a number, don't forget to have space before it */
                sprintf(tmpstr, "%d", std_title_cnt % 100);
                /* test for minimal short title like stcntlen = ".. nr" */
                if ((stdcntlen + 2) > MAX_TITLE_LEN) {
                    tlist->title = malloc(MAX_TITLE_LEN + 1);
                    strcat(tlist->title, tmpstr);
                }
            }

            /* title is bigger than allowed and title still not set */
            if ((tlist->title == NULL)
                && (strlen(recent_title) > MAX_TITLE_LEN)) {
                tlist->title = malloc(MAX_TITLE_LEN + 1);
                strncpy(tlist->title, recent_title,
                        MAX_TITLE_LEN - 2 - stdcntlen);
                tlist->title[MAX_TITLE_LEN - 3 - stdcntlen] = 0;
                strcat(tlist->title, "..");
                if (stdcntlen)
                    strcat(tlist->title, tmpstr);
            } else {
                tlist->title = malloc(strlen(recent_title) + 1);
                strcpy(tlist->title, recent_title);
            }
            tlist->nr = tlistcnt++;
            if (tlistcnt >= MAX_TABS) {
                log_event(NULL, err_log, -1,
                          _("Warning: maximum allowed configuration files exceeded!"));
                end_parsing = 1;
            }
            free(recent_title);
            recent_title = NULL;
        }
    }


    if (host_max == MAX_HOSTS)
        log_event(NULL, err_log, -1, "%s %d %s",
                  _("Maximum count of hosts reached"), MAX_HOSTS,
                  _("Modify your copy of pinger if you want more hosts (requires recompile)."));

    /* remember total hosts count to next parts of program */
    hosts->host_max = host_max;
    qprint("%s: %d\n", _("Total hosts on list"), host_max);

    if (cfg_file_opened)
        fclose(cfg_file);
    qprint("------------------\n");
}
