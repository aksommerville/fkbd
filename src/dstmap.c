#include "fkbd.h"

/* Initial mapping.
 */
 
void dstmap_init() {
  // Just making things up. It will never be correct, but you have to start somewhere.
  g.dstmap[FKBD_BTNIX_LEFT ]=KEY_A;
  g.dstmap[FKBD_BTNIX_RIGHT]=KEY_D;
  g.dstmap[FKBD_BTNIX_UP   ]=KEY_W;
  g.dstmap[FKBD_BTNIX_DOWN ]=KEY_S;
  g.dstmap[FKBD_BTNIX_SOUTH]=KEY_SPACE;
  g.dstmap[FKBD_BTNIX_WEST ]=KEY_E;
  g.dstmap[FKBD_BTNIX_EAST ]=KEY_R;
  g.dstmap[FKBD_BTNIX_NORTH]=KEY_Q;
  g.dstmap[FKBD_BTNIX_L1   ]=KEY_1;
  g.dstmap[FKBD_BTNIX_R1   ]=KEY_2;
  g.dstmap[FKBD_BTNIX_L2   ]=KEY_3;
  g.dstmap[FKBD_BTNIX_R2   ]=KEY_4;
  g.dstmap[FKBD_BTNIX_AUX1 ]=KEY_ENTER;
  g.dstmap[FKBD_BTNIX_AUX2 ]=KEY_TAB;
  g.dstmap[FKBD_BTNIX_AUX3 ]=KEY_ESC;
  // I don't usually map the remainder correctly because Egg and old consoles don't use them. But again, something is better than nothing.
  g.dstmap[FKBD_BTNIX_LP   ]=KEY_Z;
  g.dstmap[FKBD_BTNIX_RP   ]=KEY_X;
  g.dstmap[FKBD_BTNIX_LL   ]=KEY_LEFT;
  g.dstmap[FKBD_BTNIX_LR   ]=KEY_RIGHT;
  g.dstmap[FKBD_BTNIX_LU   ]=KEY_UP;
  g.dstmap[FKBD_BTNIX_LD   ]=KEY_DOWN;
  g.dstmap[FKBD_BTNIX_RL   ]=KEY_KP4;
  g.dstmap[FKBD_BTNIX_RR   ]=KEY_KP6;
  g.dstmap[FKBD_BTNIX_RU   ]=KEY_KP8;
  g.dstmap[FKBD_BTNIX_RD   ]=KEY_KP5;
}

/* Intermediate state change.
 */
 
void dstmap_event(uint32_t btnid,int value) {
  if (!btnid) return;
  int ix=0;
  while (!(btnid&(1<<ix))) ix++;
  int code=g.dstmap[ix];
  if (!code) return;
  // High codes go to the mouse device, and low to the keyboard.
  int fd=(code>=BTN_MISC)?g.ui_mouse_fd:g.ui_key_fd;
  if (fd<0) return;
  {
    struct input_event event={.type=EV_KEY,.code=code,.value=value};
    if (write(fd,&event,sizeof(event))<0) uinput_close();
  }
  {
    struct input_event event={.type=EV_SYN,.code=SYN_REPORT};
    if (write(fd,&event,sizeof(event))<0) uinput_close();
  }
}
