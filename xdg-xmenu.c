/* Author: Lu Xu <oliver_lew at outlook dot com>
 * License: MIT
 * References: https://specifications.freedesktop.org/desktop-entry-spec
 *             https://specifications.freedesktop.org/icon-theme-spec
 */

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ini.h>

/* for long texts */
#define LLEN 1024
/* for file paths */
#define MLEN 256
/* for simple names or directories */
#define SLEN 128

#define LEN(X) (sizeof(X) / sizeof(X[0]))

struct Option {
	char *fallback_icon;
	char *icon_theme;
	char *terminal;
	char *xmenu_cmd;
	int debug;
	int dry_run;
	int dump;
	int icon_size;
	int no_genname;
	int no_icon;
	int scale;
} option = {
	.fallback_icon = "application-x-executable",
	.icon_size = 24,
	.scale = 1,
	.terminal = "xterm",
	.xmenu_cmd = "xmenu"
};

typedef struct App {
	/* from desktop entry file */
	char category[SLEN];
	char exec[MLEN];
	char genericname[SLEN];
	char icon[SLEN];
	char name[SLEN];
	char path[MLEN];
	char type[SLEN];
	int terminal;
	/* derived attributes */
	char entry_path[LLEN];
	char xmenu_entry[LLEN];
	int not_show;
	struct App *next;
} App;

typedef struct List {
	char text[SLEN];
	int fd;
	struct List *next;
} List;

struct Category2Name {
	char *category;
	char *name;
} xdg_categories[] = {
	{"Audio", "Multimedia"},
	{"AudioVideo", "Multimedia"},
	{"Development", "Development"},
	{"Education", "Education"},
	{"Game", "Games"},
	{"Graphics", "Graphics"},
	{"Network", "Internet"},
	{"Office", "Office"},
	{"Others", "Others"},
	{"Science", "Science"},
	{"Settings", "Settings"},
	{"System", "System"},
	{"Utility", "Accessories"},
	{"Video", "Multimedia"}
};

struct Name2Icon {
	char *category;
	char *icon;
} category_icons[] = {
	{"Accessories", "applications-accessories"},
	{"Development", "applications-development"},
	{"Education", "applications-education"},
	{"Games", "applications-games"},
	{"Graphics", "applications-graphics"},
	{"Internet", "applications-internet"},
	{"Multimedia", "applications-multimedia"},
	{"Office", "applications-office"},
	{"Others", "applications-other"},
	{"Science", "applications-science"},
	{"Settings", "preferences-desktop"},
	{"System", "applications-system"}
};

const char *usage_str =
	"xdg-xmenu [-dGhIn] [-b ICON] [-i THEME] [-s SIZE] [-S SCALE] [-t TERMINAL] [-x CMD] [-- <xmenu_args>]\n\n"
	"Generate XDG menu for xmenu.\n\n"
	"Options:\n"
	"  -h          Show this help message and exit\n"
	"  -b ICON     Fallback icon name, default is application-x-executable\n"
	"  -d          Dump generated menu, do not run xmenu\n"
	"  -G          Do not show generic name of the app\n"
	"  -i THEME    Icon theme for app icons. Default to gtk3 settings\n"
	"  -I          Disable icon in xmenu\n"
	"  -n          Do not run app, output to stdout\n"
	"  -s SIZE     Icon size for app icons\n"
	"  -S SCALE    Icon scale factor, useful in HiDPI screens\n"
	"  -t TERMINAL Terminal emulator to use, default is xterm\n"
	"  -x CMD      Xmenu command to use, default is xmenu\n"
	"Note:\n  Options after `--' are passed to xmenu\n";

char PATH[LLEN];
char HOME[SLEN];
char XDG_DATA_HOME[SLEN];
char XDG_DATA_DIRS[LLEN];
char XDG_CONFIG_HOME[SLEN];
char XDG_CURRENT_DESKTOP[SLEN];
char DATA_DIRS[LLEN + MLEN];
char FALLBACK_ICON_PATH[MLEN];
char FALLBACK_ICON_THEME[SLEN] = "hicolor";
List icon_dirs, path_list, data_dirs_list, current_desktop_list;
App all_apps;

int  cmp_app_category_name(const void *p1, const void *p2);
int  check_app(App *app);
int  check_desktop(const char *desktop_list);
int  check_exec(const char *cmd);
void clean_up_lists();
void debug_msg(const char *msg, ...);
void extract_main_category(char *category, const char *categories);
void find_all_apps();
void find_icon(char *icon_path, char *icon_name);
void find_icon_dirs();
void gen_entry(App *app);
void getenv_fb(char *dest, char *name, char *fallback, int n);
int  handler_icon_dirs_theme(void *user, const char *section, const char *name, const char *value);
int  handler_parse_app(void *user, const char *section, const char *name, const char *value);
int  handler_set_icon_theme(void *user, const char *section, const char *name, const char *value);
void list_free(List *list);
void list_insert(List *l, char *text, int n);
void list_reverse(List *l);
void prepare_envvars();
void xmenu_dump(FILE *fp);
void xmenu_run(int argc, char *argv[]);
void set_icon_theme();
int  spawn(const char *cmd, char *const argv[], int *fd_input, int *fd_output);
void split_to_list(List *list, const char *env_string, char *sep);

int cmp_app_category_name(const void *p1, const void *p2)
{
	int cmp_category, cmp_name;
	App *a1 = *(App **)p1, *a2 = *(App **)p2;

	cmp_category = strcmp(a1->category, a2->category);
	cmp_name = strcasecmp(a1->name, a2->name);
	return cmp_category ? cmp_category : cmp_name;
}

int check_app(App *app)
{
	if (strcmp(app->type, "Application") != 0
		|| strlen(app->exec) == 0
		|| strlen(app->name) == 0)
		return 0;
	return 1;
}

int check_desktop(const char *desktop_list)
{
	for (List *desktop = current_desktop_list.next; desktop; desktop = desktop->next)
		if (strstr(desktop_list, desktop->text))
			return 1;
	return 0;
}

int check_exec(const char *cmd)
{
	char file[MLEN] = {0};
	struct stat sb;

	/* if command start with '/', check it directly */
	if (cmd[0] == '/')
		return stat(cmd, &sb) == 0 && sb.st_mode & S_IXUSR;

	for (List *dir = path_list.next; dir; dir = dir->next) {
		sprintf(file, "%s/%s", dir->text, cmd);
		if (stat(file, &sb) == 0 && sb.st_mode & S_IXUSR)
			return 1;
	}
	return 0;
}

void clean_up_lists()
{
	for (List *dir = icon_dirs.next; dir; dir = dir->next)
		if (dir->fd > 0) {
			debug_msg("%d %s\n", dir->fd, dir->text);
			close(dir->fd);
		}
	list_free(&icon_dirs);
	list_free(&path_list);
	list_free(&data_dirs_list);
	list_free(&current_desktop_list);
	for (App *p = all_apps.next, *tmp; p; tmp = p->next, free(p), p = tmp) ;
	all_apps.next = NULL;

}

void debug_msg(const char *msg, ...)
{	
	if (!option.debug)
		return;

	va_list args;
	fprintf(stderr, "DEBUG: ");
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	va_end(args);
}

void extract_main_category(char *category, const char *categories)
{
	List list_categories = {0}, *s;

	split_to_list(&list_categories, categories, ";");
	for (s = list_categories.next; s; s = s->next)
		for (int i = 0; i < LEN(xdg_categories); i++)
			if (strcmp(xdg_categories[i].category, s->text) == 0)
				snprintf(category, SLEN, "%s", xdg_categories[i].name);

	list_free(&list_categories);
}

void find_all_apps()
{
	int res;
	char folder[MLEN] = {0}, path[LLEN] = {0}, *ext;
	DIR *dir;
	struct dirent *entry;
	App *app;

	/* output all app in folder */
	for (List *data_dir = data_dirs_list.next; data_dir; data_dir = data_dir->next) {
		sprintf(folder, "%s/applications", data_dir->text);
		if ((dir = opendir(folder)) == NULL)
			continue;

		while ((entry = readdir(dir)) != NULL) {
			ext = strrchr(entry->d_name, '.');
			if ((entry->d_type != DT_REG
					&& entry->d_type != DT_LNK
					&& entry->d_type != DT_UNKNOWN)  /* not file */
				|| !ext || strcmp(ext, ".desktop") != 0) /* not desktop entry */
				continue;

			app = calloc(1, sizeof(App));
			sprintf(path, "%s/%s", folder, entry->d_name);
			debug_msg("Ini parse app entry: %s\n", path);
			if ((res = ini_parse(path, handler_parse_app, app)) > 0)
				debug_msg("%s parse failed: %d\n", path, res);

			if (!app->not_show && check_app(app)) {
				gen_entry(app);
				snprintf(app->entry_path, LLEN, "%s", path);
				if (strlen(app->category) == 0)
					snprintf(app->category, SLEN, "%s", "Others");
				app->next = all_apps.next;
				all_apps.next = app;
			}
		}
		closedir(dir);
	}
}

void find_icon(char *icon_path, char *icon_name)
{
	const char *exts[] = {"svg", "png", "xpm"};
	char test_path[SLEN] = {0};

	/* provided icon is a file path */
	if (icon_name[0] == '/') {
		snprintf(icon_path, MLEN, "%s", access(icon_name, F_OK) == 0 ?
				 icon_name : FALLBACK_ICON_PATH);
		return;
	}

	for (List *dir = icon_dirs.next; dir; dir = dir->next) {
		for (int i = 0; i < 3; i++) {
			snprintf(test_path, SLEN, "%s.%s", icon_name, exts[i]);
			/* use faccessat, might be faster than access
			 * the reason is that the directory's fd is already opened */
			if (faccessat(dir->fd, test_path, F_OK, 0) == 0) {
				snprintf(icon_path, MLEN, "%s/%s", dir->text, test_path);
				return;
			}
		}
	}
	snprintf(icon_path, MLEN, "%s", FALLBACK_ICON_PATH);
}

void find_icon_dirs()
{
	int res, len_parent;
	char dir_parent[SLEN] = {0}, index_theme[MLEN] = {0};

	for (List *dir = data_dirs_list.next; dir; dir = dir->next) {
		snprintf(index_theme, MLEN, "%s/icons/%s/index.theme", dir->text, option.icon_theme);
		if (access(index_theme, F_OK) == 0) {
			debug_msg("Ini parse icon theme: %s\n", index_theme);
			if ((res = ini_parse(index_theme, handler_icon_dirs_theme, NULL)) > 0)
				debug_msg("%s parse failed: %d\n", index_theme, res);
			/* mannually call, a hack to process the end of file */
			handler_icon_dirs_theme(NULL, "", NULL, NULL);
		}

		/* prepend dirs with parent path */
		len_parent = snprintf(dir_parent, SLEN, "%s/icons/%s/", dir->text, option.icon_theme);
		for (List *idir = icon_dirs.next; idir; idir = idir->next) {
			/* FIXME: This is hacky, change this */
			if (idir->text[0] != '/') {
				strncpy(idir->text + len_parent, idir->text, strlen(idir->text));
				memcpy(idir->text, dir_parent, strlen(dir_parent));
			}
		}
	}

	list_insert(&icon_dirs, "/usr/share/pixmaps", SLEN);
	for (List *idir = icon_dirs.next; idir; idir = idir->next) {
		idir->fd = open(idir->text, O_RDONLY);
		debug_msg("%d %s\n", idir->fd, idir->text);
	}
	/* This will restore the icon directories as in index.theme file,
	 *   which results in fewer checks of file existance (-70% in my case!).
	 * Reason: the "apps", "category" folder will be searched earlier */
	list_reverse(&icon_dirs);
}

void gen_entry(App *app)
{
	char *perc, field, replace_str[LLEN] = {0};
	char icon_path[MLEN] = {0}, buffer[MLEN] = {0};
	char name[MLEN + 4] = {0}, command[MLEN + SLEN] = {0};

	if (app->terminal)
		snprintf(command, sizeof(command), "%s -e %s", option.terminal, app->exec);
	else
		snprintf(command, sizeof(command), "%s", app->exec);

	/* replace field codes */
	/* search starting from right, this way the starting position stays the same */
	while ((perc = strrchr(command, '%')) != NULL) {
		field = *(perc + 1);
		if (isalpha(field)) {
			memset(replace_str, 0, MLEN);
			if (field == 'c')
				snprintf(replace_str, LLEN, "%s", app->entry_path);
			else if (field == 'i' && strlen(app->icon) != 0)
				snprintf(replace_str, LLEN, "--icon %s", app->icon);
			else if (field == 'k')
				snprintf(replace_str, LLEN, "%s", app->name);
			snprintf(buffer, MLEN, "%s", perc + 2);
			snprintf(perc, MLEN - (perc - command), "%s%s", replace_str, buffer);
		}
	}

	if (!option.no_genname && strlen(app->genericname) > 0)
		snprintf(name, sizeof(name), "%s (%s)", app->name, app->genericname);
	else
		strcpy(name, app->name);

	if (!option.no_icon)
		find_icon(icon_path, app->icon);

	if (option.no_icon || strlen(icon_path) == 0)
		sprintf(app->xmenu_entry, "\t%s\t%s", name, command);
	else
		sprintf(app->xmenu_entry, "\tIMG:%s\t%s\t%s", icon_path, name, command);
}

/* getenv with fallback value */
void getenv_fb(char *dest, char *name, char *fallback, int n)
{
	char *env;

	if ((env = getenv(name))) {
		snprintf(dest, n, "%s", env);
	} else if (fallback) {
		if (fallback[0] != '/')  /* relative path to $HOME */
			snprintf(dest, n, "%s/%s", HOME, fallback);
		else
			snprintf(dest, n, "%s", fallback);
	}
}

/*
 * handler for ini_parse
 * match subdirectories in an icon theme folder by parsing an index.theme file
 * - the icon size is options.icon_size
 * - the icon theme will be specified as the parsed the index.theme file
 */
int handler_icon_dirs_theme(void *user, const char *section, const char *name, const char *value)
{
	/* static variables to preserve between function calls */
	static char subdir[32], type[16];
	static int size, minsize, maxsize, threshold, scale;

	if ((!name && !value) || strcmp(section, subdir) != 0) {
		/* Check the icon size after finished parsing a section */
		if (scale == option.scale
			&& (((strcmp(type, "Threshold") == 0 || strlen(type) == 0)
					&& abs(size - option.icon_size) <= threshold)
				|| (strcmp(type, "Fixed") == 0
					&& size == option.icon_size)
				|| (strcmp(type, "Scalable") == 0
					&& minsize <= option.icon_size
					&& maxsize >= option.icon_size)))
			/* save dirs into this linked list */
			list_insert(&icon_dirs, subdir, SLEN);

		/* reset the current section */
		snprintf(subdir, 32, "%s", section);
		size = minsize = maxsize = -1;
		threshold = 2;  /* threshold fallback value */
		scale = 1;
		memset(type, 0, 16);
	}

	/* save the values into those static variables */
	if (!name && !value) { /* end of the file/section */
		return 1;
	} else if (strcmp(name, "Size") == 0) {
		size = atoi(value);
		if (minsize == -1)  /* minsize fallback value */
			minsize = size;
		if (maxsize == -1)  /* maxsize fallback value */
			maxsize = size;
	} else if (strcmp(name, "MinSize") == 0) {
		minsize = atoi(value);
	} else if (strcmp(name, "MaxSize") == 0) {
		maxsize = atoi(value);
	} else if (strcmp(name, "Threshold") == 0) {
		threshold = atoi(value);
	} else if (strcmp(name, "Scale") == 0) {
		scale = atoi(value);
	} else if (strcmp(name, "Type") == 0) {
		snprintf(type, 16, "%s", value);
	}

	return 1;
}

/* Handler for ini_parse, parse app info and save in App variable pointed by *user */
int handler_parse_app(void *user, const char *section, const char *name, const char *value)
{
	App *app = (App *)user;
	if (strcmp(section, "Desktop Entry") == 0) {
		if (strcmp(name, "Exec") == 0)
			snprintf(app->exec, MLEN, "%s", value);
		else if (strcmp(name, "Type") == 0)
			snprintf(app->type, SLEN, "%s", value);
		else if (strcmp(name, "Icon") == 0)
			snprintf(app->icon, SLEN, "%s", value);
		else if (strcmp(name, "Name") == 0)
			snprintf(app->name, SLEN, "%s", value);
		else if (strcmp(name, "Terminal") == 0)
			app->terminal = strcmp(value, "true") == 0;
		else if (strcmp(name, "GenericName") == 0)
			snprintf(app->genericname, SLEN, "%s", value);
		else if (strcmp(name, "Categories") == 0)
			extract_main_category(app->category, value);
		else if (strcmp(name, "Path") == 0)
			snprintf(app->path, MLEN, "%s", value);

		if ((strcmp(name, "NoDisplay") == 0 && strcmp(value, "true") == 0)
			|| (strcmp(name, "Hidden") == 0 && strcmp(value, "true") == 0)
			|| (strcmp(name, "Type") == 0 && strcmp(value, "Application") != 0)
			|| (strcmp(name, "TryExec") == 0 && check_exec(value) == 0)
			|| (strcmp(name, "NotShowIn") == 0 && check_desktop(value))
			|| (strcmp(name, "OnlyShowIn") == 0 && !check_desktop(value)))
			app->not_show = 1;
	}
	return 1;
}

int handler_set_icon_theme(void *user, const char *section, const char *name, const char *value)
{
	if (strcmp(section, "Settings") == 0 && strcmp(name, "gtk-icon-theme-name") == 0)
		strcpy(FALLBACK_ICON_THEME, value);
	return 1;
}

void list_free(List *list)
{
	List *p = list->next, *tmp;

	list->next = NULL;
	while (p) {
		tmp = p->next;
		free(p);
		p = tmp;
	}
}

void list_insert(List *list, char *text, int n)
{
	List *tmp;

	tmp = calloc(1, sizeof(List));
	snprintf(tmp->text, n, "%s", text);
	tmp->next = list->next;
	list->next = tmp;
}

void list_reverse(List *list)
{
	List *curr = list->next, *next;

	list->next = NULL;
	while (curr) {
		next = curr->next;
		curr->next = list->next;
		list->next = curr;
		curr = next;
	}
}

void prepare_envvars()
{
	getenv_fb(PATH, "PATH", NULL, LLEN);
	getenv_fb(HOME, "HOME", NULL, SLEN);
	getenv_fb(XDG_DATA_HOME, "XDG_DATA_HOME", ".local/share", SLEN);
	getenv_fb(XDG_DATA_DIRS, "XDG_DATA_DIRS", "/usr/share:/usr/local/share", LLEN);
	getenv_fb(XDG_CONFIG_HOME, "XDG_CONFIG_HOME", ".config", SLEN);
	getenv_fb(XDG_CURRENT_DESKTOP, "XDG_CURRENT_DESKTOP", NULL, SLEN);
	snprintf(DATA_DIRS, LLEN + MLEN, "%s:%s", XDG_DATA_DIRS, XDG_DATA_HOME);

	/* NOTE: the string in the second argument will be modified, do not use again */
	split_to_list(&path_list, PATH, ":");
	split_to_list(&data_dirs_list, DATA_DIRS, ":");
	split_to_list(&current_desktop_list, XDG_CURRENT_DESKTOP, ":");
}

void xmenu_dump(FILE *fp)
{
	int i, count;
	char icon_path[MLEN] = {0}, *curcat = NULL;
	App **app_array, *app;

	/* construct an array of apps from the linked list */
	for (count = 0, app = all_apps.next; app; count++, app = app->next)
		; /* count the apps */
	app_array = calloc(count + 1, sizeof(App *));
	for (i = 0, app = all_apps.next; app; i++, app = app->next)
		app_array[i] = app;

	qsort(app_array, count, sizeof(App *), cmp_app_category_name);
	for (i = 0; i < count; i++) {
		app = app_array[i];
		if (!curcat || strcmp(curcat, app->category)) {
			curcat = app->category;
			if (!option.no_icon)
				for (int j = 0; j < LEN(category_icons); j++)
					if (strcmp(app->category, category_icons[j].category) == 0) {
						find_icon(icon_path, category_icons[j].icon);
						break;
					}
			if (option.no_icon || strlen(icon_path) == 0)
				fprintf(fp, "%s\n", curcat);
			else
				fprintf(fp, "IMG:%s\t%s\n", icon_path, curcat);
		}
		fprintf(fp, "%s\n", app->xmenu_entry);
	}
	free(app_array);
}

void xmenu_run(int argc, char *argv[])
{
	int pid, fd_input, fd_output;
	char **xmenu_argv, line[LLEN] = {0};
	FILE *fp;

	/* construct xmenu args for exec(3).
	 * +2 is for leading 'xmenu' and the ending NULL
	 * if no_icon is set, add another '-i' option */
	xmenu_argv = calloc(argc + option.no_icon ? 3 : 2, sizeof(char*));
	xmenu_argv[0] = option.xmenu_cmd;
	for (int i = 0; i < argc; i++)
		xmenu_argv[i + 1] = argv[i];
	if (option.no_icon && strcmp(option.xmenu_cmd, "xmenu") == 0)
		xmenu_argv[argc + 1] = "-i";

	pid = spawn(option.xmenu_cmd, xmenu_argv, &fd_input, &fd_output);
	fp = fdopen(fd_input, "w");
	xmenu_dump(fp);
	fclose(fp);

	waitpid(pid, NULL, 0);
	/* Note: use larger buffer size (close to 4k) to get better performance */
	if (read(fd_output, line, LLEN) > 0) {
		*strchr(line, '\n') = 0;
		if (option.dry_run)
			puts(line);
		else
			system(strcat(line, " &"));
	}
	close(fd_output);
}

void set_icon_theme()
{
	int res;
	char gtk3_settings[MLEN] = {0}, *real_path;

	if (option.icon_theme && strlen(option.icon_theme) > 0)
		return;
	option.icon_theme = FALLBACK_ICON_THEME;

	/* Check gtk3 settings.ini file and overwrite default icon theme */
	snprintf(gtk3_settings, MLEN, "%s/gtk-3.0/settings.ini", XDG_CONFIG_HOME);
	if (access(gtk3_settings, F_OK) == 0) {
		real_path = realpath(gtk3_settings, NULL);
		debug_msg("Ini parse gtk settings: %s\n", real_path);
		if ((res = ini_parse(real_path, handler_set_icon_theme, NULL)) > 0)
			debug_msg("failed parse gtk settings: line %d\n", res);
		free(real_path);
	}
}

/*
 * User input 1--------->0 cmd 1-------->0 Output
 *             pfd_write        pdf_read
 * Create 2 pipes connecting cmd process with pfd_read[1] and pfd_write[0], and
 * return pfd_read[0] and pfd_write[1] back to user for read and write.
 */
int spawn(const char *cmd, char *const argv[], int *fd_input, int *fd_output)
{
	pid_t pid;
	int pfd_read[2], pfd_write[2];

	pipe(pfd_read);
	pipe(pfd_write);

	if ((pid = fork()) == 0) { /* in child */
		dup2(pfd_read[1], 1);
		dup2(pfd_write[0], 0);
		close(pfd_read[0]);
		close(pfd_write[1]);
		execvp(cmd, argv);
	} else if (pid > 0) { /* in parent */
		*fd_output = pfd_read[0];
		*fd_input = pfd_write[1];
	}

	close(pfd_read[1]);
	close(pfd_write[0]);
	return pid;
}

void split_to_list(List *list, const char *env_string, char *sep)
{
	char *buffer = strdup(env_string);

	for (char *p = strtok(buffer, sep); p; p = strtok(NULL, sep))
		list_insert(list, p, SLEN);
	free(buffer);
}

int main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "b:dDGhi:Ins:S:t:x:")) != -1) {
		switch (opt) {
			case 'b': option.fallback_icon = optarg; break;
			case 'd': option.dump = 1; break;
			case 'D': option.debug = 1; break;
			case 'G': option.no_genname = 1; break;
			case 'i': option.icon_theme = optarg; break;
			case 'I': option.no_icon = 1; break;
			case 'n': option.dry_run = 1; break;
			case 's': option.icon_size = atoi(optarg); break;
			case 'S': option.scale = atoi(optarg); break;
			case 't': option.terminal = optarg; break;
			case 'x': option.xmenu_cmd = optarg; break;
			case 'h': default: puts(usage_str); exit(0); break;
		}
	}

#ifdef DEBUG
	for (int i = 0; i < 1000; i++) {
#endif
	prepare_envvars();
	set_icon_theme();
	if (!option.no_icon) {
		find_icon_dirs();
		find_icon(FALLBACK_ICON_PATH, option.fallback_icon);
	}
	find_all_apps();

	if (option.dump)
		xmenu_dump(stdout);
	else
		xmenu_run(argc - optind, argv + optind);

	clean_up_lists();
#ifdef DEBUG
	}
#endif
	return 0;
}
