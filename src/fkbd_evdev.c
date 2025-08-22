#include "fkbd.h"

/* Hard-coded device templates.
 * Add devices here, by vendor, product, and version.
 */
 
static const struct fkbd_tm {
  uint16_t vid,pid,ver;
  struct fkbd_map mapv[FKBD_MAP_LIMIT];
} tmv[]={

  { // El Cheapo. "MY-POWER CO.,LTD. 2In1 USB Joystick"
    0x0e8f,0x0003,0x0110,{
      {EV_ABS,0x0010,FKBD_BTN_LEFT|FKBD_BTN_RIGHT},
      {EV_ABS,0x0011,FKBD_BTN_UP|FKBD_BTN_DOWN},
      {EV_KEY,0x0122,FKBD_BTN_SOUTH},
      {EV_KEY,0x0123,FKBD_BTN_WEST},
      {EV_KEY,0x0121,FKBD_BTN_EAST},
      {EV_KEY,0x0120,FKBD_BTN_NORTH},
      {EV_KEY,0x0124,FKBD_BTN_L1},
      {EV_KEY,0x0125,FKBD_BTN_R1},
      {EV_KEY,0x0126,FKBD_BTN_L2},
      {EV_KEY,0x0127,FKBD_BTN_R2},
      {EV_KEY,0x0129,FKBD_BTN_AUX1},
      {EV_KEY,0x0128,FKBD_BTN_AUX2},
      {EV_KEY,0x012b,FKBD_BTN_AUX3},
    },
  },
};

/* Find a mapping template.
 */
 
static const struct fkbd_tm *fkbd_tm_find(uint16_t vid,uint16_t pid,uint16_t ver) {
  const struct fkbd_tm *tm=tmv;
  int i=sizeof(tmv)/sizeof(tmv[0]);
  for (;i-->0;tm++) {
    if (tm->vid!=vid) continue;
    if (tm->pid!=pid) continue;
    if (tm->ver!=ver) continue;
    return tm;
  }
  return 0;
}

/* Search mapped buttons.
 */
 
static int fkbd_map_search(int type,int code) {
  int lo=0,hi=fkbd.mapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct fkbd_map *q=fkbd.mapv+ck;
         if (type<q->srctype) hi=ck;
    else if (type>q->srctype) lo=ck+1;
    else if (code<q->srccode) hi=ck;
    else if (code>q->srccode) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Apply mapping template.
 */
 
static struct fkbd_map *fkbd_map_add(int type,int code) {
  int p=fkbd_map_search(type,code);
  if (p>=0) return 0;
  p=-p-1;
  struct fkbd_map *map=fkbd.mapv+p;
  memmove(map+1,map,sizeof(struct fkbd_map)*(fkbd.mapc-p));
  fkbd.mapc++;
  return map;
}
 
static void fkbd_tm_apply(const struct fkbd_tm *tm) {
  fkbd.mapc=0;
  const struct fkbd_map *src=tm->mapv;
  int i=FKBD_MAP_LIMIT;
  for (;i-->0;src++) {
    if (!src->srctype) break;
    struct fkbd_map *dst=fkbd_map_add(src->srctype,src->srccode);
    if (!dst) break;
    memcpy(dst,src,sizeof(struct fkbd_map));
    
    if (src->dstbtnid==(FKBD_BTN_LEFT|FKBD_BTN_RIGHT|FKBD_BTN_UP|FKBD_BTN_DOWN)) {
      dst->mode=4;
      if (dst->srclo==dst->srchi) {
        dst->srclo=0;
        dst->srchi=7;
      }
      
    } else if ((src->dstbtnid==(FKBD_BTN_LEFT|FKBD_BTN_RIGHT))||(src->dstbtnid==(FKBD_BTN_UP|FKBD_BTN_DOWN))) {
      dst->mode=3;
      if (dst->srclo==dst->srchi) {
        dst->srclo=-1;
        dst->srchi=1;
      }
      
    } else {
      dst->mode=2;
    }
  }
}

/* Open device.
 */
 
int fkbd_evdev_open() {
  if (fkbd.evfd>0) return -1;
  if ((fkbd.evfd=open(fkbd.evdev_path,O_RDONLY))<0) {
    fprintf(stderr,"%s:open: %m\n",fkbd.evdev_path);
    return -1;
  }
  
  struct input_id id={0};
  if (ioctl(fkbd.evfd,EVIOCGID,&id)<0) {
    fprintf(stderr,"%s:EVIOCGID: %m\n",fkbd.evdev_path);
    return -1;
  }
  fprintf(stderr,"%s: bus=0x%04x vendor=0x%04x product=0x%04x version=0x%04x\n",fkbd.evdev_path,id.bustype,id.vendor,id.product,id.version);
  
  //TODO Should we grab it?
  
  /* Select mapping.
   */
  const struct fkbd_tm *tm=fkbd_tm_find(id.vendor,id.product,id.version);
  if (!tm) {
    fprintf(stderr,"%s: Device not found. Entering '--tattle' mode so you can configure it. Add to 'tmv' in %s.\n",fkbd.evdev_path,__FILE__);
    fprintf(stderr,"IDs: 0x%04x,0x%04x,0x%04x\n",id.vendor,id.product,id.version);
    fkbd.tattle=1;
    //TODO Or should we read the caps and make something up? That's what a game would do. But seems pretty fuzzy for us.
  } else {
    fkbd_tm_apply(tm);
  }
  
  return 0;
}

/* Receive event.
 */
 
void fkbd_evdev_update(int type,int code,int value) {
  int p=fkbd_map_search(type,code);
  if (p<0) return;
  struct fkbd_map *map=fkbd.mapv+p;
  if (map->srcvalue==value) return;
  map->srcvalue=value;
  switch (map->mode) {
  
    case 2: {
        value=value?1:0;
        if (map->dstvalue==value) return;
        if (map->dstvalue=value) fkbd.state|=map->dstbtnid;
        else fkbd.state&=~map->dstbtnid;
      } break;
      
    case 3: {
        value=(value<=map->srclo)?-1:(value>=map->srchi)?1:0;
        if (map->dstvalue==value) return;
        uint16_t btnidlo=(FKBD_BTN_LEFT|FKBD_BTN_UP)&map->dstbtnid;
        uint16_t btnidhi=(FKBD_BTN_RIGHT|FKBD_BTN_DOWN)&map->dstbtnid;
        if (map->dstvalue<0) fkbd.state&=~btnidlo;
        else if (map->dstvalue>0) fkbd.state&=~btnidhi;
        map->dstvalue=value;
        if (value<0) fkbd.state|=btnidlo;
        else if (value>0) fkbd.state|=btnidhi;
      } break;
      
    case 4: {
        value-=map->srclo;
        if (value==map->dstvalue) return;
        int nx=0,ny=0,px=0,py=0;
        switch (value) {
          case 7: case 6: case 5: nx=-1; break;
          case 1: case 2: case 3: nx=1; break;
        }
        switch (value) {
          case 7: case 0: case 1: ny=-1; break;
          case 5: case 4: case 3: ny=1; break;
        }
        switch (map->dstvalue) {
          case 7: case 6: case 5: px=-1; break;
          case 1: case 2: case 3: px=1; break;
        }
        switch (map->dstvalue) {
          case 7: case 0: case 1: py=-1; break;
          case 5: case 4: case 3: py=1; break;
        }
        map->dstvalue=value;
        if (nx!=px) {
               if (px<0) fkbd.state&=~FKBD_BTN_LEFT;
          else if (px>0) fkbd.state&=~FKBD_BTN_RIGHT;
               if (nx<0) fkbd.state|=FKBD_BTN_LEFT;
          else if (nx>0) fkbd.state|=FKBD_BTN_RIGHT;
        }
        if (ny!=py) {
               if (py<0) fkbd.state&=~FKBD_BTN_UP;
          else if (py>0) fkbd.state&=~FKBD_BTN_DOWN;
               if (ny<0) fkbd.state|=FKBD_BTN_UP;
          else if (ny>0) fkbd.state|=FKBD_BTN_DOWN;
        }
      } break;
  }
}
