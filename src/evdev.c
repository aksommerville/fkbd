#include "fkbd.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dirent.h>

/* Clean up connected device.
 */
 
void evdev_disconnect() {
  if (g.evdev_fd>=0) {
    close(g.evdev_fd);
    g.evdev_fd=-1;
  }
  if (g.evdev_path) {
    free(g.evdev_path);
    g.evdev_path=0;
  }
  if (g.evdev_name) {
    free(g.evdev_name);
    g.evdev_name=0;
  }
}

/* Open new connection.
 */
 
int evdev_connect(const char *path) {
  if ((g.evdev_fd>=0)||g.evdev_path||g.evdev_name) return -1;
  if (!path||!path[0]) return -1;
  
  if ((g.evdev_fd=open(path,O_RDONLY))<0) return -1;
  
  g.evdev_path=strdup(path);
  
  srcmap_connect(0);
  
  // If we have a record of this device, copy its name and search for a map.
  struct evdev_device *device=g.evdev_devicev;
  int i=g.evdev_devicec;
  for (;i-->0;device++) {
    if (strcmp(path,device->path)) continue;
    if (device->name) {
      g.evdev_name=strdup(device->name);
    }
    srcmap_connect(device);
    break;
  }
  
  return 0;
}

/* Clean up device.
 */
 
void evdev_device_cleanup(struct evdev_device *device) {
  if (device->path) free(device->path);
  if (device->name) free(device->name);
}

/* Populate capability bits and abs ranges.
 */
 
static int evdev_device_collect_caps(struct evdev_device *device,int fd) {
  if (ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(device->btnbit)),device->btnbit)<0) return -1;
  if (ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(device->absbit)),device->absbit)<0) return -1;
  int major=0;
  for (;major<sizeof(device->absbit);major++) {
    if (!device->absbit[major]) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(device->absbit[major]&(1<<minor))) continue;
      if (ioctl(fd,EVIOCGABS((major<<3)|minor),device->absinfo+(major<<3)+minor)<0) return -1;
    }
  }
  return 0;
}

/* Initialize new device.
 */
 
static int evdev_device_init(struct evdev_device *device,const char *path) {
  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  
  char name[256];
  int namec=ioctl(fd,EVIOCGNAME(256),name);
  if ((namec<1)||(namec>=sizeof(name))) {
    close(fd);
    return -1;
  }
  name[namec]=0;
  if (ioctl(fd,EVIOCGID,&device->id)<0) {
    close(fd);
    return -1;
  }
  if (evdev_device_collect_caps(device,fd)<0) {
    close(fd);
    return -1;
  }
  close(fd);
  
  if (!(device->path=strdup(path))) return -1;
  if (!(device->name=strdup(name))) return -1;
  
  return 0;
}

/* Found a likely evdev device.
 * If we can pull ids from it, add to the list.
 * It's fine to return success without adding.
 */
 
static int evdev_add_device(const char *path) {
  if (g.evdev_devicec>=g.evdev_devicea) {
    int na=g.evdev_devicea+8;
    if (na>INT_MAX/sizeof(struct evdev_device)) return -1;
    void *nv=realloc(g.evdev_devicev,sizeof(struct evdev_device)*na);
    if (!nv) return -1;
    g.evdev_devicev=nv;
    g.evdev_devicea=na;
  }
  struct evdev_device *device=g.evdev_devicev+g.evdev_devicec++;
  memset(device,0,sizeof(struct evdev_device));
  if (evdev_device_init(device,path)<0) {
    evdev_device_cleanup(device);
    g.evdev_devicec--;
    return 0;
  }
  /*fprintf(stderr,"%s %04x:%04x:%04x '%s' @%s\n",
    __func__,
    device->id.vendor,device->id.product,device->id.version,
    device->name,device->path
  );/**/
  return 0;
}

/* Drop devices and scan from scratch.
 */
 
int evdev_scan() {
  while (g.evdev_devicec>0) {
    g.evdev_devicec--;
    evdev_device_cleanup(g.evdev_devicev+g.evdev_devicec);
  }
  char subpath[1024];
  int pfxc=snprintf(subpath,sizeof(subpath),"%s/",g.evdev_devices_path);
  if ((pfxc<1)||(pfxc>=sizeof(subpath))) return -1;
  DIR *dir=opendir(g.evdev_devices_path);
  if (!dir) {
    fprintf(stderr,"%s: Failed to read directory\n",g.evdev_devices_path);
    return -2;
  }
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if ((basec<=5)||memcmp(base,"event",5)) continue;
    if (pfxc+basec>=sizeof(subpath)) continue;
    memcpy(subpath+pfxc,base,basec+1);
    int err=evdev_add_device(subpath);
    if (err<0) {
      closedir(dir);
      return err;
    }
  }
  closedir(dir);
  return 0;
}

/* Initialize evdev.
 */
 
int evdev_init() {
  int err;
  if ((err=evdev_scan())<0) return err;
  return 0;
}

/* Read from the evdev device and dispatch.
 */
 
int evdev_update() {
  if (g.evdev_fd<0) return 0;
  char buf[1024];
  int bufc=read(g.evdev_fd,buf,sizeof(buf));
  if (bufc<=0) return -1;
  int bufp=0;
  for (;;) {
    if (bufp>bufc-sizeof(struct input_event)) break;
    struct input_event *event=(struct input_event*)(buf+bufp);
    bufp+=sizeof(struct input_event);
    srcmap_event(event);
  }
  return 0;
}
