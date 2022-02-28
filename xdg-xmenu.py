#!/usr/bin/env python
# Author: Lu Xu <oliver_lew at outlook dot com>
# License: MIT
# References: https://specifications.freedesktop.org/desktop-entry-spec
#             https://specifications.freedesktop.org/icon-theme-spec

import os
import argparse
import configparser


def get_args():
    parser = argparse.ArgumentParser(description="A python rewrite of the shell version")
    parser.add_argument('-b', '--fallback-icon', default="application-x-executable",
                        help='Fallback icon for apps without icons')
    parser.add_argument('-i', '--icon-theme', default="Adwaita",
                        help='Icon theme for app icons')
    parser.add_argument('-s', '--icon-size', type=int, default=24,
                        help='Icon theme for app icons')
    parser.add_argument('-e', '--xdg-de', action='store_true',
                        help='Show apps according to desktop environments')
    parser.add_argument('-N', '--dry-run', action='store_true',
                        help='Do not run app, output to stdout')
    parser.add_argument('-t', '--terminal', metavar='terminal', default='xterm',
                        help='Terminal emulator to use, default is %(default)s')
    return parser.parse_args()


def valid_dirs(theme_dir):
    # Find directories in an icon theme matching the target size
    indexfile = f"{theme_dir}/index.theme"
    if not os.path.exists(indexfile):
        return []

    # parse the index.theme file
    entries = configparser.RawConfigParser()
    entries.read(indexfile)
    for subdir in entries.get("Icon Theme", "Directories").rstrip(",").split(","):
        # "Size" is required
        Size = entries.getint(subdir, "Size")
        # FIXME: scaled icons are ignored for now, sorry HiDPI
        if entries.getint(subdir, "Scale", fallback=1) != 1:
            continue

        # defaults if they are not specified
        Type = entries.get(subdir, "Type", fallback="Threshold")
        MaxSize = entries.getint(subdir, "MaxSize", fallback=Size)
        MinSize = entries.getint(subdir, "MinSize", fallback=Size)
        Threshold = entries.getint(subdir, "Threshold", fallback=2)

        # match subdirectory sizes based on 'Type'
        if ((Type == "Threshold" and abs(args.icon_size - Size) <= Threshold) or
                (Type == "Scalable" and MinSize <= args.icon_size <= MaxSize) or
                (Type == "Fixed" and Size == args.icon_size)):
            yield f"{theme_dir}/{subdir}"


def find_icon_dirs():
    # Find all directories matching the size and containing icons directly
    all_dirs = []
    for datadir in DATA_DIRS:
        for theme in [args.icon_theme, "hicolor"]:
            all_dirs.extend(valid_dirs(f"{datadir}/icons/{theme}"))
    all_dirs.append("/usr/share/pixmaps")
    return all_dirs


def find_icon(icon, fallback):
    # Search for an icon based on its name
    # find any png, svg or xpm icon in available directories
    for dir in icon_dirs:
        for ext in ["png", "svg", "xpm"]:
            icon_file = f"{dir}/{icon}.{ext}"
            if os.path.exists(icon_file):
                return icon_file
    # when the fallback icon is not found, return none instead
    return None if icon == fallback else fallback


def gen_entry(entry):
    Exec = entry.get('Exec')
    Icon = entry.get('Icon')
    Name = entry.get('Name')
    Terminal = entry.get('Terminal') == 'true'
    GenericName = entry.get('GenericName')
    Categories = entry.get('Categories')

    Exec = Exec.replace('%f', '') \
               .replace('%F', '') \
               .replace('%u', '') \
               .replace('%U', '') \
               .replace('%c', Name) \
               .replace('%k', entry.get('Path', ''))
    if Terminal:
        Exec = f"{args.terminal} -e {Exec}"

    Icon = find_icon(Icon, fallback)

    line = f"\tIMG:{Icon}\t{Name}({GenericName})\t{Exec}"

    for c in Categories.rstrip(";").split(";"):
        if categories.get(c):
            return categories[c]["name"], line
    return categories["Others"]["name"], line


def if_show(entry):
    if (entry.get('Exec') is None or
            entry.get('NoDisplay') == 'true' or
            entry.get('Hidden') == 'true' or
            entry.get('Type') != "Application"):
        return False

    if entry.get('TryExec'):
        TryExec = entry.get('TryExec')
        if os.path.isabs(TryExec):
            if os.path.exists(TryExec):
                return True
        else:
            for path in os.getenv("PATH"):
                if os.path.exists(os.path.join(path, TryExec)):
                    return True
        return False

    if not args.xdg_de:
        if entry.get('NotShowIn'):
            for DE in entry.get('NotShowIn').rstrip(';').split(';'):
                if current_desktops.find(DE) >= 0:
                    return False

        if entry.get('OnlyShowIn'):
            for DE in entry.get('OnlyShowIn').rstrip(';').split(';'):
                if current_desktops.find(DE) >= 0:
                    return True
            return False

    return True


def get_apps():
    apps = {}
    for data_dir in DATA_DIRS:
        app_dir = os.path.join(data_dir, 'applications')
        # Desktop entry files can be in nested directories
        for root, dirs, files in os.walk(app_dir):
            for name in files:
                if name.endswith('.desktop'):
                    app = os.path.join(root, name)
                    app_id = os.path.relpath(app, app_dir).replace('/', '-')

                    # Apps with the same id, choose the first
                    apps[app_id] = app
    return apps.values()


if __name__ == "__main__":
    args = get_args()
    current_desktops = os.getenv('XDG_CURRENT_DESKTOP', '')

    # XDG directories
    HOME = os.getenv("HOME")
    XDG_DATA_HOME = os.getenv("XDG_DATA_HOME", f"{HOME}/.local/share")
    XDG_DATA_DIRS = os.getenv("XDG_DATA_DIRS", "/usr/share:/usr/local/share").split(":")
    DATA_DIRS = [*XDG_DATA_DIRS, XDG_DATA_HOME]

    icon_dirs = find_icon_dirs()
    fallback = find_icon(args.fallback_icon, None)

    categories = {
        "AudioVideo": {"name": "Multimedia", "icon": "applications-multimedia"},
        "Audio": {"name": "Multimedia", "icon": "applications-multimedia"},
        "Video": {"name": "Multimedia", "icon": "applications-multimedia"},
        "Development": {"name": "Development", "icon": "applications-development"},
        "Education": {"name": "Education", "icon": "applications-education"},
        "Game": {"name": "Games", "icon": "applications-games"},
        "Graphics": {"name": "Graphics", "icon": "applications-graphics"},
        "Network": {"name": "Internet", "icon": "applications-internet"},
        "Office": {"name": "Office", "icon": "applications-office"},
        "Science": {"name": "Science", "icon": "applications-science"},
        "Settings": {"name": "Settings", "icon": "preferences-desktop"},
        "System": {"name": "System", "icon": "applications-system"},
        "Utility": {"name": "Accessories", "icon": "applications-accessories"},
        "Others": {"name": "Others", "icon": "applications-other"},
    }

    menu = {}
    for app in get_apps():
        app_parser = configparser.RawConfigParser()
        app_parser.read(app)
        entry = app_parser["Desktop Entry"]
        if if_show(entry):
            category, item = gen_entry(entry)
            if menu.get(category):
                menu[category].append(item)
            else:
                menu[category] = [item]

    for category in menu:
        icon = find_icon("applications-other", fallback)
        print(f"IMG:{icon}\t{category}")
        for item in menu[category]:
            print(item)
