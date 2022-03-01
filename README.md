# xdg-xmenu

The current branch is a Python rewrite of previous shell version of `xdg-xmenu`. It's a small python script to generate xdg desktop menu for [xmenu](https://github.com/phillbush/xmenu), a simple x11 menu utility. This Python version can also launch xmenu directly, due to a little faster execution time.

**Important:** Since Imlib2 1.8.0, svg icons are supported (but currently renders at low resolution). The python version assumes that you have installed the latest Imlib2. As a result, the svg icons are not converted to png anymore, unlike the shell version. If you don't have the latest Imlib2, use the shell version instead.

The shell version is in `shell` branch of the git repository.

## Usage

```
xdg-xmenu [-b fallback_icon] [-i icon_theme] [-n] [-s icon_size] [-t terminal]

Options
  -b fallback_icon
      Fallback icon in case one can not be found. Both an icon name and a file
      path is acceptable. Default is application-x-executable.

  -i icon_theme
      Icon theme. Default is parsed from gtk3 configuration file.

  -n  Dry run mode. The conversion of svg images will be skipped. In others words,
      there is no file operations. The menu will still be printed with all the
      icon paths.

  -s icon_size
      Icon size. Default is 24.

  -t terminal
      Terminal emulator to use. Default is xterm.
```
