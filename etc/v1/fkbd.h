#ifndef FKBD_H
#define FKBD_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <linux/uinput.h>

#define FKBD_MAP_LIMIT 64

struct fkbd_map {
  int srctype,srccode;
  int dstbtnid;
  int srclo,srchi;
  int srcvalue,dstvalue;
  int mode; // 2,3,4 = two-state, three-way, hat
};

extern struct fkbd {
  volatile int sigc;
  const char *exename;
  int uifd;
  const char *evdev_path;
  int evfd;
  uint16_t state; // Mapped source device.
  uint16_t pvstate;
  int tattle; // If nonzero, don't open uinput and just dump evdev events.
  struct fkbd_map mapv[FKBD_MAP_LIMIT];
  int mapc;
  uint16_t dstmap[16]; // KEY_ codes indexed by the bit index of (state).
} fkbd;

int fkbd_uinput_open();
int fkbd_uinput_event(int code,int value);
int fkbd_uinput_update(); // Fire whatever events are needed according to (state,pvstate).
int fkbd_uinput_map(const char *name);
void fkbd_uinput_log_map_names();

int fkbd_evdev_open();
void fkbd_evdev_update(int type,int code,int value); // Updates (fkbd.state) accordingly.

/* Source evdev devices map to an abbreviated Standard Gamepad.
 * Not bothering to name the analogue sticks (or to decide whether they should quantize before or after this mapping).
 */
#define FKBD_BTN_LEFT      0x0001
#define FKBD_BTN_RIGHT     0x0002
#define FKBD_BTN_UP        0x0004
#define FKBD_BTN_DOWN      0x0008
#define FKBD_BTN_SOUTH     0x0010
#define FKBD_BTN_WEST      0x0020
#define FKBD_BTN_EAST      0x0040
#define FKBD_BTN_NORTH     0x0080
#define FKBD_BTN_L1        0x0100
#define FKBD_BTN_R1        0x0200
#define FKBD_BTN_L2        0x0400
#define FKBD_BTN_R2        0x0800
#define FKBD_BTN_AUX1      0x1000
#define FKBD_BTN_AUX2      0x2000
#define FKBD_BTN_AUX3      0x4000

#endif
