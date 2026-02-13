# PN532 Card reader utility
RaspberryPi NFC Board reader based on Waveshare PN532 library.
Currently supports reading MiFare/ISO14443A cards data.

## Requirements
- Libarary [Wiring PI](https://github.com/WiringPi/WiringPi)
- Any ARM64 board compatible with WiringPI (Raspberry Pi boards are better)
- Supported build systems: [Meson](https://mesonbuild.com/) or [make](https://linux.die.net/man/1/make)

## How to use

### To build with meson:
```bash
meson setup build
ninja -C build
build/reader
```

### To build with make:
```bash
make
./reader
```

### Commandline options
```bash
reader -v -q -x -k ffffffffffff -s 0 -e 63 -b 1-3,5-8
#where
 -v, --verbose     - Increase debug level +1
 -q, --quiet       - Minimal debug level
 -x, --extended    - Extended logs with file name, line number, function name
 -k, --key KEY_A   - Custom 6-bytes Key_A in hex format (default is FFFFFFFFFFFF)
 -s, --start 0     - Start block for read (default 0)
 -e, --end 63      - End block for read (default 63)
 -b, --blocks 1-3  - List blocks for read, overrides -s and -e if specified, (default is `start`-`end` [0-63])
```
