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
#include <unistd.h>

#include <ini.h>

#define LLEN 1024
#define MLEN 256
#define SLEN 64

char *PATH, *HOME, *XDG_DATA_HOME, *XDG_DATA_DIRS, *XDG_CONFIG_HOME, *XDG_CURRENT_DESKTOP;

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
	int terminal;
	char entry_path[MLEN];
	char exec[MLEN];
	char icon[SLEN];
	char name[SLEN];
	char genericname[SLEN];
	char categories[MLEN];
	char path[MLEN];
} App;

typedef struct MenuEntry {
	char category[SLEN];
	char text[LLEN];
	void *next;
} MenuEntry;

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
	"xdg-xmenu [-deGhIn] [-b ICON] [-i THEME] [-s SIZE] [-t TERMINAL] [-x CMD]\n\n"
	"Generate XDG menu for xmenu.\n\n"
	"Options:\n"
	"  -h          show this help message and exit\n"
	"  -b ICON     Fallback icon for apps without icons, default is application-x-executable\n"
	"  -d          Dump generated menu, do not run xmenu\n"
	"  -e          Show apps according to desktop environments\n"
	"  -G          Do not show generic name of the app\n"
	"  -i THEME    Icon theme for app icons. Default to gtk3 settings.\n"
	"  -I          Disable icon in xmenu.\n"
	"  -n          Do not run app, output to stdout\n"
	"  -s SIZE     Icon theme for app icons\n"
	"  -S SCALE    Icon size scale factor, work with HiDPI screens\n"
	"  -t TERMINAL Terminal emulator to use, default is xterm\n"
	"  -x CMD      Xmenu command to use, default is xmenu\n";

int try_exec(const char *cmd) {
	char file[MLEN], path_temp[LLEN], *dir;
	struct stat sb;

	/* if command start with '/', check it directly */
	if (cmd[0] == '/')
		return stat(cmd, &sb) == 0 && sb.st_mode & S_IXUSR;

	strncpy(path_temp, PATH, LLEN);
	dir = strtok(path_temp, ":");
	while (dir != NULL) {
		sprintf(file, "%s/%s", dir, cmd);
		if (stat(file, &sb) == 0 && sb.st_mode & S_IXUSR)
			return 1;
		dir = strtok(NULL, ":");
	}
	return 0;
}

int find_desktop_in(char *current_desktop, const char *show_in_list) {
	char *desktop;

	desktop = strtok(current_desktop, ":");
	while (desktop != NULL) {
		if (strstr(show_in_list, desktop) != NULL)
			return 1;
		desktop = strtok(NULL, ":");
	}
	return 0;
}

/* Handler for ini_parse, check if this application needs to show */
int if_show(void *user, const char *section, const char *name, const char *value) {
	int *flag = (int *)user;
	if (strcmp(section, "Desktop Entry") == 0
		&& ((strcmp(name, "NoDisplay") == 0 && strcmp(value, "true") == 0)
			|| (strcmp(name, "Hidden") == 0 && strcmp(value, "true") == 0)
			|| (strcmp(name, "Type") == 0 && strcmp(value, "Application") != 0)
			|| (strcmp(name, "TryExec") == 0 && try_exec(value) == 0)
			|| (option.xdg_de && strcmp(name, "NotShowIn") == 0
				&& find_desktop_in(XDG_CURRENT_DESKTOP, value))
			|| (option.xdg_de && strcmp(name, "OnlyShowIn") == 0
				&& !find_desktop_in(XDG_CURRENT_DESKTOP, value))))
		*flag = 0;
	return 1;  /* success */
}

/* Handler for ini_parse, parse app info and save in App variable pointed by *user */
int get_app(void *user, const char *section, const char *name, const char *value) {
	App *app = (App *)user;
	if (strcmp(section, "Desktop Entry") == 0) {
		if (strcmp(name, "Exec") == 0) {
			strcpy(app->exec, value);
		} else if (strcmp(name, "Icon") == 0) {
			strcpy(app->icon, value);
		} else if (strcmp(name, "Name") == 0) {
			strcpy(app->name, value);
		} else if (strcmp(name, "Terminal") == 0) {
			app->terminal = strcmp(value, "true") == 0;
		} else if (strcmp(name, "GenericName") == 0) {
			strcpy(app->genericname, value);
		} else if (strcmp(name, "Categories") == 0) {
			strcpy(app->categories, value);
		} else if (strcmp(name, "Path") == 0) {
			strcpy(app->path, value);
		}
	}
	return 1;
}

void find_icon(char *icon_name, char *icon_path) {
	strncpy(icon_path, icon_name, MLEN);
}

void gen_entry(App *app, MenuEntry *entry) {
	int totlen, replen, prelen;
	char *perc, *replace_str, *prefix_str;
	char icon_path[MLEN], name[MLEN], command[MLEN + 32];

	if (app->terminal)
		sprintf(command, "%s -e %s", "xterm", app->exec);
	else
		strcpy(command, app->exec);

	/* replace field codes */
	/* search starting from right, this way the starting position stays the same */
	while ((perc = strrchr(command, '%')) != NULL) {
		if (isalpha(*(perc + 1))) {
			replace_str = "";
			prefix_str = "";
			switch (*(perc + 1)) {
				case 'c':
					replace_str = app->entry_path;
					break;
				case 'k':
					replace_str = app->name;
					break;
				case 'i':
					if (strlen(app->icon) != 0) {
						replace_str = app->icon;
						prefix_str = "--icon ";
					}
					break;
			}
			prelen = strlen(prefix_str);
			replen = strlen(replace_str);
			totlen = prelen + replen;
			/* use memmove to deal with overlapping cases */
			memmove(perc + totlen, perc + 2, command + MLEN - perc - totlen);
			memmove(perc, prefix_str, prelen);
			memmove(perc + prelen, replace_str, replen);
		}
	}

	find_icon(app->icon, icon_path);
	if (!option.no_genname && strlen(app->genericname) > 0)
		sprintf(name, "%s (%s)", app->name, app->genericname);
	else
		strcpy(name, app->name);
	sprintf(entry->text, "\tIMG:%s\t%s\t%s", icon_path, name, command);
}

/*
 * handler for ini_parse
 * match subdirectories in an icon theme folder by parsing an index.theme file
 * - the icon size is options.icon_size
 * - the icon theme will be specified as the parsed the index.theme file
 */
int match_icon_subdir(void *user, const char *section, const char *name, const char *value) {
	/* static variables to preserve between function calls */
	static char subdir[32], type[16];
	static int size, minsize, maxsize, threshold, scale;

	if (strcmp(section, subdir) != 0 || (!name && !value)) {
		/* Check the icon size after finished parsing a section */
		if (scale == option.scale)
			if (((strlen(type) == 0 || strcmp(type, "Threshold") == 0)
					&& abs(size - option.icon_size) <= threshold)
				|| (strcmp(type, "Fixed") == 0
					&& size == option.icon_size)
				|| (strcmp(type, "Scalable") == 0
					&& minsize <= option.icon_size
					&& maxsize >= option.icon_size))
				printf("%s\n", subdir);

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

int get_gtk_icon_theme_handler(void *user, const char *section, const char *name, const char *value) {
	if (strcmp(section, "Settings") == 0 && strcmp(name, "gtk-icon-theme-name") == 0)
		strcpy(user, value);
	return 1;
}

void get_gtk_icon_theme(char *buffer, const char *fallback) {
	int res;
	char gtk3_settings[256] = {0}, *real_path;

	sprintf(gtk3_settings, "%s/gtk-3.0/settings.ini", XDG_CONFIG_HOME);
	if (access(gtk3_settings, F_OK) == 0) {
		real_path = realpath(gtk3_settings, NULL);
		if ((res = ini_parse(real_path, get_gtk_icon_theme_handler, buffer)) < 0)
			printf("failed parse gtk settings\n");
		free(real_path);
	}

	if (strlen(buffer) == 0)
		strcpy(buffer, fallback);
}

int main(int argc, char *argv[])
{
	/* FIXME: fallback values */
	PATH = getenv("PATH");
	HOME = getenv("HOME");
	XDG_DATA_HOME = getenv("XDG_DATA_HOME");
	XDG_DATA_DIRS = getenv("XDG_DATA_DIRS");
	XDG_CONFIG_HOME = getenv("XDG_CONFIG_HOME");
	XDG_CURRENT_DESKTOP = getenv("XDG_CURRENT_DESKTOP");

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

	int if_show_flag, res;
	App app;
	MenuEntry menuentry;
	char *folder = "/usr/share/applications", path[LLEN], icon_theme[64] = {0};
	DIR *dir;
	struct dirent *entry;

	/* output all app in folder */
	if ((dir = opendir(folder)) == NULL) {
		fprintf(stderr, "Open dir failed: %s\n", folder);
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strstr(entry->d_name, ".desktop") == NULL) {
			continue;
		}
		sprintf(path, "%s/%s", folder, entry->d_name);

		if_show_flag = 1;
		if ((res = ini_parse(path, if_show, &if_show_flag)) < 0)
			fprintf(stderr, "Desktop file parse failed: %d\n", res);

		memset(&app, 0, sizeof(App));
		strcpy(app.entry_path, path);
		if ((res = ini_parse(path, get_app, &app)) < 0)
			fprintf(stderr, "Desktop file parse failed: %d\n", res);

		gen_entry(&app, &menuentry);
		puts(menuentry.text);
	}

	if (dir)
		closedir(dir);

	char *index_theme = "/usr/share/icons/Adwaita/index.theme";
	if ((res = ini_parse(index_theme, match_icon_subdir, NULL)) < 0)
		fprintf(stderr, "Desktop file parse failed: %d\n", res);
	/* a hack to process the end of file */
	match_icon_subdir(NULL, "", NULL, NULL);

	get_gtk_icon_theme(icon_theme, "hicolor");
	printf("icon theme: (%s)\n", icon_theme);

	return 0;
}
