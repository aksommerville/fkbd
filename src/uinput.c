#include "fkbd.h"

/* Close connections.
 */
 
void uinput_close() {
  if (g.ui_key_fd>=0) {
    close(g.ui_key_fd);
    g.ui_key_fd=-1;
  }
  if (g.ui_mouse_fd>=0) {
    close(g.ui_mouse_fd);
    g.ui_mouse_fd=-1;
  }
}

/* Open connection.
 */
 
int uinput_open() {
  if ((g.ui_key_fd>=0)||(g.ui_mouse_fd>=0)) return -1;
  fprintf(stderr,"%s:%d:TODO %s\n",__FILE__,__LINE__,__func__);//TODO
  return 0;
}
