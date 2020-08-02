# xdg-xmenu

`xdg-xmenu` is a small shell script to generate xdg desktop menu file for [xmenu](https://github.com/phillbush/xmenu), a simple x11 menu utility.

## Requirement

If the icons found are svg images, which is very possible, you will need any one of the following convert tools (sorted by priority):

- `rsvg-convert`: provided by `librsvg` package, it is the fastest one among the three.
- `convert`: provided by `imagemagick` package.
- `inkscape`: this is about as fast (or slow) as `convert`

## Usage

The script can be simply invoked with

```sh
$ xdg-xmenu > menu
```

Then feed it to `xmenu` by

```sh
$ xmenu < menu | sh &
```

[xmenu-apps](xmenu-app) is an example script.

The configurations of this script is controlled by environment variables.

- **TERMINAL**:
  Terminal emulator to use.  Default is xterm if not specified.

- **ICON_SIZE**:
  Icon  size  (should match the sizes in icon theme folders).  Default is 24 if
  not specified.

- **ICON_THEME**:
  Icon theme (as in the icon folder name).  Default is  gtk3  settings  if  not
  specified.

- **FALLBACK_ICON**:
  Fallback  icon  in  case  one can not be found.  Both an icon name and a file
  path is acceptable.  Default is application-x-executable.

## Screenshot

<img src="demo.gif" width="480px">
