# Zoomify, yet another zoomer application

Yet another zoomer application on Linux & macOS implemented in [raylib](https://github.com/raysan5/raylib)
with some personal needs that inspired by [Boomer](https://github.com/tsoding/boomer).

## Dependencies

### Linux

libx11

### macOS

macOS 14.0+, Xcode, Xcode Command Line Tool

## How to use

### Linux & macOS

```sh
# This will build zoomify & cp the executable file to `/usr/local/bin/`, if don't
# wanna do that, you can just run `make BUILD_MODE=RELEASE`, and the executable
# file will be located at `build/zoomify` (Linux) or `build/Release/zoomify` (macOS)
make install
zoomify
```

### Windows

Not implement yet.

## Keybinds

| key                           | description                              |
| :---------------------------- | :--------------------------------------- |
| Drag with left mouse button   | Move screenshot around                   |
| Scroll mouse wheel            | Zoom in & out or change spotlight radius |
| <kbd>l</kbd>                  | Toggle spotlight                         |
| <kbd>h</kbd>                  | toggle keystroke tips                    |
| <kbd>ESC</kbd>                | Quit Zoomify                             |

## TODO

- [x] Basic functionality (zoom in & out, toggle spotlight)
- [ ] Options by command line parameters
- [ ] Multiscreen support
- [ ] Draw on canvas
- [ ] Save the selected screenshot as an image
- [ ] Windows support

## License

MIT License

## References

[Boomer](https://github.com/tsoding/boomer)