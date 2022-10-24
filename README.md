# xdg-xmenu

The current branch is a C rewrite of previous shell and python version of `xdg-xmenu`. It's a small command to generate xdg desktop menu for [xmenu](https://github.com/phillbush/xmenu), a simple x11 menu utility. This C version can launch xmenu directly, due to a faster execution time.

**Important:** Since Imlib2 1.8.0, svg icons are supported. Thus, `xdg-xmenu` assumes that you have installed Imlib2 at least that version. As a result, the svg icons are not converted to png anymore, unlike the shell version. If you don't have the required version of Imlib2, use the shell version instead.

For the legacy shell and python versions, see corresponding branches of this git repository.

## Usage

```
xdg-xmenu [-dGhIn] [-b ICON] [-i THEME] [-s SIZE] [-S SCALE] [-t TERMINAL] [-x CMD] [-- <xmenu_args>]

Generate XDG menu for xmenu.

Options:
  -h          Show this help message and exit
  -b ICON     Fallback icon name, default is application-x-executable
  -d          Dump generated menu, do not run xmenu
  -G          Do not show generic name of the app
  -i THEME    Icon theme for app icons. Default to gtk3 settings
  -I          Disable icon in xmenu
  -n          Do not run app, output to stdout
  -s SIZE     Icon theme for app icons
  -S SCALE    Icon size scale factor, work with HiDPI screens
  -t TERMINAL Terminal emulator to use, default is xterm
  -x CMD      Xmenu command to use, default is xmenu
Note:
  Options after `--' are passed to xmenu
```
