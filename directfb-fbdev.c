/*
  DirectFB-fbdev      DirectFB wrapper for Linux Framebuffer applications
  Copyright (C) 2014  Nicolas Caramelli

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <directfb.h>
#include <linux/fb.h>

static int fb_fd = -1;
static void *fb_addr = NULL;
static IDirectFB *dfb;
static IDirectFBSurface *primary;

static int (*_open)(const char *file, int oflag, ...) = NULL;
static int (*_close)(int fd) = NULL;
static int (*_ioctl)(int fd, unsigned long request, ...) = NULL;
static void *(*_mmap)(void *addr, size_t len, int prot, int flags, int fd, off_t offset) = NULL;
static int (*_munmap)(void *addr, size_t len) = NULL;

int open(const char *file, int oflag, ...)
{
  if (!_open)
    _open = dlsym(RTLD_NEXT, "open");

  if (fb_fd == -1 && strstr(file, "/dev/fb")) {
    fb_fd = open("/dev/null", oflag);
    DirectFBInit(NULL, NULL);
    DirectFBSetOption("no-vt", NULL);
    DirectFBCreate(&dfb);
    dfb->SetCooperativeLevel(dfb, DFSCL_FULLSCREEN);
    DFBSurfaceDescription dsc;
    dsc.flags = DSDESC_CAPS;
    dsc.caps = DSCAPS_PRIMARY;
    dfb->CreateSurface(dfb, &dsc, &primary);
    return fb_fd;
  }
  else
    return _open(file, oflag);
}

int ioctl(int fd, unsigned long request, ...)
{
  va_list args;
  void *arg;

  if (!_ioctl)
    _ioctl = dlsym(RTLD_NEXT, "ioctl");

  va_start(args, request);
  arg = va_arg(args, void *);
  va_end(args);

  if (fd == fb_fd) {
    int width, height;
    DFBSurfacePixelFormat fmt;
    if (request == FBIOGET_VSCREENINFO || request == FBIOGET_FSCREENINFO) {
      primary->GetSize(primary, &width, &height);
      primary->GetPixelFormat(primary, &fmt);
    }

    switch (request) {
      case FBIOGET_VSCREENINFO:
      {
        struct fb_var_screeninfo *fb_var = arg;
        memset(fb_var, 0, sizeof(struct fb_var_screeninfo));
        fb_var->xres = fb_var->xres_virtual = width;
        fb_var->yres = fb_var->yres_virtual = height;
        switch (fmt) {
          case DSPF_RGB16:
            fb_var->bits_per_pixel = 16;
            fb_var->red.offset = 11;
            fb_var->green.offset = 5;
            fb_var->red.length = fb_var->blue.length = 5;
            fb_var->green.length = 6;
            break;
          case DSPF_RGB24:
            fb_var->bits_per_pixel = 24;
            fb_var->red.offset = 16;
            fb_var->green.offset = 8;
            fb_var->red.length = fb_var->green.length = fb_var->blue.length = 8;
            break;
          case DSPF_RGB32:
            fb_var->bits_per_pixel = 32;
            fb_var->transp.offset = 24;
            fb_var->red.offset = 16;
            fb_var->green.offset = 8;
            fb_var->transp.length = fb_var->red.length = fb_var->green.length = fb_var->blue.length = 8;
            break;
          default:
            errno = EINVAL;
            return -1;
        }
        break;
      }
      case FBIOGET_FSCREENINFO:
      {
        struct fb_fix_screeninfo *fb_fix = arg;
        memset(fb_fix, 0, sizeof(struct fb_fix_screeninfo));
        fb_fix->visual = FB_VISUAL_TRUECOLOR;
        switch (fmt) {
          case DSPF_RGB16:
            fb_fix->smem_len = 2 * width * height;
            fb_fix->line_length = 2 * width;
            break;
          case DSPF_RGB24:
            fb_fix->smem_len = 3 * width * height;
            fb_fix->line_length = 3 * width;
            break;
          case DSPF_RGB32:
            fb_fix->smem_len = 4 * width * height;
            fb_fix->line_length = 4 * width;
            break;
          default:
            errno = EINVAL;
            return -1;
        }
        break;
      }
      default:
        break;
    }
    return 0;
  }
  else
    return _ioctl(fd, request, arg);
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
  if (!_mmap)
    _mmap = dlsym(RTLD_NEXT, "mmap");

  if (fd == fb_fd) {
    int pitch;
    primary->Lock(primary, DSLF_WRITE, &fb_addr, &pitch);
    primary->Unlock(primary);
    return fb_addr;
  }
  else
    return _mmap(addr, len, prot, flags, fd, offset);
}

int munmap(void *addr, size_t len)
{
  if (!_munmap)
    _munmap = dlsym(RTLD_NEXT, "munmap");

  if (addr == fb_addr) {
    fb_addr = NULL;
    return 0;
  }
  else
    return _munmap(addr, len);
}

int close(int fd)
{
  if (!_close)
    _close = dlsym(RTLD_NEXT, "close");

  if (fd == fb_fd) {
    primary->Release(primary);
    dfb->Release(dfb);
    fb_fd = -1;
    return 0;
  }
  else
    return _close(fd);
}
