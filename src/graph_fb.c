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
    int fdFb = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    struct mxcfb_gbl_alpha alpha;
    long int screensize = 0;
    char *fbBuf = 0;
    char *fbDev = "/dev/fb0";

    if (argc > 1) {
        fbDev = argv[1];
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
    printf("  TRN: offset=%u, length=%u, msb_right=%u\n", vinfo.transp.offset, vinfo.transp.length, vinfo.transp.msb_right);

    // Calculate the size of the screen in bytes
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    printf("Screen size in bytes: %ld\n", screensize);

    // Map the framebuffer to memory
    fbBuf = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fdFb, 0);
    if ((intptr_t)fbBuf == -1) {
        printf("Error: failed to map framebuffer device to memory. Error(%d): %m", errno);
        return 4;
    }

    // Clear the screen by setting all pixels to black
    memset(fbBuf, 0, screensize);

    // Disable global alpha since we need Pixel Alpha
    alpha.enable = 0;
    alpha.alpha = 0xff;
    ioctl(fdFb, MXCFB_SET_GBL_ALPHA, &alpha);

    // Unmap the framebuffer and close the device file
    munmap(fbBuf, screensize);
    close(fdFb);

  return 0;
}   