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
    uint8_t btnbit[(KEY_MAX+1)>>7];
    uint8_t absbit[(ABS_MAX+1)>>7];
    struct input_absinfo absinfo[ABS_MAX+1];
  } *evdev_devicev;
  int evdev_devicec,evdev_devicea;
  
  int evdev_fd;
  char *evdev_path; // Copied on open, in case evdev_devicev gets rebuilt.
  char *evdev_name;
  
  int ui_key_fd;
  int ui_mouse_fd;
  //TODO device id etc, keep on hand
  
  struct map {
    char *name;
    int namec;
    struct button {
      int srctype,srcbtnid;
      int srclo,srchi;
      int dstbtnid;
      int srcvalue; // Volatile
    } *buttonv;
    int buttonc,buttona;
  } *mapv;
  int mapc,mapa;
  struct map *map; // WEAK, points into (mapv).
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

int map_init();
int map_connect(const struct evdev_device *device); // Null to set no map.
int map_event(const struct input_event *event);
struct map *map_get(const char *name,int namec);
struct map *map_new(const char *name,int namec);
int map_buttonv_search(const struct map *map,int srctype,int srcbtnid); // If multiple, always returns the lowest matching index.
int map_buttonv_insert(struct map *map,int p,int srctype,int srcbtnid,int srclo,int srchi,int dstbtnid);

void uinput_close();
int uinput_open();

int fkbd_connect_path(const char *path);

#endif
