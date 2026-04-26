#include "fkbd.h"
#include <signal.h>
#include <sys/poll.h>

struct g g={0};

/* Signal handler.
 */
 
static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(g.sigc)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",g.exename);
        exit(1);
      } break;
  }
}

/* Connect device.
 */

int fkbd_connect_path(const char *path) {

  // Disconnect whatever's connected, even if it's the same as the request.
  evdev_disconnect();
  uinput_close();
  
  // If not empty, make a new connection.
  if (path&&path[0]) {
    if (evdev_connect(path)<0) return -1;
    if (uinput_open(path)<0) return -1;
  }
  
  return 0;
}

/* Update one file.
 * We prefer to close and cleanup in response to errors, rather than returning the error.
 * If we return <0 it's fatal.
 */
 
static int update_fdr(int fd) {

  if (fd==g.http_server) {
    return http_accept();
  }
  
  if (fd==g.evdev_fd) {
    int err=evdev_update();
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error reading from evdev device.\n",g.exename);
      evdev_disconnect();
      uinput_close();
    }
    return 0;
  }

  struct http_client *client=g.http_clientv;
  int i=g.http_clientc;
  for (;i-->0;client++) {
    if (client->fd!=fd) continue;
    int err=http_client_read(client);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Closing http client %d due to failed read.\n",g.exename,client->fd);
      close(client->fd);
      client->fd=-1;
    }
    return 0;
  }

  return 0;
}

static int update_fdw(int fd) {
  /* Only (http_client) poll for writing.
   * (uinput_fd) is also writeable, but we block on it.
   */
  struct http_client *client=g.http_clientv;
  int i=g.http_clientc;
  for (;i-->0;client++) {
    if (client->fd!=fd) continue;
    int err=http_client_write(client);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Closing http client %d due to failed write.\n",g.exename,client->fd);
      close(client->fd);
      client->fd=-1;
    }
    return 0;
  }
  return 0;
}

/* Update.
 */
 
static int update() {
  #define FDLIMIT 8
  struct pollfd pollfdv[FDLIMIT]={0};
  int pollfdc=0;
  
  if (g.evdev_fd>=0) pollfdv[pollfdc++]=(struct pollfd){.fd=g.evdev_fd,.events=POLLIN|POLLERR|POLLHUP};
  
  if (g.http_server>=0) pollfdv[pollfdc++]=(struct pollfd){.fd=g.http_server,.events=POLLIN|POLLERR|POLLHUP};
  
  struct http_client *client=g.http_clientv;
  int i=g.http_clientc,defunct=0;
  for (;i-->0;client++) {
    if (client->fd<0) {
      defunct=1;
      continue;
    }
    if (pollfdc>=FDLIMIT) break;
    struct pollfd *pollfd=pollfdv+pollfdc++;
    pollfd->fd=client->fd;
    if (client->wbufc>0) {
      pollfd->events=POLLOUT|POLLERR|POLLHUP;
    } else {
      pollfd->events=POLLIN|POLLERR|POLLHUP;
    }
  }
  
  if (defunct) {
    for (client=g.http_clientv+g.http_clientc-1,i=g.http_clientc;i-->0;client--) {
      if (client->fd>=0) continue;
      http_client_cleanup(client);
      g.http_clientc--;
      memmove(client,client+1,sizeof(struct http_client)*(g.http_clientc-i));
    }
  }
  
  // Nothing for (g.uinput_fd); it is write-only.
  
  if (pollfdc<1) {
    fprintf(stderr,"%s: All connections closed.\n",g.exename);
    return -2;
  }
  int err=poll(pollfdv,pollfdc,1000);
  if (err<1) return 0;
  
  struct pollfd *pollfd=pollfdv;
  for (i=pollfdc;i-->0;pollfd++) {
    if (pollfd->revents&(POLLIN|POLLERR|POLLHUP)) err=update_fdr(pollfd->fd);
    else if (pollfd->revents&POLLOUT) err=update_fdw(pollfd->fd);
    else continue;
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error updating fd %d\n",g.exename,pollfd->fd);
      return -2;
    }
  }
  
  #undef FDLIMIT
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  int err;
  
  g.exename="fkbd";
  g.htdocs="src/www"; // XXX We can only run from this project's directory.
  g.evdev_devices_path="/dev/input";
  g.http_port=4444;
  g.http_server=-1;
  g.evdev_fd=-1;
  g.ui_key_fd=-1;
  g.ui_mouse_fd=-1;
  
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) g.exename=argv[0];
  
  signal(SIGINT,rcvsig);
  
  dstmap_init();
  
  if ((err=http_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing HTTP server.\n",g.exename);
    return 1;
  }
  
  if ((err=evdev_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing evdev.\n",g.exename);
    return 1;
  }
  
  while (!g.sigc) {
    if ((err=update())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error updating program.\n",g.exename);
      return 1;
    }
  }
  
  fprintf(stderr,"%s: Normal exit.\n",g.exename);
  return 0;
}
