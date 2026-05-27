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

struct mxcfb_gbl_alpha {
	int enable;
	int alpha;
};
#define MXCFB_SET_GBL_ALPHA     _IOW('F', 0x21, struct mxcfb_gbl_alpha)

int main(int argc, char *argv[]) {
    int fdFb = 0, solidColor = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    struct mxcfb_gbl_alpha alpha;
    long int screenSize = 0;
    unsigned short pixelSize = 0;
    char *fbBuf = 0;
    char *fbDev = "/dev/fb0";
    char userColor[16] = {0};
    size_t len, bit, i;

    for(i = 1; i < argc; i++) {
        if (argv[i][0] == '0' || argv[i][0] == '1') {
            len = strlen(argv[i]);
            solidColor = 1;
            for (bit = 0; bit < len && bit < 16 * 8; bit++) {
                if (argv[i][bit] == '1') {
                    userColor[bit/8] |= (1 << (7 - (bit % 8)));
                }
            }
            printf("User first 2 colors(HEX): %02x %02x\n", userColor[0], userColor[1]);
        } else if (argv[i][0] == '/') {
            fbDev = argv[i];
        } else {
            printf("Unknown argument: %s\n", argv[i]);
        }
    }

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
    printf("Fixed screen info:\n");
    printf("  id: %s\n", finfo.id);
    printf("  smem_len: %u\n", finfo.smem_len);
    printf("  type: %u\n", finfo.type);
    printf("  type_aux: %u\n", finfo.type_aux);
    printf("  visual: %u\n", finfo.visual);
    printf("  xpanstep: %u\n", finfo.xpanstep);
    printf("  ypanstep: %u\n", finfo.ypanstep);
    printf("  ywrapstep: %u\n", finfo.ywrapstep);
    printf("  line_length: %u\n", finfo.line_length);

    // Disable global alpha since we need Pixel Alpha
    // alpha.enable = 0;
    // alpha.alpha = 0xff;
    // ioctl(fdFb, MXCFB_SET_GBL_ALPHA, &alpha);


    // Get variable screen information
    if (ioctl(fdFb, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        printf("Error reading variable information from device %s. Error(%d): %m", fbDev, errno);
        return 3;
    }
    printf("Variable screen info:\n");
    printf("  RES: %ux%u\n", vinfo.xres, vinfo.yres);
    printf("  bpp: %u\n", vinfo.bits_per_pixel);
    printf("  RED: offset=%u, length=%u, msb_right=%u\n", vinfo.red.offset, vinfo.red.length, vinfo.red.msb_right);
    printf("  GRN: offset=%u, length=%u, msb_right=%u\n", vinfo.green.offset, vinfo.green.length, vinfo.green.msb_right);
    printf("  BLU: offset=%u, length=%u, msb_right=%u\n", vinfo.blue.offset, vinfo.blue.length, vinfo.blue.msb_right);
    printf("  ALP: offset=%u, length=%u, msb_right=%u\n", vinfo.transp.offset, vinfo.transp.length, vinfo.transp.msb_right);

    // Calculate the size of the screen in bytes
    screenSize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    pixelSize = vinfo.bits_per_pixel / 8;
    printf("Screen size in bytes: %ld\n", screenSize);
    printf("Pixel size in bytes: %hd\n", pixelSize);

    if (pixelSize == 0) {
        printf("Error: bits_per_pixel is zero, cannot calculate pixel size.");
        return 5;
    }

    // Map the framebuffer to memory
    fbBuf = (char *)mmap(0, screenSize, PROT_READ | PROT_WRITE, MAP_SHARED, fdFb, 0);
    if ((intptr_t)fbBuf == -1) {
        printf("Error: failed to map framebuffer device to memory. Error(%d): %m", errno);
        return 4;
    }

    // Clear the screen by setting all pixels to black
    memset(fbBuf, 0, screenSize);

    // Disable screen blanking (keep it always on)
    ioctl(fdFb, FBIOBLANK, VESA_NO_BLANKING);

    if (solidColor == 1) {
        printf("Put solid color in %ld buffer by %hd bytes:", screenSize, pixelSize);
        for (bit = 0; bit < pixelSize; bit++) {
            printf(" %02x", bit, userColor[bit]);
        }
        printf("\n");
        for (i = 0; i < screenSize; i+=pixelSize) {
            for (bit = 0; bit < pixelSize; bit++) {
                // size_t off = i + bit;
                // printf("[%ld]=%02x ", i + bit, userColor[bit]);
                fbBuf[i + bit]= userColor[bit];
            }
        }
    } else {
        printf("Print rainbow\n");
        // row Rainbow
        for (size_t x = 0; x < vinfo.xres; x++) {
            for (size_t y = 0; y < vinfo.yres; y++) {
                size_t offset = (x + y * vinfo.xres) * pixelSize;
                size_t colorIx = x / (vinfo.xres/7);
                char pixel[pixelSize];
                switch (colorIx) {
                    case 0: // Red
                        pixel[0] = 0b00000000;
                        pixel[1] = 0b00011111;
                        break;
                    case 1: // Orange
                        pixel[0] = 0b00010100;
                        pixel[1] = 0b11111111;
                        break;
                    case 2: // Yellow
                        pixel[0] = 0b00000111;
                        pixel[1] = 0b11111111;
                        break;
                    case 3: // Green
                        pixel[0] = 0b00000111;
                        pixel[1] = 0b11100000;
                        break;
                    case 4: // Cyan
                        pixel[0] = 0b11111111;
                        pixel[1] = 0b11100000;
                        break;
                    case 5: // Blue
                        pixel[0] = 0b11111000;
                        pixel[1] = 0b00000000;
                        break;
                    case 6: // Purple
                        pixel[0] = 0b11111001;
                        pixel[1] = 0b00010011;
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

    // Unmap the framebuffer and close the device file
    munmap(fbBuf, screenSize);
    close(fdFb);

    return 0;
}