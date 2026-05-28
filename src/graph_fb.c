#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>

char userColor[16] = {0};
char *fbDev = "/dev/fb0";
int fdFb = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
long int screenSize = 0;
unsigned short pixelSize = 0;
char *fbBuf = 0;

int solid_fill() {
    long i;
    unsigned short bit;

    for (i = 0; i < screenSize; i+=pixelSize) {
        for (bit = 0; bit < pixelSize; bit++) {
            fbBuf[i + bit]= userColor[bit];
        }
    }
}

int rainbow_fill() {
    size_t bit, i;

    // row Rainbow
    for (size_t x = 0; x < vinfo.xres; x++) {
        for (size_t y = 0; y < vinfo.yres; y++) {
            size_t offset = (x + y * vinfo.xres) * pixelSize;
            size_t colorIx = x / (vinfo.xres/7);
            char pixel[pixelSize];
            switch (colorIx) {
                case 0: // Red
                    pixel[0] = 0b00000000;
                    pixel[1] = 0b11111000;
                    break;
                case 1: // Orange
                    pixel[0] = 0b11100010;
                    pixel[1] = 0b11111100;
                    break;
                case 2: // Yellow
                    pixel[0] = 0b11100000;
                    pixel[1] = 0b11111111;
                    break;
                case 3: // Green
                    pixel[0] = 0b11100000;
                    pixel[1] = 0b00000111;
                    break;
                case 4: // Cyan
                    pixel[0] = 0b11111111;
                    pixel[1] = 0b00000111;
                    break;
                case 5: // Blue
                    pixel[0] = 0b00011111;
                    pixel[1] = 0b00000000;
                    break;
                case 6: // Purple
                    pixel[0] = 0b00011111;
                    pixel[1] = 0b10011001;
                    break;
                default: // Black
                    pixel[0] = 0b00000000;
                    pixel[1] = 0b00000000;
                    break;
            }
            fbBuf[offset] = pixel[0];
            fbBuf[offset+1] = pixel[1];
        }
    }
}

int parse_args(int argc, char *argv[]) {
    size_t i, len, bit;
    int mode = -1;

    for(i = 1; i < argc; i++) {
        if (argv[i][0] == '0' || argv[i][0] == '1') {
            len = strlen(argv[i]);
            mode = 1;
            for (bit = 0; bit < len && bit < 16 * 8; bit++) {
                if (argv[i][bit] == '1') {
                    userColor[bit/8] |= (1 << (7 - (bit % 8)));
                }
            }
            printf("User first 2 colors(HEX): %02x %02x\n", userColor[0], userColor[1]);
        } else if (argv[i][0] == '/') {
            fbDev = argv[i];
        } else if (argv[i][0] == 'r') {
            mode = 2;
        } else if (argv[i][0] == 'c') {
            mode = 0; // clear
        } else if (argv[i][0] == 'h') {
            printf("Usage %s [FB] [BITS] [r] [c] [h]\n"
                "Possible arguments\n"
                "  FB       framebuffer device /dev/fbX\n"
                "  BITS     user color in binary format: 101011111000111000111 (up to 16 bytes)\n"
                "  r        display rainbow\n"
                "  c        clear display\n"
                "  h        show this help\n"
            );
            exit(0);
        } else {
            printf("Unknown argument: %s\n", argv[i]);
        }
    }
    return mode;
}

int main(int argc, char *argv[]) {
    size_t i, len, bit;
    int mode = parse_args(argc, argv);

    // Open the framebuffer device file
    fdFb = open(fbDev, O_RDWR);
    if (fdFb == -1) {
        printf("Cannot open device %s. Error(%d): %m", fbDev, errno);
        return 1;
    }

    // Get fixed screen information
    if (ioctl(fdFb, FBIOGET_FSCREENINFO, &finfo) == -1) {
        printf("Error reading fixed information from device %s. Error(%d): %m", fbDev, errno);
        return 2;
    }
    printf("Screen: %s\n", finfo.id);
    printf("Memory: %u\n", finfo.smem_len);
    printf("  Line: %u\n", finfo.line_length);

    // Get variable screen information
    if (ioctl(fdFb, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        printf("Error reading variable information from device %s. Error(%d): %m", fbDev, errno);
        return 3;
    }
    printf("  Size: %ux%u\n", vinfo.xres, vinfo.yres);
    printf(" Pixel: %u\n", vinfo.bits_per_pixel);
    printf("  RGBA: %u/%u%s, %u/%u%s, %u/%u%s, %u/%u%s"
        , vinfo.red.offset, vinfo.red.length, vinfo.red.msb_right ? "(REV)" : ""
        , vinfo.green.offset, vinfo.green.length, vinfo.green.msb_right ? "(REV)" : ""
        , vinfo.blue.offset, vinfo.blue.length, vinfo.blue.msb_right ? "(REV)" : ""
        , vinfo.transp.offset, vinfo.transp.length, vinfo.transp.msb_right ? "(REV)" : "");

    // Calculate the size of the screen in bytes
    screenSize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    pixelSize = vinfo.bits_per_pixel / 8;
    printf(" Bytes: %ld\n", screenSize);
    printf(" Pixel: %hd\n", pixelSize);

    if (pixelSize == 0) {
        printf("Error: bits_per_pixel is zero, cannot calculate pixel size.");
        return 5;
    }

    // Disable screen blanking (keep it always on)
    ioctl(fdFb, FBIOBLANK, VESA_NO_BLANKING);


    // Map the framebuffer to memory
    fbBuf = (char *)mmap(0, screenSize, PROT_READ | PROT_WRITE, MAP_SHARED, fdFb, 0);
    if ((intptr_t)fbBuf == -1) {
        printf("Error: failed to map framebuffer device to memory. Error(%d): %m", errno);
        return 4;
    }

    // Clear the screen by setting all pixels to black
    memset(fbBuf, 0, screenSize);

    switch(mode) {
        case 0:
            break;
        case 1:
            solid_fill();
            break;
        case 2:
            rainbow_fill();
            break;
    }

    // Unmap the framebuffer and close the device file
    munmap(fbBuf, screenSize);
    close(fdFb);

    return 0;
}