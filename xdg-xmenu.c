/* Author: Lu Xu <oliver_lew at outlook dot com>
 * License: MIT
 * References: https://specifications.freedesktop.org/desktop-entry-spec
 *             https://specifications.freedesktop.org/icon-theme-spec
 */

#include <ctype.h>
#include <dirent.h>
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

struct Option {
	char *fallback_icon;
	char *icon_theme;
	char *terminal;
	char *xmenu_cmd;
	int dry_run;
	int dump;
	int icon_size;
	int no_genname;
	int no_icon;
	int scale;
	int xdg_de;
} option = {
	.fallback_icon = "application-x-executable",
	.icon_size = 24,
	.scale = 1,
	.terminal = "xterm",
	.xmenu_cmd = "xmenu"
};

typedef struct App {
	char categories[LLEN];
	char entry_path[MLEN];
	char exec[MLEN];
	char genericname[SLEN];
	char icon[SLEN];
	char name[SLEN];
	char path[MLEN];
	int terminal;
	int not_show;
} App;

typedef struct MenuEntry {
	char category[SLEN];
	char text[LLEN];
	struct MenuEntry *next;
} MenuEntry;

typedef struct List {
	char text[SLEN];
	struct List *next;
} List;

typedef struct spawn_t {
	int readfd;
	int writefd;
	pid_t pid;
} spawn_t;

struct Category2Name {
	char *catetory;
	char *name;
} xdg_categories[] = {
	{"AudioVideo", "Multimedia"},
	{"Audio", "Multimedia"},
	{"Video", "Multimedia"},
	{"Development", "Development"},
	{"Education", "Education"},
	{"Game", "Games"},
	{"Graphics", "Graphics"},
	{"Network", "Internet"},
	{"Office", "Office"},
	{"Science", "Science"},
	{"Settings", "Settings"},
	{"System", "System"},
	{"Utility", "Accessories"},
	{"Others", "Others"}
};

struct Name2Icon {
	char *category;
	char *icon;
} category_icons[] = {
	{"Multimedia", "applications-multimedia"},
	{"Development", "applications-development"},
	{"Education", "applications-education"},
	{"Games", "applications-games"},
	{"Graphics", "applications-graphics"},
	{"Internet", "applications-internet"},
	{"Office", "applications-office"},
	{"Science", "applications-science"},
	{"Settings", "preferences-desktop"},
	{"System", "applications-system"},
	{"Accessories", "applications-accessories"},
	{"Others", "applications-other"}
};

const char *usage_str =
	"xdg-xmenu [-deGhIn] [-b ICON] [-i THEME] [-s SIZE] [-S SCALE] [-t TERMINAL] [-x CMD]\n\n"
	"Generate XDG menu for xmenu.\n\n"
	"Options:\n"
	"  -h          Show this help message and exit\n"
	"  -b ICON     Fallback icon name, default is application-x-executable\n"
	"  -d          Dump generated menu, do not run xmenu\n"
	"  -e          Show apps according to desktop environments\n"
	"  -G          Do not show generic name of the app\n"
	"  -i THEME    Icon theme for app icons. Default to gtk3 settings\n"
	"  -I          Disable icon in xmenu\n"
	"  -n          Do not run app, output to stdout\n"
	"  -s SIZE     Icon theme for app icons\n"
	"  -S SCALE    Icon size scale factor, work with HiDPI screens\n"
	"  -t TERMINAL Terminal emulator to use, default is xterm\n"
	"  -x CMD      Xmenu command to use, default is xmenu\n";

char PATH[LLEN];
char HOME[SLEN];
char XDG_DATA_HOME[SLEN];
char XDG_DATA_DIRS[LLEN];
char XDG_CONFIG_HOME[SLEN];
char XDG_CURRENT_DESKTOP[SLEN];
char DATA_DIRS[LLEN + MLEN];
char FALLBACK_ICON_PATH[MLEN];
char *FALLBACK_ICON_THEME = "hicolor";
List all_dirs, path_list, data_dirs_list, current_desktop_list;

int check_desktop(const char *show_in_list);
int check_exec(const char *cmd);
int collect_icon_subdir(void *user, const char *section, const char *name, const char *value);
void find_icon(char *icon_path, char *icon_name);
void find_icon_dirs(List *dirs);
void gen_entry(App *app, MenuEntry *entry);
int get_app(void *user, const char *section, const char *name, const char *value);
void getenv_fb(char *dest, char *name, char *fallback, int n);
void list_insert(List *list, char *text, int n);
void list_free(List *list);
int set_icon_theme();
int set_icon_theme_handler(void *user, const char *section, const char *name, const char *value);
void show_xdg_menu();
spawn_t spawn(const char *cmd, char *const argv[]);
void split_env(List *list, char *env_string);

int check_desktop(const char *desktop_list) {
	for (List *desktop = current_desktop_list.next; desktop; desktop = desktop->next)
		if (strstr(desktop_list, desktop->text))
			return 1;
	return 0;
}

int check_exec(const char *cmd)
{
	char file[MLEN];
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

void find_icon(char *icon_path, char *icon_name)
{
	char test_path[MLEN];
	static const char *exts[] = {"svg", "png", "xpm"};

	for (List *dir = all_dirs.next; dir; dir = dir->next) {
		for (int i = 0; i < 3; i++) {
			snprintf(test_path, MLEN, "%s/%s.%s", dir->text, icon_name, exts[i]);
			if (access(test_path, F_OK) == 0) {
				strncpy(icon_path, test_path, MLEN);
				return;
			}
		}
	}
	strncpy(icon_path, FALLBACK_ICON_PATH, MLEN);
}

void find_icon_dirs(List *dirs)
{
	int res, len_parent;
	char dir_parent[SLEN] = {0}, index_theme[MLEN] = {0};

	for (List *dir = data_dirs_list.next; dir; dir = dir->next) {
		/* dir is now a data directory */
		snprintf(index_theme, MLEN, "%s/icons/%s/index.theme", dir->text, option.icon_theme);
		if (access(index_theme, F_OK) == 0) {
			if ((res = ini_parse(index_theme, collect_icon_subdir, dirs)) < 0)
				fprintf(stderr, "Desktop file parse failed: %d\n", res);
			/* mannually call. a hack to process the end of file */
			collect_icon_subdir(dirs, "", NULL, NULL);
		}

		/* prepend dirs with parent path */
		len_parent = snprintf(dir_parent, SLEN, "%s/icons/%s/", dir->text, option.icon_theme);
		while (dirs->next) {
			/* FIXME: This is hacky, change this */
			if (dirs->text[0] != '/') {
				strncpy(dirs->text + len_parent, dirs->text, strlen(dirs->text));
				memcpy(dirs->text, dir_parent, strlen(dir_parent));
			}
			dirs = dirs->next;
		}
	}
}

void gen_entry(App *app, MenuEntry *entry)
{
	char *perc, field, replace_str[MLEN];
	char icon_path[MLEN], name[MLEN + 4], command[MLEN + SLEN], buffer[MLEN];

	if (app->terminal)
		sprintf(command, "%s -e %s", option.terminal, app->exec);
	else
		strcpy(command, app->exec);

	/* replace field codes */
	/* search starting from right, this way the starting position stays the same */
	while ((perc = strrchr(command, '%')) != NULL) {
		field = *(perc + 1);
		if (isalpha(field)) {
			memset(replace_str, 0, MLEN);
			if (field == 'c')
				strncpy(replace_str, app->entry_path, MLEN);
			else if (field == 'i' && strlen(app->icon) != 0)
				snprintf(replace_str, MLEN, "--icon %s", app->icon);
			else if (field == 'k')
				strncpy(replace_str, app->name, MLEN);
			strncpy(buffer, perc + 2, MLEN);
			snprintf(perc, MLEN - (perc - command), "%s%s", replace_str, buffer);
		}
	}

	find_icon(icon_path, app->icon);
	if (!option.no_genname && strlen(app->genericname) > 0)
		sprintf(name, "%s (%s)", app->name, app->genericname);
	else
		strcpy(name, app->name);

	if (option.no_icon)
		sprintf(entry->text, "\t%s\t%s", name, command);
	else
		sprintf(entry->text, "\tIMG:%s\t%s\t%s", icon_path, name, command);
}

/* Handler for ini_parse, parse app info and save in App variable pointed by *user */
int get_app(void *user, const char *section, const char *name, const char *value)
{
	App *app = (App *)user;
	if (strcmp(section, "Desktop Entry") == 0) {
		if (strcmp(name, "Exec") == 0)
			strcpy(app->exec, value);
		else if (strcmp(name, "Icon") == 0)
			strcpy(app->icon, value);
		else if (strcmp(name, "Name") == 0)
			strcpy(app->name, value);
		else if (strcmp(name, "Terminal") == 0)
			app->terminal = strcmp(value, "true") == 0;
		else if (strcmp(name, "GenericName") == 0)
			strcpy(app->genericname, value);
		else if (strcmp(name, "Categories") == 0)
			strcpy(app->categories, value);
		else if (strcmp(name, "Path") == 0)
			strcpy(app->path, value);

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

/* getenv with fallback value */
void getenv_fb(char *dest, char *name, char *fallback, int n)
{
	char *env;

	if ((env = getenv(name))) {
		strncpy(dest, env, n);
	} else if (fallback) {
		if (fallback[0] != '/')  /* relative path to $HOME */
			snprintf(dest, n, "%s/%s", HOME, fallback);
		else
			strncpy(dest, fallback, n);
	}
}

void list_insert(List *list, char *text, int n)
{
	List *tmp = malloc(sizeof(List));
	strncpy(tmp->text, text, n);
	tmp->next = list->next;
	list->next = tmp;
}

void list_free(List *list)
{
	List *p, *tmp;

	p = list->next;
	list->next = NULL;
	while (p) {
		tmp = p->next;
		free(p);
		p = tmp;
	}
}

int set_icon_theme()
{
	int res;
	char gtk3_settings[MLEN] = {0}, *real_path, *buffer;

	if (option.icon_theme && strlen(option.icon_theme) > 0)
		return 0;

	buffer = malloc(SLEN);
	snprintf(gtk3_settings, MLEN, "%s/gtk-3.0/settings.ini", XDG_CONFIG_HOME);
	if (access(gtk3_settings, F_OK) == 0) {
		real_path = realpath(gtk3_settings, NULL);
		if ((res = ini_parse(real_path, set_icon_theme_handler, buffer)) < 0)
			fprintf(stderr, "failed parse gtk settings\n");
		free(real_path);
	}

	if (strlen(buffer) == 0)
		option.icon_theme = FALLBACK_ICON_THEME;
	else
		option.icon_theme = buffer;
	return 1;
}

int set_icon_theme_handler(void *user, const char *section, const char *name, const char *value)
{
	if (strcmp(section, "Settings") == 0 && strcmp(name, "gtk-icon-theme-name") == 0)
		strcpy(user, value);
	return 1;
}

/*
 * handler for ini_parse
 * match subdirectories in an icon theme folder by parsing an index.theme file
 * - the icon size is options.icon_size
 * - the icon theme will be specified as the parsed the index.theme file
 */
int collect_icon_subdir(void *user, const char *section, const char *name, const char *value)
{
	/* static variables to preserve between function calls */
	static char subdir[32], type[16];
	static int size, minsize, maxsize, threshold, scale;

	List *dirs = (List *)user;
	while (dirs->next != NULL)
		dirs = dirs->next;

	if (strcmp(section, subdir) != 0 || (!name && !value)) {
		/* Check the icon size after finished parsing a section */
		if (scale == option.scale)
			if (((strcmp(type, "Threshold") == 0 || strlen(type) == 0)
					&& abs(size - option.icon_size) <= threshold)
				|| (strcmp(type, "Fixed") == 0
					&& size == option.icon_size)
				|| (strcmp(type, "Scalable") == 0
					&& minsize <= option.icon_size
					&& maxsize >= option.icon_size)) {
				/* save dirs into this linked list */
				list_insert(dirs, subdir, SLEN);
			}

		/* reset the current section */
		strncpy(subdir, section, 32);
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
		strncpy(type, value, 16);
	}

	return 1;
}

void show_xdg_menu(FILE *xmenu_input)
{
	int res, icon_theme_need_free;
	App app;
	MenuEntry menuentry;
	char folder[MLEN] = {0}, path[LLEN] = {0};
	DIR *dir;
	struct dirent *entry;

	icon_theme_need_free = set_icon_theme();

	find_icon_dirs(&all_dirs);
	list_insert(&all_dirs, "/usr/share/pixmaps", SLEN);

	find_icon(FALLBACK_ICON_PATH, option.fallback_icon);

	/* output all app in folder */
	for (List *data_dir = data_dirs_list.next; data_dir; data_dir = data_dir->next) {
		sprintf(folder, "%s/applications", data_dir->text);
		if ((dir = opendir(folder)) == NULL) {
			continue;
		}

		while ((entry = readdir(dir)) != NULL) {
			if (strstr(entry->d_name, ".desktop") == NULL) {
				continue;
			}
			sprintf(path, "%s/%s", folder, entry->d_name);

			memset(&app, 0, sizeof(App));
			strcpy(app.entry_path, path);
			if ((res = ini_parse(path, get_app, &app)) < 0)
				fprintf(stderr, "Desktop file parse failed: %d\n", res);

			if (app.not_show)
				continue;

			gen_entry(&app, &menuentry);
			if (option.dump)
				printf("%s\n", menuentry.text);
			else
				fprintf(xmenu_input, "%s\n", menuentry.text);
		}

		if (dir)
			closedir(dir);
	}

	if (icon_theme_need_free)
		free(option.icon_theme);
	list_free(&all_dirs);
}

/*
 * User input 1--------->0 cmd 1-------->0 Output
 *             pfd_write        pdf_read
 * Create 2 pipes connecting cmd process with pfd_read[1] and pfd_write[0], and
 * return pfd_read[0] and pfd_write[1] back to user for read and write.
 */
spawn_t spawn(const char *cmd, char *const argv[])
{
	pid_t pid;
	spawn_t status = { -1, -1, -1 };
	int pfd_read[2] = { -1, -1 }, pfd_write[2] = { -1, -1 };

	pipe(pfd_read);
	pipe(pfd_write);

	if ((pid = fork()) == 0) { /* in child */
		dup2(pfd_read[1], 1);
		dup2(pfd_write[0], 0);
		close(pfd_read[0]);
		close(pfd_write[1]);
		if(execvp(cmd, argv) == -1) {
			fprintf(stderr, "execute %s failed\n", cmd);
			exit(1);
		}
	} else if (pid > 0) { /* in parent */
		status.pid = pid;
		status.readfd = pfd_read[0];
		status.writefd = pfd_write[1];
	}

	close(pfd_read[1]);
	close(pfd_write[0]);
	return status;
}

void split_env(List *list, char *env_string)
{
	List *tmp;

	for (char *p = strtok(env_string, ":"); p; p = strtok(NULL, ":")) {
		tmp = (List *)malloc(sizeof(List));
		strncpy(tmp->text, p, SLEN);
		tmp->next = list->next;
		list->next = tmp;
	}
}

int main(int argc, char *argv[])
{
	getenv_fb(PATH, "PATH", NULL, LLEN);
	getenv_fb(HOME, "HOME", NULL, SLEN);
	getenv_fb(XDG_DATA_HOME, "XDG_DATA_HOME", ".local/share", SLEN);
	getenv_fb(XDG_DATA_DIRS, "XDG_DATA_DIRS", "/usr/share:/usr/local/share", LLEN);
	getenv_fb(XDG_CONFIG_HOME, "XDG_CONFIG_HOME", ".config", SLEN);
	getenv_fb(XDG_CURRENT_DESKTOP, "XDG_CURRENT_DESKTOP", NULL, SLEN);
	snprintf(DATA_DIRS, LLEN + MLEN, "%s:%s", XDG_DATA_DIRS, XDG_DATA_HOME);

	split_env(&path_list, PATH);
	split_env(&data_dirs_list, DATA_DIRS);
	split_env(&current_desktop_list, XDG_CURRENT_DESKTOP);

	int c;
	while ((c = getopt(argc, argv, "b:deGhi:Ins:S:t:x:")) != -1) {
		switch (c) {
			case 'b': option.fallback_icon = optarg; break;
			case 'd': option.dump = 1; break;
			case 'e': option.xdg_de = 1; break;
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

	if (option.dump) {
		show_xdg_menu(NULL);
	} else {
		/* FIXME: check return */
		char *xmenu_argv[] = {NULL}, line[1024] = {0};
		FILE *fwrite, *fread;
		/* TODO: maybe use -- to split xdg-xmenu and xmenu args? */
		spawn_t s = spawn(option.xmenu_cmd, xmenu_argv);

		fwrite = fdopen(s.writefd, "w");
		show_xdg_menu(fwrite);
		fclose(fwrite);

		waitpid(s.pid, NULL, 0);
		fread = fdopen(s.readfd, "r");
		if (fgets(line, 1024, fread)) {
			if (option.dry_run)
				printf("%s", line);
			else
				system(line);
		}

		close(s.writefd);
		close(s.readfd);
	}

	return 0;
}
