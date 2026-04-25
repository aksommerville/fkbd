#include "fkbd.h"

/* Initialize maps.
 */
 
int map_init() {
  //TODO
  return 0;
}

/* Zero all button states.
 */
 
static void map_zero_state(struct map *map) {
  struct button *button=map->buttonv;
  int i=map->buttonc;
  for (;i-->0;button++) button->srcvalue=0;
}

/* KEY_* for some BTN_*
 * (note that those technically are the same namespace).
 * Not going to stress too hard over this, it's always going to be wrong either way.
 */
 
static int default_dstbtnid_for_key(int srcbtnid) {
  switch (srcbtnid) {
    case BTN_SOUTH: return KEY_SPACE;
    case BTN_WEST: return KEY_E;
    case BTN_EAST: return KEY_R;
    case BTN_NORTH: return KEY_Q;
    case BTN_TL: case BTN_TL2: return KEY_LEFTSHIFT;
    case BTN_TR: case BTN_TR2: return KEY_RIGHTSHIFT;
    case BTN_SELECT: return KEY_TAB;
    case BTN_START: return KEY_ENTER;
  }
  if (srcbtnid<KEY_MAX) return srcbtnid;
  return KEY_SPACE;
}

/* Add map button for an absolute axis.
 */
 
static int map_add_abs(struct map *map,int srcbtnid,const struct input_absinfo *absinfo,int dstbtnidlo,int dstbtnidhi) {
  int range=absinfo->maximum-absinfo->minimum+1;
  if (range<3) return 0;
  int mid=(absinfo->minimum+absinfo->maximum)>>1;
  int midlo=(absinfo->minimum+mid)>>1;
  if (midlo>=mid) midlo--;
  int midhi=(absinfo->maximum+mid)>>1;
  if (midhi<=mid) midhi++;
  map_buttonv_insert(map,map->buttonc,EV_ABS,srcbtnid,INT_MIN,midlo,dstbtnidlo);
  map_buttonv_insert(map,map->buttonc,EV_ABS,srcbtnid,midhi,INT_MAX,dstbtnidhi);
  return 0;
}

static int map_add_abs_oneway(struct map *map,int srcbtnid,const struct input_absinfo *absinfo) {
  int mid=(absinfo->minimum+absinfo->maximum)>>1;
  if (mid<=absinfo->minimum) mid++;
  map_buttonv_insert(map,map->buttonc,EV_ABS,srcbtnid,mid,INT_MAX,KEY_SPACE);
  return 0;
}

/* Create a new map and make up mappings for this device's buttons.
 */
 
static struct map *map_synthesize(const struct evdev_device *device) {
  struct map *map=map_new(device->name,-1);
  if (!map) return 0;
  int major=0;
  for (;major<sizeof(device->btnbit);major++) {
    if (!device->btnbit[major]) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(device->btnbit[major]&(1<<minor))) continue;
      int srcbtnid=(major<<3)|minor;
      map_buttonv_insert(map,map->buttonc,EV_KEY,srcbtnid,1,INT_MAX,default_dstbtnid_for_key(srcbtnid));
    }
  }
  for (major=0;major<sizeof(device->absbit);major++) {
    if (!device->absbit[major]) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(device->absbit[major]&(1<<minor))) continue;
      int srcbtnid=(major<<3)|minor;
      const struct input_absinfo *absinfo=device->absinfo+srcbtnid;
      switch (srcbtnid) {
        case ABS_X:
        case ABS_RX:
        case ABS_HAT0X:
        case ABS_HAT1X:
        case ABS_HAT2X:
        case ABS_HAT3X: {
            map_add_abs(map,srcbtnid,absinfo,KEY_A,KEY_D);
          } break;
        case ABS_Y:
        case ABS_RY:
        case ABS_HAT0Y:
        case ABS_HAT1Y:
        case ABS_HAT2Y:
        case ABS_HAT3Y: {
            map_add_abs(map,srcbtnid,absinfo,KEY_W,KEY_S);
          } break;
        default: {
            map_add_abs_oneway(map,srcbtnid,absinfo);
          } break;
      }
    }
  }
  return map;
}

/* Connect device.
 */
 
int map_connect(const struct evdev_device *device) {
  g.map=0;
  if (!device||!device->name||!device->name[0]) return 0;
  struct map *map=g.mapv;
  int i=g.mapc;
  for (;i-->0;map++) {
    if (memcmp(map->name,device->name,map->namec)) continue;
    if (device->name[map->namec]) continue;
    fprintf(stderr,"%s:%d: Map found for device '%s', buttonc=%d\n",__FILE__,__LINE__,device->name,map->buttonc);
    g.map=map;
    map_zero_state(map);
    return 0;
  }
  fprintf(stderr,"%s:%d: Synthesizing map for device '%s'.\n",__FILE__,__LINE__,device->name);//TODO make one up
  if (!(g.map=map_synthesize(device))) return -1;
  return 0;
}

/* Receive event from evdev.
 */
 
int map_event(const struct input_event *event) {
  if (!g.map) return 0;
  if (event->type==EV_SYN) return 0;
  if (event->type==EV_MSC) return 0; // MSC_SCAN might be useful; it gives HID Usage codes sometimes.
  int p=map_buttonv_search(g.map,event->type,event->code);
  if (p<0) return 0;
  struct button *button=g.map->buttonv+p;
  int i=g.map->buttonc-p;
  for (;i-->0;button++) {
    if ((button->srcbtnid!=event->code)||(button->srctype!=event->type)) break;
    int pv=((button->srcvalue>=button->srclo)&&(button->srcvalue<=button->srchi))?1:0;
    int nv=((event->value>=button->srclo)&&(event->value<=button->srchi))?1:0;
    button->srcvalue=event->value;
    if (pv!=nv) {
      fprintf(stderr,"%s: %04x:%04x=%d >>> %04x=%d\n",__func__,event->type,event->code,event->value,button->dstbtnid,nv);//TODO
    }
  }
  return 0;
}

/* Find map in registry.
 */
 
struct map *map_get(const char *name,int namec) {
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  struct map *map=g.mapv;
  int i=g.mapc;
  for (;i-->0;map++) {
    if (map->namec!=namec) continue;
    if (memcmp(map->name,name,namec)) continue;
    return map;
  }
  return 0;
}

/* New map.
 */
 
struct map *map_new(const char *name,int namec) {
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (g.mapc>=g.mapa) {
    if (g.map) g.map=0;
    int na=g.mapa+8;
    if (na>INT_MAX/sizeof(struct map)) return 0;
    void *nv=realloc(g.mapv,sizeof(struct map)*na);
    if (!nv) return 0;
    g.mapv=nv;
    g.mapa=na;
  }
  struct map *map=g.mapv+g.mapc++;
  memset(map,0,sizeof(struct map));
  if (namec) {
    if (!(map->name=malloc(namec+1))) { g.mapc--; return 0; }
    memcpy(map->name,name,namec);
    map->name[map->namec=namec]=0;
  }
  return map;
}

/* Search buttons in map.
 */
 
int map_buttonv_search(const struct map *map,int srctype,int srcbtnid) {
  int lo=0,hi=map->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct button *button=map->buttonv+ck;
         if (srctype<button->srctype) hi=ck;
    else if (srctype>button->srctype) lo=ck+1;
    else if (srcbtnid<button->srcbtnid) hi=ck;
    else if (srcbtnid>button->srcbtnid) lo=ck+1;
    else {
      while ((ck>lo)&&(button[-1].srcbtnid==srcbtnid)&&(button[-1].srctype==srctype)) { ck--; button--; }
      return ck;
    }
  }
  return -lo-1;
}

/* Add button to map.
 */
 
int map_buttonv_insert(struct map *map,int p,int srctype,int srcbtnid,int srclo,int srchi,int dstbtnid) {
  if (!map||(p<0)||(p>map->buttonc)) return -1;
  if (map->buttonc>=map->buttona) {
    int na=map->buttona+32;
    if (na>INT_MAX/sizeof(struct button)) return -1;
    void *nv=realloc(map->buttonv,sizeof(struct button)*na);
    if (!nv) return -1;
    map->buttonv=nv;
    map->buttona=na;
  }
  struct button *button=map->buttonv+p;
  memmove(button+1,button,sizeof(struct button)*(map->buttonc-p));
  map->buttonc++;
  button->srctype=srctype;
  button->srcbtnid=srcbtnid;
  button->srclo=srclo;
  button->srchi=srchi;
  button->dstbtnid=dstbtnid;
  if (srclo<=INT_MIN) button->srcvalue=srchi+1;
  else button->srcvalue=srclo-1;
  return 0;
}
