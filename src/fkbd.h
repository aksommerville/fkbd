/* fkbd.h
 * Fake Keyboard, version 2.
 * This will be considerably more advanced than v1. v1 is preserved under etc/ if we want to go back.
 * Changes from v1:
 *  - HTTP server.
 *  - Monitor /dev/input with inotify.
 *  - - Actually no. There won't be live updating in the web app, just a Refresh button. So no need to monitor.
 *  - Cheesy web app to monitor devices.
 *  - Map to mouse buttons in addition to keyboard, for those fucking obnoxious games that require a mouse click.
 *  - - Note that if the mouse is needed as a pointer, we're not going to try faking that. Just use the mouse.
 *  - Set up mapping on the fly.
 * I think we're still going to operate on just one input device at a time, why would we ever need more?
 */

#ifndef FKBD_H
#define FKBD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include "serial/serial.h"
#include "fs.h"

// Generic intermediate device state.
#define FKBD_BTN_LEFT     0x00000001 /* dpad... */
#define FKBD_BTN_RIGHT    0x00000002
#define FKBD_BTN_UP       0x00000004
#define FKBD_BTN_DOWN     0x00000008
#define FKBD_BTN_SOUTH    0x00000010 /* thumbs... */
#define FKBD_BTN_WEST     0x00000020
#define FKBD_BTN_EAST     0x00000040
#define FKBD_BTN_NORTH    0x00000080
#define FKBD_BTN_L1       0x00000100 /* triggers... */
#define FKBD_BTN_R1       0x00000200
#define FKBD_BTN_L2       0x00000400
#define FKBD_BTN_R2       0x00000800
#define FKBD_BTN_AUX1     0x00001000 /* start */
#define FKBD_BTN_AUX2     0x00002000 /* select */
#define FKBD_BTN_AUX3     0x00004000 /* heart */
// One bit available here.
#define FKBD_BTN_LP       0x00010000 /* left plunger */
#define FKBD_BTN_RP       0x00020000 /* right plunger */
// Two bits available here.
#define FKBD_BTN_LL       0x00100000 /* left stick, quantized... */
#define FKBD_BTN_LR       0x00200000
#define FKBD_BTN_LU       0x00400000
#define FKBD_BTN_LD       0x00800000
#define FKBD_BTN_RL       0x01000000 /* right stick, quantized... */
#define FKBD_BTN_RR       0x02000000
#define FKBD_BTN_RU       0x04000000
#define FKBD_BTN_RD       0x08000000
// Four bits available here.

#define FKBD_BTNIX_LEFT     0
#define FKBD_BTNIX_RIGHT    1
#define FKBD_BTNIX_UP       2
#define FKBD_BTNIX_DOWN     3
#define FKBD_BTNIX_SOUTH    4
#define FKBD_BTNIX_WEST     5
#define FKBD_BTNIX_EAST     6
#define FKBD_BTNIX_NORTH    7
#define FKBD_BTNIX_L1       8
#define FKBD_BTNIX_R1       9
#define FKBD_BTNIX_L2      10
#define FKBD_BTNIX_R2      11
#define FKBD_BTNIX_AUX1    12
#define FKBD_BTNIX_AUX2    13
#define FKBD_BTNIX_AUX3    14
// Skip one.
#define FKBD_BTNIX_LP      16
#define FKBD_BTNIX_RP      17
// Skip two.
#define FKBD_BTNIX_LL      20
#define FKBD_BTNIX_LR      21
#define FKBD_BTNIX_LU      22
#define FKBD_BTNIX_LD      23
#define FKBD_BTNIX_RL      24
#define FKBD_BTNIX_RR      25
#define FKBD_BTNIX_RU      26
#define FKBD_BTNIX_RD      27
// Skip four.

extern struct g {
  const char *exename;
  volatile int sigc;
  const char *htdocs;
  const char *evdev_devices_path;
  
  int http_port;
  int http_server; // fd
  
  struct http_client {
    int fd;
    char *rbuf;
    int rbufp,rbufc,rbufa;
    char *wbuf;
    int wbufp,wbufc,wbufa;
  } *http_clientv;
  int http_clientc,http_clienta;
  
  struct evdev_device {
    char *path;
    char *name;
    struct input_id id;
    uint8_t btnbit[(KEY_MAX+1)>>3];
    uint8_t absbit[(ABS_MAX+1)>>3];
    struct input_absinfo absinfo[ABS_MAX+1];
  } *evdev_devicev;
  int evdev_devicec,evdev_devicea;
  
  int evdev_fd;
  char *evdev_path; // Copied on open, in case evdev_devicev gets rebuilt.
  char *evdev_name;
  
  int ui_key_fd;
  int ui_mouse_fd;
  //TODO device id etc, keep on hand
  int dstmap[32]; // KEY_*, corresponding to (srcmap.state) little-endianly.
  
  /* (srcmap) turns the evdev device into something akin to Standard Mapping,
   * without caring how that will turn into output keys.
   * The output of this stage is 32 bitfields.
   * (srcmap) is recreated at each evdev connection.
   * We can use my shared device configs at ~/.config/aksomm/input for this.
   */
  struct srcmap {
    struct srcbtn {
      int type,code; // From evdev.
      int srcvalue; // Last observed input value.
      int srclo,srchi; // Input range, inclusive. If within, the output value is 1.
      int dstvalue; // 0,1: Last value we emitted.
      uint32_t dstbtnid;
    } *srcbtnv;
    int srcbtnc,srcbtna;
    uint32_t state; // FKBD_BTN_*
  } srcmap;
} g;

int http_init();
int http_accept();
void http_client_cleanup(struct http_client *client);
int http_client_read(struct http_client *client);
int http_client_write(struct http_client *client);
int http_request_measure(const char *src,int srcc);
int http_measure_line(const char *src,int srcc); // 0 if not terminated by CRLF.
int memcasecmp(const char *a,const char *b,int c);
int http_serve(struct http_client *client,const char *req,int reqc); // Must not modify (rbuf).
int http_serve_split(struct http_client *client,const char *method,int methodc,const char *path,int pathc,const char *query,int queryc,const void *body,int bodyc);
int http_client_wbuf_append(struct http_client *client,const char *src,int srcc);
int http_client_wbuf_require(struct http_client *client);

int evdev_init();
int evdev_update();
int evdev_scan(); // Drops all devices and rebuilds.
void evdev_device_cleanup(struct evdev_device *device);
void evdev_disconnect(); // Cleans up g.(evdev_fd,evdev_path,evdev_name)
int evdev_connect(const char *path);

void uinput_close();
int uinput_open();

int srcmap_connect(const struct evdev_device *device); // Null to drop mapping.
void srcmap_event(const struct input_event *event);

void dstmap_init();
void dstmap_event(uint32_t btnid,int value);

int fkbd_connect_path(const char *path);

uint32_t fkbd_btnname_eval(const char *src,int srcc);

#endif
