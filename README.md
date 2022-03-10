# xdg-xmenu

The current branch is a Python rewrite of previous shell version of `xdg-xmenu`. It's a small python script to generate xdg desktop menu for [xmenu](https://github.com/phillbush/xmenu), a simple x11 menu utility. This Python version can also launch xmenu directly, due to a little faster execution time.

**Important:** Since Imlib2 1.8.0, svg icons are supported. The python version assumes that you have installed the latest Imlib2. As a result, the svg icons are not converted to png anymore, unlike the shell version. If you don't have the latest Imlib2, use the shell version instead.

The shell version is in `shell` branch of the git repository.

## Usage

```
xdg-xmenu [-h] [-b FALLBACK_ICON] [-d] [-i ICON_THEME] [-I]
          [-s ICON_SIZE] [-e] [-n] [-t TERMINAL] [-x XMENU_CMD]

options:
  -h, --help            show this help message and exit
  -b,--fallback-icon FALLBACK_ICON
                        Fallback icon for apps without icons, default is
                        application-x-executable
  -d, --dump            Dump generated menu, do not run xmenu
  -i,--icon-theme ICON_THEME
                        Icon theme for app icons. If not specified, default
                        gtk3 settings file (~/.config/gtk-3.0/settings.ini)
                        will be searched for gtk icon theme.
  -I, --no-icon         Disable icon in xmenu.
  -s,--icon-size ICON_SIZE
                        Icon theme for app icons
  -e, --xdg-de          Show apps according to desktop environments
  -n, --dry-run         Do not run app, output to stdout
  -t,--terminal TERMINAL
                        Terminal emulator to use, default is xterm
  -x,--xmenu-cmd XMENU_CMD
                        Xmenu command to use, default is "xmenu"
```
