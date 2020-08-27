# xdg-xmenu

`xdg-xmenu` is a small shell script to generate xdg desktop menu file for [xmenu](https://github.com/phillbush/xmenu), a simple x11 menu utility.

This script is only a toy project and I have not tested it outside of my own machine, so bugs are expected and feel free to report them here.

## Requirement

The script will look for icons according to [freedesktop specification](https://specifications.freedesktop.org). If the icons found are svg images, which is very possible, you will need any one of the following convert tools (sorted by priority):

- `rsvg-convert`: provided by `librsvg` package, it is the fastest one among the three.
- `convert`: provided by `imagemagick` package.
- `inkscape`: this is about as fast (or slow) as `convert`

The svg icons will be converted to png images and be cached to `~/.cache/xdg-xmenu/icons` folder. As a result, the first time running this script might cost some time.

## Usage

```
xdg-xmenu [-a] [-b fallback_font] [-i icon_theme] [-s icon_size] [-t terminal]

Options
  -a  Show icons with `OnlyShowIn' key in .desktop file.  These desktop entries are
      usually programs specifically for desktop environments.  Default is not show‐
      ing those desktop entries.

  -b fallback_icon
      Fallback  icon  in  case  one can not be found.  Both an icon name and a file
      path is acceptable.  Default is application-x-executable.

  -i icon_theme
      Icon theme.  Default is parsed from gtk3 configuration file.

  -s icon_size
      Icon size.  Default is 24.

  -t terminal
      Terminal emulator to use.  Default is xterm.
```

To use it, you can redirect the output to a file and then feed it to `xmenu` by

```sh
$ xdg-xmenu > menu; xmenu < menu | sh &
```

Piping directly to `xmenu` is not recommended due to the slow speed.

[xmenu-apps](xmenu-app) is an example script.

### Configuration

The command line options can also be replaced with environment variables:
- `XDG_MENU_FALLBACK_ICON`: set fallback icon
- `XDG_MENU_ICON_SIZE`: set icon size
- `XDG_MENU_ICON_THEME`: set icon theme
- `XDG_MENU_SHOW_ALL`: show 'OnlyShowIn' apps
- `XDG_MENU_TERMINAL`: set terminal emulator

## Screenshot

<img src="demo.gif" width="480px">
