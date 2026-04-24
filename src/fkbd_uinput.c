#include "fkbd.h"

/* Keycodes.
 * We'll report the virtual device as capable of all of them, so it looks like a keyboard.
 * Even though only a small subset will actually be used.
 */
#define KEYCODE_FIRST 1 /* KEY_A */
#define KEYCODE_LAST 111 /* KEY_DELETE. Everything above looks exotic. */

/* Output maps.
 * For new devices, add them at fkbd_evdev.c.
 * New games, add them here.
 * Order of these arrays is: LEFT RIGHT UP DOWN SOUTH WEST EAST NORTH L1 R2 L2 R2 AUX1 AUX2 AUX3 UNUSED
 */
 
static const uint16_t dstmap_arrowzx[16]={ KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN, KEY_Z,KEY_X,KEY_C,KEY_E, KEY_LEFTSHIFT,KEY_BACKSLASH,KEY_GRAVE,KEY_BACKSPACE, KEY_ENTER,KEY_SPACE,KEY_ESC };
static const uint16_t dstmap_wasde[16]={ KEY_A,KEY_D,KEY_W,KEY_S, KEY_E,KEY_R,KEY_Q,KEY_J, KEY_SPACE,KEY_COMMA,KEY_DOT,KEY_SLASH, KEY_ENTER,KEY_SPACE,KEY_ESC };
static const uint16_t dstmap_wasdq[16]={ KEY_A,KEY_D,KEY_W,KEY_S, KEY_SPACE,KEY_Q,KEY_E,KEY_J, KEY_N,KEY_COMMA,KEY_DOT,KEY_SLASH, KEY_ENTER,KEY_SPACE,KEY_ESC };
static const uint16_t dstmap_wasdj[16]={ KEY_A,KEY_D,KEY_W,KEY_S, KEY_J,KEY_K,KEY_Q,KEY_E, KEY_N,KEY_COMMA,KEY_DOT,KEY_SLASH, KEY_ENTER,KEY_SPACE,KEY_ESC };
static const uint16_t dstmap_wasdkl[16]={ KEY_A,KEY_D,KEY_W,KEY_S, KEY_L,KEY_K,KEY_Q,KEY_E, KEY_N,KEY_COMMA,KEY_DOT,KEY_SLASH, KEY_ENTER,KEY_SPACE,KEY_ESC };
static const uint16_t dstmap_wasdqekl[16]={ KEY_A,KEY_D,KEY_W,KEY_S, KEY_L,KEY_K,KEY_R,KEY_T, KEY_N,KEY_COMMA,KEY_DOT,KEY_ENTER, KEY_E,KEY_Q,KEY_ESC };
static const uint16_t dstmap_horzspace[16]={ KEY_A,KEY_D,0,KEY_S, KEY_W,KEY_SPACE,KEY_LEFTSHIFT,KEY_E, 0,0,0,0, 0,0,0,0 };
static const uint16_t dstmap_arrowxzc[16]={ KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN, KEY_X,KEY_C,KEY_Z,KEY_V, KEY_TAB,KEY_BACKSLASH,KEY_GRAVE,KEY_BACKSPACE, KEY_ENTER,KEY_SPACE,KEY_ESC };
static const uint16_t dstmap_wasdspace[16]={ KEY_A,KEY_D,KEY_W,KEY_S, KEY_SPACE,KEY_K,KEY_Q,KEY_E, KEY_N,KEY_COMMA,KEY_DOT,KEY_SLASH, KEY_ENTER,KEY_SPACE,KEY_ESC };
static const uint16_t dstmap_clawstrike[16]={ KEY_A,KEY_D,KEY_M,KEY_S, KEY_W,KEY_SPACE,KEY_E,KEY_R, 0,0,0,0, 0,0,0,0 }; // Dammit Remi
static const uint16_t dstmap_arrowjump[16]={ KEY_LEFT,KEY_RIGHT,0,KEY_DOWN, KEY_UP,0,0,0, 0,0,0,0, 0,0,0,0 }; // Up to jump? No thank you.
static const uint16_t dstmap_wasdjkl[16]={ KEY_A,KEY_D,KEY_W,KEY_S, KEY_J,KEY_K,KEY_L,0, 0,0,0,0, KEY_ENTER,KEY_SPACE,KEY_ESC };
static const uint16_t dstmap_wasdhjb[16]={ KEY_A,KEY_D,KEY_W,KEY_S, KEY_H,KEY_J,KEY_B,0, 0,0,0,0, KEY_ENTER,KEY_SPACE,KEY_ESC };
static const uint16_t dstmap_arrowzspace[16]={ KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN, KEY_SPACE,KEY_Z,KEY_R,0, KEY_ENTER,0,0,0 };
static const uint16_t dstmap_stop_daichi[16]={ KEY_A,KEY_D,0,0, KEY_W,KEY_K,KEY_J,0, KEY_R,0,0,0 };
static const uint16_t dstmap_gardener[16]={ KEY_Q,KEY_D,KEY_Z,KEY_S, KEY_E,0,0,0, 0,0,0,0, 0,0,0,0 }; // Gardener Little Robot, in Uplifting #7

#define DSTMAP_FOR_EACH \
  _(arrowzx) \
  _(wasde) \
  _(wasdq) \
  _(wasdj) \
  _(wasdkl) \
  _(wasdqekl) \
  _(horzspace) \
  _(arrowxzc) \
  _(wasdspace) \
  _(clawstrike) \
  _(arrowjump) \
  _(wasdjkl) \
  _(wasdhjb) \
  _(arrowzspace) \
  _(stop_daichi) \
  _(gardener)
  
int fkbd_uinput_map(const char *name) {
  #define _(tag) if (!strcmp(name,#tag)) { \
    memcpy(fkbd.dstmap,dstmap_##tag,sizeof(fkbd.dstmap)); \
    return 0; \
  }
  DSTMAP_FOR_EACH
  #undef _
  return -1;
}

void fkbd_uinput_log_map_names() {
  fprintf(stderr,"Maps defined in %s:",__FILE__);
  #define _(tag) fprintf(stderr," %s",#tag);
  DSTMAP_FOR_EACH
  #undef _
  fprintf(stderr,"\n");
}

/* Open uinput and declare our fake device.
 */

int fkbd_uinput_open() {
  if (fkbd.uifd>0) return -1;
  if ((fkbd.uifd=open("/dev/uinput",O_RDWR))<0) {
    fprintf(stderr,"/dev/uinput: %m\n");
    return -1;
  }
  struct uinput_setup setup={
    .id={
      .bustype=BUS_VIRTUAL,
      .vendor=0xffbd,
      .product=0x0001,
      .version=0x0100,
    },
    .name="fkbd Fake Keyboard",
    .ff_effects_max=0,
  };
  if (ioctl(fkbd.uifd,UI_DEV_SETUP,&setup)<0) {
    fprintf(stderr,"/dev/uinput:UI_DEV_SETUP: %m\n");
    return -1;
  }
  if (ioctl(fkbd.uifd,UI_SET_EVBIT,EV_KEY)<0) {
    fprintf(stderr,"/dev/uinput:UI_SET_EVBIT(EV_KEY): %m\n");
    return -1;
  }
  int keycode=KEYCODE_FIRST;
  for (;keycode<=KEYCODE_LAST;keycode++) {
    if (ioctl(fkbd.uifd,UI_SET_KEYBIT,keycode)<0) {
      fprintf(stderr,"/dev/uinput:UI_SET_KEYBIT(%d): %m\n",keycode);
      return -1;
    }
  }
  if (ioctl(fkbd.uifd,UI_DEV_CREATE)<0) {
    fprintf(stderr,"/dev/uinput:UI_DEV_CREATE: %m\n");
    return -1;
  }
  return 0;
}

/* Send one event and a sync.
 */
 
int fkbd_uinput_event(int code,int value) {
  struct input_event event={.type=EV_KEY,.code=code,.value=value};
  if (write(fkbd.uifd,&event,sizeof(event))<0) {
    fprintf(stderr,"/dev/uinput:write: %m\n");
    return -1;
  }
  event.type=EV_SYN;
  event.code=SYN_REPORT;
  event.value=0;
  if (write(fkbd.uifd,&event,sizeof(event))<0) {
    fprintf(stderr,"/dev/uinput:write: %m\n");
    return -1;
  }
  return 0;
}

/* Update per global (state,pvstate).
 */
 
int fkbd_uinput_update() {
  uint16_t bit=1,bitix=0;
  for (;bit;bit<<=1,bitix++) {
    if ((fkbd.state&bit)&&!(fkbd.pvstate&bit)) {
      fkbd_uinput_event(fkbd.dstmap[bitix],1);
    } else if (!(fkbd.state&bit)&&(fkbd.pvstate&bit)) {
      fkbd_uinput_event(fkbd.dstmap[bitix],0);
    }
  }
  return 0;
}
