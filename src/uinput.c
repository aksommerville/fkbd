#include "fkbd.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/uinput.h>

/* Keycodes.
 * We'll report the virtual device as capable of all of them, so it looks like a keyboard.
 * Even though only a small subset will actually be used.
 */
#define KEYCODE_FIRST 1 /* KEY_A */
#define KEYCODE_LAST 111 /* KEY_DELETE. Everything above looks exotic. */

/* Close connections.
 */
 
void uinput_close() {
  if (g.ui_key_fd>=0) {
    close(g.ui_key_fd);
    g.ui_key_fd=-1;
  }
  if (g.ui_mouse_fd>=0) {
    close(g.ui_mouse_fd);
    g.ui_mouse_fd=-1;
  }
}

/* Open one output.
 */
 
static int uinput_open_one(char which) {
  int fd=open("/dev/uinput",O_RDWR);
  if (fd<0) {
    fprintf(stderr,"/dev/uinput: %m\n");
    return -1;
  }
  struct uinput_setup setup={
    .id={
      .bustype=BUS_VIRTUAL,
      .vendor=0xffbd,
      .product=(which=='m')?0x0002:0x0001,
      .version=0x0200,
    },
    .name="fkbd Fake Keyboard",
    .ff_effects_max=0,
  };
  if (ioctl(fd,UI_DEV_SETUP,&setup)<0) {
    fprintf(stderr,"/dev/uinput:UI_DEV_SETUP: %m\n");
    close(fd);
    return -1;
  }
  if (ioctl(fd,UI_SET_EVBIT,EV_KEY)<0) {
    fprintf(stderr,"/dev/uinput:UI_SET_EVBIT(EV_KEY): %m\n");
    close(fd);
    return -1;
  }
  switch (which) {
    case 'k': {
        int keycode=KEYCODE_FIRST;
        for (;keycode<=KEYCODE_LAST;keycode++) {
          if (ioctl(fd,UI_SET_KEYBIT,keycode)<0) {
            fprintf(stderr,"/dev/uinput:UI_SET_KEYBIT(%d): %m\n",keycode);
            close(fd);
            return -1;
          }
        }
      } break;
    case 'm': {
        if (
          (ioctl(fd,UI_SET_EVBIT,EV_REL)<0)|| /* We won't send these, but we might need them in order to look like a mouse. */
          (ioctl(fd,UI_SET_RELBIT,REL_X)<0)||
          (ioctl(fd,UI_SET_RELBIT,REL_Y)<0)||
          (ioctl(fd,UI_SET_KEYBIT,BTN_LEFT)<0)|| // These three, we may send.
          (ioctl(fd,UI_SET_KEYBIT,BTN_RIGHT)<0)||
          (ioctl(fd,UI_SET_KEYBIT,BTN_MIDDLE)<0)
        ) {
          fprintf(stderr,"/dev/uinput:UI_SET_KEYBIT: %m\n");
          close(fd);
          return -1;
        }
      } break;
  }
  if (ioctl(fd,UI_DEV_CREATE)<0) {
    fprintf(stderr,"/dev/uinput:UI_DEV_CREATE: %m\n");
    close(fd);
    return -1;
  }
  return fd;
}

/* Open connection.
 */
 
int uinput_open() {
  if ((g.ui_key_fd>=0)||(g.ui_mouse_fd>=0)) return -1;
  int need_key=0,need_mouse=0;
  int i=32; while (i-->0) {
    if (g.dstmap[i]>=BTN_MISC) need_mouse=1;
    else if (g.dstmap[i]>0) need_key=1;
  }
  if (need_key) {
    if ((g.ui_key_fd=uinput_open_one('k'))<0) return -1;
  }
  if (need_mouse) {
    if ((g.ui_mouse_fd=uinput_open_one('m'))<0) return -1;
  }
  return 0;
}
