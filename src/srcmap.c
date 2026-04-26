#include "fkbd.h"

/* Search srcbtnv. Returns the lowest index if there's more than one.
 */
 
static int srcmap_srcbtnv_search(int type,int code) {
  int lo=0,hi=g.srcmap.srcbtnc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct srcbtn *q=g.srcmap.srcbtnv+ck;
         if (type<q->type) hi=ck;
    else if (type>q->type) lo=ck+1;
    else if (code<q->code) hi=ck;
    else if (code>q->code) lo=ck+1;
    else {
      while ((ck>lo)&&(q[-1].code==code)&&(q[-1].type==type)) { ck--; q--; }
      return ck;
    }
  }
  return -lo-1;
}

/* Reallocate srcbtnv.
 */
 
static int srcmap_srcbtnv_require() {
  if (g.srcmap.srcbtnc<g.srcmap.srcbtna) return 0;
  int na=g.srcmap.srcbtna+32;
  if (na>INT_MAX/sizeof(struct srcbtn)) return -1;
  void *nv=realloc(g.srcmap.srcbtnv,sizeof(struct srcbtn)*na);
  if (!nv) return -1;
  g.srcmap.srcbtnv=nv;
  g.srcmap.srcbtna=na;
  return 0;
}

/* Append to srcbtnv. Caller guarantees they will call in order of (type,code).
 */
 
static int srcmap_append(int type,int code,int srclo,int srchi,uint32_t dstbtnid) {
  if (srcmap_srcbtnv_require()<0) return -1;
  struct srcbtn *srcbtn=g.srcmap.srcbtnv+g.srcmap.srcbtnc++;
  srcbtn->type=type;
  srcbtn->code=code;
  srcbtn->srclo=srclo;
  srcbtn->srchi=srchi;
  srcbtn->dstbtnid=dstbtnid;
  if (srclo<=INT_MIN) srcbtn->srcvalue=srchi+1;
  else srcbtn->srcvalue=srclo-1;
  srcbtn->dstvalue=0;
  return 0;
}

/* Add to srcbtnv wherever it belongs.
 */
 
static int srcmap_add_srcbtn(int type,int code,int srclo,int srchi,uint32_t dstbtnid) {
  int p=srcmap_srcbtnv_search(type,code);
  if (p<0) p=-p-1;
  if (srcmap_srcbtnv_require()<0) return -1;
  struct srcbtn *srcbtn=g.srcmap.srcbtnv+p;
  memmove(srcbtn+1,srcbtn,sizeof(struct srcbtn)*(g.srcmap.srcbtnc-p));
  g.srcmap.srcbtnc++;
  srcbtn->type=type;
  srcbtn->code=code;
  srcbtn->srclo=srclo;
  srcbtn->srchi=srchi;
  srcbtn->dstbtnid=dstbtnid;
  if (srclo<=INT_MAX) srcbtn->srcvalue=srchi+1;
  else srcbtn->srcvalue=srclo-1;
  srcbtn->dstvalue=0;
  return 0;
}

/* Shared config file, block header.
 */
 
struct incfg_criteria {
  int vid,pid,version;
  const char *name;
  int namec;
};

static int incfg_hexuint(int *dst,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return -1;
  if ((srcp<srcc-2)&&!memcmp(src+srcp,"0x",2)) srcp+=2;
  *dst=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) {
    char ch=src[srcp++];
         if ((ch>='0')&&(ch<='9')) ch=ch-'0';
    else if ((ch>='a')&&(ch<='f')) ch=ch-'a'+10;
    else if ((ch>='A')&&(ch<='F')) ch=ch-'A'+10;
    else return -1;
    if ((*dst)&0xf0000000) return -1;
    (*dst)<<=4;
    (*dst)|=ch;
  }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  return srcp;
}

static int incfg_criteria_eval(struct incfg_criteria *dst,const char *src,int srcc) {
  int srcp=0,err;
  // Skip leading space and '>', in case the introducer is still included.
  while ((srcp<srcc)&&(((unsigned char)src[srcp]<=0x20)||(src[srcp]=='>'))) srcp++;
  // Followed by three unprefixed hexadecimal integers delimited by whitespace: vid pid version
  if ((err=incfg_hexuint(&dst->vid,src+srcp,srcc-srcp))<0) return -1; srcp+=err;
  if ((err=incfg_hexuint(&dst->pid,src+srcp,srcc-srcp))<0) return -1; srcp+=err;
  if ((err=incfg_hexuint(&dst->version,src+srcp,srcc-srcp))<0) return -1; srcp+=err;
  // Followed by a quoted string for the name. No escapes, and don't complain if it's missing.
  if ((srcp>srcc-2)||(src[srcp]!='"')||(src[srcc-1]!='"')) return 0;
  dst->name=src+srcp+1;
  dst->namec=srcc-srcp-2;
  return 0;
}

/* Apply rule from shared config file.
 */
 
static int srcmap_apply_rule_twoway(const struct evdev_device *device,int type,int code,uint32_t dstbtnidlo,uint32_t dstbtnidhi) {
  if (type!=EV_ABS) return 0; // twoway only applies to abs inputs
  int major=code>>3;
  if (major>=sizeof(device->absbit)) return 0;
  int minor=code&7;
  if (!(device->absbit[major]&(1<<minor))) return 0;
  const struct input_absinfo *absinfo=device->absinfo+code;
  if (absinfo->minimum>absinfo->maximum-2) return 0; // need a range of at least 3: 1 intermediate value
  int mid=(absinfo->minimum+absinfo->maximum)>>1;
  int midlo=(absinfo->minimum+mid)>>1;
  int midhi=(absinfo->maximum+mid)>>1;
  if (midlo>=mid) midlo=mid-1;
  if (midhi<=mid) midhi=mid+1;
  if (srcmap_add_srcbtn(type,code,INT_MIN,midlo,dstbtnidlo)<0) return -1;
  if (srcmap_add_srcbtn(type,code,midhi,INT_MAX,dstbtnidhi)<0) return -1;
  return 0;
}
 
static int srcmap_apply_rule(const struct evdev_device *device,int type,int code,uint32_t dstbtnid) {
  if (code<0) return 0;

  /* Support HORZ and VERT. Any other multibit output (eg hats) is not currently supported.
   */
  if (!dstbtnid) return 0;
  if (dstbtnid==(FKBD_BTN_LEFT|FKBD_BTN_RIGHT)) return srcmap_apply_rule_twoway(device,type,code,FKBD_BTN_LEFT,FKBD_BTN_RIGHT);
  if (dstbtnid==(FKBD_BTN_UP|FKBD_BTN_DOWN)) return srcmap_apply_rule_twoway(device,type,code,FKBD_BTN_UP,FKBD_BTN_DOWN);
  int bitc=0;
  uint32_t mask=0x80000000;
  for (;mask;mask>>=1) if (dstbtnid&mask) bitc++;
  if (bitc!=1) return 0; // Hat or some other multibit combo. Not supported.
  
  /* EV_KEY, check our caps and then use a hard-coded input range.
   */
  if (type==EV_KEY) {
    int major=code>>3;
    if (major>=sizeof(device->btnbit)) return 0;
    int minor=code&7;
    if (!(device->btnbit[major]&(1<<minor))) return 0;
    return srcmap_add_srcbtn(type,code,1,INT_MAX,dstbtnid);
  }
  
  /* EV_ABS, check our caps and configure as a oneway axis.
   */
  if (type==EV_ABS) {
    int major=code>>3;
    if (major>=sizeof(device->absbit)) return 0;
    int minor=code&7;
    if (!(device->absbit[major]&(1<<minor))) return 0;
    const struct input_absinfo *absinfo=device->absinfo+code;
    if (absinfo->minimum>=absinfo->maximum) return 0;
    return srcmap_add_srcbtn(type,code,absinfo->minimum+1,INT_MAX,dstbtnid);
  }
  
  // Other type? Don't bother.
  return 0;
}

/* From shared config file, in memory.
 */
 
static int srcmap_from_config_text(const struct evdev_device *device,const char *src,int srcc,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int lineno=1,linec,bestscore=-1,bestp=0,bestlineno=0;
  const char *line;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    // In this first pass, we're only interested in the introducer lines, which start ">>>".
    if ((linec<3)||memcmp(line,">>>",3)) continue;
    struct incfg_criteria criteria={0};
    if (incfg_criteria_eval(&criteria,line,linec)<0) continue;
    // Zeroes in criteria match all. Skip if any mismatch, and record the count of matched non-wildcards.
    int score=0;
    if (criteria.vid) {
      if (criteria.vid!=device->id.vendor) continue;
      score++;
    }
    if (criteria.pid) {
      if (criteria.pid!=device->id.product) continue;
      score++;
    }
    if (criteria.version) {
      if (criteria.version!=device->id.version) continue;
      score++;
    }
    if (criteria.namec) {
      if (!device->name) continue;
      if (memcmp(criteria.name,device->name,criteria.namec)) continue;
      if (device->name[criteria.namec]) continue;
      score++;
    }
    // Ties break early. If we get a score of four, it can't be beat, so stop reading.
    if (score>bestscore) {
      bestscore=score;
      bestp=decoder.p;
      bestlineno=lineno;
      if (score>=4) break;
    }
  }
  if (bestscore<0) return -1; // Device not found.
  
  /* We found a match.
   * Put the decoder back there, and read again linewise, applying each rule as we find it.
   */
  decoder.p=bestp;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (linec<1) continue;
    if (line[0]=='#') continue;
    if ((linec>=3)&&!memcmp(line,">>>",3)) break; // End of block.
    int srcbtnid=0;
    int linep=incfg_hexuint(&srcbtnid,line,linec);
    int type=srcbtnid>>16;
    int code=srcbtnid&0xffff;
    uint32_t dstbtnid=fkbd_btnname_eval(line+linep,linec-linep);
    if (!dstbtnid) continue;
    if (srcmap_apply_rule(device,type,code,dstbtnid)<0) return -1;
  }
  
  return 0;
}

/* Read the global input config, search for this device, if we find something, apply it.
 * Fail if nothing found.
 */
 
static int srcmap_from_config(const struct evdev_device *device) {
  
  const char *HOME=getenv("HOME");
  if (!HOME||!HOME[0]) return -1;
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/.config/aksomm/input",HOME);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return -1;
  
  int err=srcmap_from_config_text(device,src,srcc,path);
  free(src);
  return err;
}

/* Write the global config file, replacing or appending the entry for (g.srcmap).
 */
 
int srcmap_write_config() {
  fprintf(stderr,"%s: TODO\n",__func__);//TODO
  return 0;
}

/* Connect or disconnect.
 */
 
int srcmap_connect(const struct evdev_device *device) {

  // Drop all held state.
  if (g.srcmap.state) {
    uint32_t bit=0x80000000;
    for (;bit;bit>>=1) {
      if (g.srcmap.state&bit) {
        dstmap_event(bit,0);
      }
    }
    g.srcmap.state=0;
  }

  // Wipe the srcmap.
  g.srcmap.srcbtnc=0;
  
  // If we're not getting a new device, we're done.
  if (!device) return 0;
  
  // If we can load it from an environmental config file, awesome.
  if (srcmap_from_config(device)>=0) return 0;
  g.srcmap.srcbtnc=0; // In case loading failed partway thru.
  
  /* No config available. Synthesize it.
   * We'll be pretty dumb about this, since even the smartest implementation will still usually be wrong.
   * Trust the codes for axes. Two-state buttons, just assign in an arbitrary sequence.
   * Don't assign two-states to the dpad. Assume the dpad reports as axes.
   */
  int seq=0;
  int major=0;
  for (;major<sizeof(device->btnbit);major++) {
    if (!device->btnbit[major]) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(device->btnbit[major]&(1<<minor))) continue;
      int code=(major<<3)|minor;
      uint32_t dstbtnid=0;
      switch (seq++) {
        case 0: dstbtnid=FKBD_BTN_SOUTH; break;
        case 1: dstbtnid=FKBD_BTN_WEST; break;
        case 2: dstbtnid=FKBD_BTN_EAST; break;
        case 3: dstbtnid=FKBD_BTN_NORTH; break;
        case 4: dstbtnid=FKBD_BTN_AUX1; break;
        case 5: dstbtnid=FKBD_BTN_L1; break;
        case 6: dstbtnid=FKBD_BTN_R1; break;
        case 7: dstbtnid=FKBD_BTN_L2; break;
        case 8: dstbtnid=FKBD_BTN_R2; break;
        case 9: dstbtnid=FKBD_BTN_AUX2; break;
        case 10: dstbtnid=FKBD_BTN_AUX3; break;
        case 11: dstbtnid=FKBD_BTN_LP; break;
        case 12: dstbtnid=FKBD_BTN_RP; seq=0; break;
      }
      if (!dstbtnid) continue;
      if (srcmap_append(EV_KEY,code,1,INT_MAX,dstbtnid)<0) return -1;
    }
  }
  for (major=0;major<sizeof(device->absbit);major++) {
    if (!device->absbit[major]) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(device->absbit[major]&(1<<minor))) continue;
      int code=(major<<3)|minor;
      const struct input_absinfo *absinfo=device->absinfo+code;
      if (absinfo->minimum>=absinfo->maximum) continue;
      uint32_t btnidlo=0,btnidhi=0;
      switch (code) {
        case ABS_X: btnidlo=FKBD_BTN_LL; btnidhi=FKBD_BTN_LR; break;
        case ABS_Y: btnidlo=FKBD_BTN_LU; btnidhi=FKBD_BTN_LD; break;
        case ABS_RX: btnidlo=FKBD_BTN_RL; btnidhi=FKBD_BTN_RR; break;
        case ABS_RY: btnidlo=FKBD_BTN_RU; btnidhi=FKBD_BTN_RD; break;
        case ABS_Z: btnidlo=FKBD_BTN_RL; btnidhi=FKBD_BTN_RR; break; // El Cheapo, the right stick appears as (Z,RZ) on my Nuc.
        case ABS_RZ: btnidlo=FKBD_BTN_RU; btnidhi=FKBD_BTN_RD; break;
        case ABS_HAT0X: case ABS_HAT1X: case ABS_HAT2X: case ABS_HAT3X: btnidlo=FKBD_BTN_LEFT; btnidhi=FKBD_BTN_RIGHT; break;
        case ABS_HAT0Y: case ABS_HAT1Y: case ABS_HAT2Y: case ABS_HAT3Y: btnidlo=FKBD_BTN_UP; btnidhi=FKBD_BTN_DOWN; break;
        default: btnidhi=FKBD_BTN_NORTH; break; // Call it a one-way axis on NORTH, just so we're not discarding it.
      }
      if (!btnidhi) continue;
      if (btnidlo) { // two-way
        int mid=(absinfo->minimum+absinfo->maximum)>>1;
        int midlo=(absinfo->minimum+mid)>>1;
        int midhi=(absinfo->maximum+mid)>>1;
        if (midlo>=mid) midlo=mid-1;
        if (midhi<=mid) midhi=mid+1;
        if (srcmap_append(EV_ABS,code,INT_MIN,midlo,btnidlo)<0) return -1;
        if (srcmap_append(EV_ABS,code,midhi,INT_MAX,btnidhi)<0) return -1;
      } else { // one-way
        if (srcmap_append(EV_ABS,code,absinfo->minimum+1,INT_MAX,btnidhi)<0) return -1;
      }
    }
  }
  srcmap_write_config();
  
  return 0;
}

/* Event.
 */
 
void srcmap_event(const struct input_event *event) {
  if (event->type==EV_SYN) return;
  if (event->type==EV_MSC) return;
  
  /* EV_KEY and EV_ABS states get recorded globally for monitoring.
   * This is argubaly evdev's problem, not ours, but we're kind of in a better spot for it.
   * This state tracking is always on, even when not monitoring.
   * So we've taken pains to keep it cheap.
   */
  switch (event->type) {
    case EV_KEY: {
        int major=event->code>>3;
        if ((major>=0)&&(major<sizeof(g.evbtnv))) {
          uint8_t mask=1<<(event->code&7);
          if (event->value) g.evbtnv[major]|=mask;
          else g.evbtnv[major]&=~mask;
        }
      } break;
    case EV_ABS: {
        if ((event->code>=0)&&(event->code<=ABS_MAX)) {
          g.evabsv[event->code]=event->value;
        }
      } break;
  }
  
  /* Then the real thing: Check maps registered for this button.
   */
  int srcbtnp=srcmap_srcbtnv_search(event->type,event->code);
  if (srcbtnp<0) return;
  struct srcbtn *srcbtn=g.srcmap.srcbtnv+srcbtnp;
  for (;(srcbtnp<g.srcmap.srcbtnc)&&(srcbtn->code==event->code)&&(srcbtn->type==event->type);srcbtnp++,srcbtn++) {
    if (event->value==srcbtn->srcvalue) continue;
    srcbtn->srcvalue=event->value;
    int dstvalue=((event->value>=srcbtn->srclo)&&(event->value<=srcbtn->srchi))?1:0;
    if (dstvalue==srcbtn->dstvalue) continue;
    srcbtn->dstvalue=dstvalue;
    if (dstvalue) {
      if (g.srcmap.state&srcbtn->dstbtnid) continue;
      g.srcmap.state|=srcbtn->dstbtnid;
    } else {
      if (!(g.srcmap.state&srcbtn->dstbtnid)) continue;
      g.srcmap.state&=~srcbtn->dstbtnid;
    }
    dstmap_event(srcbtn->dstbtnid,dstvalue);
  }
}

/* Replace per JSON from client.
 */
 
int srcmap_from_json(const char *src,int srcc) {

  // Drop all held state.
  if (g.srcmap.state) {
    uint32_t bit=0x80000000;
    for (;bit;bit>>=1) {
      if (g.srcmap.state&bit) {
        dstmap_event(bit,0);
      }
    }
    g.srcmap.state=0;
  }
  g.srcmap.srcbtnc=0;
  
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_array_start(&decoder)<0) return -1;
  while (sr_decode_json_next(0,&decoder)>0) {
    int type=0,code=0,srclo=0,srchi=0,dst=0;
    int octx=sr_decode_json_object_start(&decoder);
    const char *k;
    int kc;
    while ((kc=sr_decode_json_next(&k,&decoder))>0) {
           if ((kc==4)&&!memcmp(k,"type",4)) sr_decode_json_int(&type,&decoder);
      else if ((kc==4)&&!memcmp(k,"code",4)) sr_decode_json_int(&code,&decoder);
      else if ((kc==5)&&!memcmp(k,"srclo",5)) sr_decode_json_int(&srclo,&decoder);
      else if ((kc==5)&&!memcmp(k,"srchi",5)) sr_decode_json_int(&srchi,&decoder);
      else if ((kc==3)&&!memcmp(k,"dst",3)) sr_decode_json_int(&dst,&decoder);
      else sr_decode_json_skip(&decoder);
    }
    if (sr_decode_json_end(&decoder,octx)<0) return -1;
    if (srcmap_add_srcbtn(type,code,srclo,srchi,dst)<0) return -1;
  }
  return 0;
}

/* Encode current state to JSON.
 */
 
int srcmap_to_json(struct sr_encoder *dst) {
  int actx=sr_encode_json_array_start(dst,0,0);
  const struct srcbtn *srcbtn=g.srcmap.srcbtnv;
  int i=g.srcmap.srcbtnc;
  for (;i-->0;srcbtn++) {
    int octx=sr_encode_json_object_start(dst,0,0);
    sr_encode_json_int(dst,"type",4,srcbtn->type);
    sr_encode_json_int(dst,"code",4,srcbtn->code);
    sr_encode_json_int(dst,"srclo",5,srcbtn->srclo);
    sr_encode_json_int(dst,"srchi",5,srcbtn->srchi);
    sr_encode_json_int(dst,"dst",3,srcbtn->dstbtnid);
    if (sr_encode_json_end(dst,octx)<0) return -1;
  }
  return sr_encode_json_end(dst,actx);
}

/* Encode volatile state.
 */
 
int srcmap_encode_state(struct sr_encoder *dst) {
  int octx=sr_encode_json_object_start(dst,0,0);
  
  /* We can only populate the "buttons" and "axes" if we have the evdev_device it spawned from.
   * Yep, we're searching the whole list for every update, by name.
   */
  struct evdev_device *device=0;
  if (g.evdev_path) {
    struct evdev_device *q=g.evdev_devicev;
    int i=g.evdev_devicec;
    for (;i-->0;q++) {
      if (strcmp(q->path,g.evdev_path)) continue;
      device=q;
      break;
    }
  }
  
  // Buttons.
  int actx=sr_encode_json_array_start(dst,"buttons",7);
  if (device) {
    int major=0;
    for (;major<sizeof(device->btnbit);major++) {
      if (!device->btnbit[major]) continue;
      int minor=0;
      for (;minor<8;minor++) {
        if (!(device->btnbit[major]&(1<<minor))) continue;
        int value=(g.evbtnv[major]&(1<<minor))?1:0;
        sr_encode_json_int(dst,0,0,value);
      }
    }
  }
  sr_encode_json_end(dst,actx);
  
  // Axes.
  actx=sr_encode_json_array_start(dst,"axes",4);
  if (device) {
    int major=0;
    for (;major<sizeof(device->absbit);major++) {
      if (!device->absbit[major]) continue;
      int minor=0;
      for (;minor<8;minor++) {
        if (!(device->absbit[major]&(1<<minor))) continue;
        int code=(major<<3)|minor;
        sr_encode_json_int(dst,0,0,g.evabsv[code]);
      }
    }
  }
  sr_encode_json_end(dst,actx);
  
  // Logical state doesn't require the device, it's always real.
  sr_encode_json_int(dst,"logical",7,g.srcmap.state);
  
  return sr_encode_json_end(dst,octx);
}
