#include "fkbd.h"

struct fkbd fkbd={0};

/* Signal.
 */
 
static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(fkbd.sigc)>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Cleanup.
 */
 
static void fkbd_quit() {
  if (fkbd.uifd>0) close(fkbd.uifd);
  if (fkbd.evfd>0) close(fkbd.evfd);
  memset(&fkbd,0,sizeof(fkbd));
}

/* Main.
 */
 
int main(int argc,char **argv) {
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) fkbd.exename=argv[0];
  else fkbd.exename="fkbd";

  signal(SIGINT,rcvsig);
  
  /* Read command line.
   */
  int argi=1;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    // No dash: evdev path.
    if (arg[0]!='-') {
      if (fkbd.evdev_path) {
        fprintf(stderr,"%s: Multiple source paths.\n",fkbd.exename);
        return 1;
      }
      fkbd.evdev_path=arg;
      continue;
    }
    
    if (!strcmp(arg,"--help")) {
      fprintf(stderr,
        "\n"
        "Usage: %s EVDEV_PATH --map=MAP_NAME\n"
        "   Or: %s --tattle EVDEV_PATH\n"
        "Creates a fake keyboard device via uinput and echoes events from the given gamepad.\n"
        "Find your evdev device in /proc/bus/input/devices.\n"
        "\n"
      ,fkbd.exename,fkbd.exename);
      fkbd_uinput_log_map_names();
      return 0;
    }
    
    if (!strcmp(arg,"--tattle")) {
      fkbd.tattle=1;
      continue;
    }
    
    if (!memcmp(arg,"--map=",6)) {
      if (fkbd_uinput_map(arg+6)<0) {
        fprintf(stderr,"%s: Unknown map '%s'.\n",fkbd.exename,arg+6);
        fkbd_uinput_log_map_names();
        return 1;
      }
      continue;
    }
    
    fprintf(stderr,"%s: Unexpected argument '%s'.\n",fkbd.exename,arg);
    return 1;
  }
  if (!fkbd.evdev_path) {
    fprintf(stderr,"%s: Expected source path.\n",fkbd.exename);
    return 1;
  }
  
  /* Prepare evdev.
   */
  if (fkbd_evdev_open()<0) return 1;
  if (!fkbd.tattle&&!memcmp(fkbd.dstmap,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",32)) {
    fprintf(stderr,"%s: Please specify a map with '--map=NAME'.\n",fkbd.exename);
    fkbd_uinput_log_map_names();
    return 1;
  }
  
  /* Prepare uinput.
   */
  if (!fkbd.tattle) {
    if (fkbd_uinput_open()<0) return 1;
  }
  
  /* Main loop.
   * We have a single input source, so there's no poll, just block on read.
   * ...hmm actually SIGINT is not interrupting our reads. So do poll for just the one file.
   */
  fprintf(stderr,"%s running...\n",fkbd.exename);
  while (!fkbd.sigc) {
    struct pollfd pollfd={.fd=fkbd.evfd,.events=POLLIN|POLLERR|POLLHUP};
    if (poll(&pollfd,1,1000)<=0) continue;
    struct input_event eventv[32];
    int eventc=read(fkbd.evfd,eventv,sizeof(eventv));
    if (eventc<=0) {
      fprintf(stderr,"%s:read: %m\n",fkbd.evdev_path);
      break;
    }
    eventc/=sizeof(struct input_event);
    struct input_event *event=eventv;
    for (;eventc-->0;event++) {
      if (event->type==EV_SYN) continue;
      if (event->type==EV_MSC) continue;
      if (fkbd.tattle) {
        if (event->value) fprintf(stderr,"EVENT: %02x.%04x = %d\n",event->type,event->code,event->value);
        continue;
      }
      fkbd_evdev_update(event->type,event->code,event->value);
      if (fkbd.state!=fkbd.pvstate) {
        //fprintf(stderr,"STATE CHANGED: 0x%04x <= 0x%04x\n",fkbd.state,fkbd.pvstate);
        if (fkbd_uinput_update()<0) {
          return 1;
        }
        fkbd.pvstate=fkbd.state;
      }
    }
  }
  
  fprintf(stderr,"%s: Normal exit.\n",fkbd.exename);
  fkbd_quit();
  return 0;
}
