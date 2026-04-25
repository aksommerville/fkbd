#include "fkbd.h"

/* Button names.
 */
 
uint32_t fkbd_btnname_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  if (srcc<1) return 0;
  #define SIMPLE(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return FKBD_BTN_##tag;
  #define ALIAS(tag,dsttag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return FKBD_BTN_##dsttag;
  #define COMPOUND(tag,btnid) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return (btnid);
  SIMPLE(LEFT)
  SIMPLE(RIGHT)
  SIMPLE(UP)
  SIMPLE(DOWN)
  SIMPLE(SOUTH)
  SIMPLE(WEST)
  SIMPLE(EAST)
  SIMPLE(NORTH)
  SIMPLE(L1)
  SIMPLE(R1)
  SIMPLE(L2)
  SIMPLE(R2)
  SIMPLE(AUX1)
  SIMPLE(AUX2)
  SIMPLE(AUX3)
  SIMPLE(LP)
  SIMPLE(RP)
  ALIAS(QUIT,AUX3)
  COMPOUND(HORZ,FKBD_BTN_LEFT|FKBD_BTN_RIGHT)
  COMPOUND(VERT,FKBD_BTN_UP|FKBD_BTN_DOWN)
  COMPOUND(DPAD,FKBD_BTN_LEFT|FKBD_BTN_RIGHT|FKBD_BTN_UP|FKBD_BTN_DOWN) // We're not supporting hats yet, but we're ready for it here at least.
  #undef SIMPLE
  #undef ALIAS
  #undef COMPOUND
  return 0;
}
