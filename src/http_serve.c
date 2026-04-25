#include "fkbd.h"
#include <fcntl.h>

/* Content-Type
 */
 
static const char *mime_type_from_path(const char *src,int srcc) {
  if (!src) return "application/octet-stream";
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return "application/octet-stream";
  const char *extsrc=src+srcc;
  int extc=0;
  while ((extc<srcc)&&(extsrc[0]!='.')&&(extsrc[0]!='/')) { extsrc--; extc++; }
  if ((extc<2)||(extsrc[0]!='.')) return "application/octet-stream";
  extsrc++;
  extc--;
  char ext[16];
  if (extc>sizeof(ext)) return "application/octet-stream";
  int i=extc; while (i-->0) {
    if ((extsrc[i]>='A')&&(extsrc[i]<='Z')) ext[i]=extsrc[i]+0x20;
    else ext[i]=extsrc[i];
  }
  switch (extc) {
    case 2: {
        if (!memcmp(ext,"js",2)) return "application/javascript";
      } break;
    case 3: {
        if (!memcmp(ext,"css",3)) return "text/css";
        if (!memcmp(ext,"ico",3)) return "image/x-icon";
      } break;
    case 4: {
        if (!memcmp(ext,"html",4)) return "text/html";
      } break;
  }
  return "application/octet-stream";
}

/* POST /heartbeat
 */
 
static int http_serve_heartbeat(struct http_client *client) {
  if (http_client_wbuf_append(client,"HTTP/1.1 222 TODO\r\nContent-Length: 1\r\nContent-Type: text/plain\r\n\r\n",-1)<0) return -1;
  if (g.evdev_fd>=0) {
    if (http_client_wbuf_append(client,"1\r\n",3)<0) return -1;
  } else {
    if (http_client_wbuf_append(client,"0\r\n",3)<0) return -1;
  }
  return 0;
}

/* POST /shutdown
 */
 
static int http_serve_shutdown(struct http_client *client) {
  fprintf(stderr,"%s: Shutting down per POST /shutdown\n",g.exename);
  g.sigc++;
  // Client will not receive it but:
  return http_client_wbuf_append(client,"HTTP/1.1 200 Shutting down\r\nContent-Length: 0\r\n\r\n",-1);
}

/* Encode a device into a JSON encoder.
 */
 
static int http_encode_evdev_device(struct sr_encoder *dst,const struct evdev_device *device) {
  int jsonctx=sr_encode_json_object_start(dst,0,0);
  sr_encode_json_string(dst,"path",4,device->path,-1);
  sr_encode_json_string(dst,"name",4,device->name,-1);
  sr_encode_json_int(dst,"vid",3,device->id.vendor);
  sr_encode_json_int(dst,"pid",3,device->id.product);
  sr_encode_json_int(dst,"version",7,device->id.version);
  int bctx=sr_encode_json_array_start(dst,"buttons",7);
  int major=0;
  for (;major<sizeof(device->btnbit);major++) {
    if (!device->btnbit[major]) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(device->btnbit[major]&(1<<minor))) continue;
      sr_encode_json_int(dst,0,0,(major<<3)|minor);
    }
  }
  sr_encode_json_end(dst,bctx);
  int actx=sr_encode_json_array_start(dst,"axes",4);
  for (major=0;major<sizeof(device->absbit);major++) {
    if (!(device->absbit[major])) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(device->absbit[major]&(1<<minor))) continue;
      int id=(major<<3)|minor;
      int axisctx=sr_encode_json_object_start(dst,0,0);
      sr_encode_json_int(dst,"id",2,id);
      sr_encode_json_int(dst,"lo",2,device->absinfo[id].minimum);
      sr_encode_json_int(dst,"hi",2,device->absinfo[id].maximum);
      sr_encode_json_end(dst,axisctx);
    }
  }
  sr_encode_json_end(dst,actx);
  return sr_encode_json_end(dst,jsonctx);
}

/* POST /devices
 */
 
static int http_serve_devices(struct http_client *client) {
  struct sr_encoder encoder={0};
  sr_encode_json_array_start(&encoder,0,0);
  struct evdev_device *device=g.evdev_devicev;
  int i=g.evdev_devicec;
  for (;i-->0;device++) {
    if (http_encode_evdev_device(&encoder,device)<0) {
      sr_encoder_cleanup(&encoder);
      return -1;
    }
  }
  sr_encode_json_end(&encoder,0);
  if (sr_encoder_assert(&encoder)<0) {
    sr_encoder_cleanup(&encoder);
    return http_client_wbuf_append(client,"HTTP/1.1 500\r\nContent-Length: 0\r\n\r\n",-1);
  }
  if (http_client_wbuf_append(client,"HTTP/1.1 222 TODO\r\nContent-Type: application/json\r\nContent-Length: ",-1)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  char v[16];
  int c=sr_decsint_repr(v,sizeof(v),encoder.c);
  if ((c<1)||(c>sizeof(v))) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  if (http_client_wbuf_append(client,v,c)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  if (http_client_wbuf_append(client,"\r\n\r\n",4)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  if (http_client_wbuf_append(client,encoder.v,encoder.c)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  sr_encoder_cleanup(&encoder);
  if (http_client_wbuf_append(client,"\r\n",2)<0) return -1;
  return 0;
}

/* POST /scan
 */
 
static int http_serve_scan(struct http_client *client) {
  evdev_scan();
  return http_serve_devices(client);
}

/* POST /connect
 */
 
static int http_serve_connect(struct http_client *client,const char *body,int bodyc) {
  fprintf(stderr,"%s '%.*s'\n",__func__,bodyc,body);//TODO
  char path[1024];
  int pathc=0;
  struct sr_decoder decoder={.v=body,.c=bodyc};
  if (sr_decode_json_object_start(&decoder)>=0) {
    const char *k;
    int kc;
    while ((kc=sr_decode_json_next(&k,&decoder))>0) {
      if ((kc==4)&&!memcmp(k,"path",4)) {
        pathc=sr_decode_json_string(path,sizeof(path),&decoder);
        if ((pathc<1)||(pathc>=sizeof(path))) {
          pathc=0;
          path[0]=0;
        }
        break;
      } else {
        if (sr_decode_json_skip(&decoder)<0) break;
      }
    }
  }
  if (pathc) {
    if (fkbd_connect_path(path)<0) {
      return http_client_wbuf_append(client,"HTTP/1.1 500 Failed to connect\r\nContent-Length: 0\r\n\r\n",-1);
    }
  } else {
    fprintf(stderr,"%s:TODO: Disconnect\n",__func__);
  }
  return http_client_wbuf_append(client,"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n",-1);
}

/* Dispatch POST.
 */
 
static int http_serve_post(
  struct http_client *client,
  const char *path,int pathc,
  const char *query,int queryc,
  const void *body,int bodyc
) {
  if ((pathc==10)&&!memcmp(path,"/heartbeat",10)) return http_serve_heartbeat(client);
  if ((pathc==9)&&!memcmp(path,"/shutdown",9)) return http_serve_shutdown(client);
  if ((pathc==5)&&!memcmp(path,"/scan",5)) return http_serve_scan(client);
  if ((pathc==8)&&!memcmp(path,"/devices",8)) return http_serve_devices(client);
  if ((pathc==8)&&!memcmp(path,"/connect",8)) return http_serve_connect(client,body,bodyc);
  return http_client_wbuf_append(client,"HTTP/1.1 404\r\nContent-Length: 0\r\n\r\n",-1);
}

/* Serve GET or HEAD.
 * We have a strict sense of HTTP methods: GET means a static file, and POST means RPC.
 * HEAD of course is just GET with the body lopped off.
 */
 
static int http_serve_get(
  struct http_client *client,
  const char *path,int pathc,
  const char *query,int queryc
) {
  while (pathc&&(path[0]=='/')) { pathc--; path++; }
  while (pathc&&(path[pathc-1]=='/')) pathc--;
  if (!pathc) {
    path="index.html";
    pathc=10;
  }
  int i=pathc-1; while (i-->0) {
    if (!memcmp(path+i,"..",2)) return -1;
  }
  char fullpath[1024];
  int fullpathc=snprintf(fullpath,sizeof(fullpath),"%s/%.*s",g.htdocs,pathc,path);
  if ((fullpathc<1)||(fullpathc>=sizeof(fullpath))) return -1;
  
  // Don't serve directory listings. If we can't read it we won't serve it.
  int fd=open(fullpath,O_RDONLY);
  if (fd<0) return http_client_wbuf_append(client,"HTTP/1.1 404 Not found\r\nContent-Length: 0\r\n\r\n",-1);
  if (http_client_wbuf_append(client,"HTTP/1.1 200 OK\r\nContent-Type: ",-1)<0) return -1;
  if (http_client_wbuf_append(client,mime_type_from_path(fullpath,-1),-1)<0) return -1;
  if (http_client_wbuf_append(client,"\r\nContent-Length: ",-1)<0) return -1;
  int lenp=client->wbufc;
  if (http_client_wbuf_append(client,"         \r\n\r\n",-1)<0) return -1; // If length won't fit in 9 digits, we'll bail out.
  int startp=client->wbufc;
  for (;;) {
    if (http_client_wbuf_require(client)<0) {
      close(fd);
      return -1;
    }
    int rdc=client->wbufa-client->wbufc-client->wbufp;
    int err=read(fd,client->wbuf+client->wbufp+client->wbufc,rdc);
    if (err<0) {
      close(fd);
      return -1;
    }
    client->wbufc+=err;
    if (err<rdc) break;
  }
  close(fd);
  int len=client->wbufc-startp;
  if ((len<0)||(len>999999999)) return -1;
  char *lendst=client->wbuf+client->wbufp+lenp;
  *lendst++=(len>=100000000)?('0'+(len/100000000)%10):' ';
  *lendst++=(len>= 10000000)?('0'+(len/ 10000000)%10):' ';
  *lendst++=(len>=  1000000)?('0'+(len/  1000000)%10):' ';
  *lendst++=(len>=   100000)?('0'+(len/   100000)%10):' ';
  *lendst++=(len>=    10000)?('0'+(len/    10000)%10):' ';
  *lendst++=(len>=     1000)?('0'+(len/     1000)%10):' ';
  *lendst++=(len>=      100)?('0'+(len/      100)%10):' ';
  *lendst++=(len>=       10)?('0'+(len/       10)%10):' ';
  *lendst++='0'+len%10;
  
  if (http_client_wbuf_append(client,"\r\n",2)<0) return -1; // I'm not clear on whether this is required or not.
  
  return 0;
}
 
static int http_serve_head(
  struct http_client *client,
  const char *path,int pathc,
  const char *query,int queryc
) {
  int wbufc0=client->wbufc;
  int err=http_serve_get(client,path,pathc,query,queryc);
  if (err<0) return err;
  // If it's a success response and we can find the header/body separator, cut it off there.
  const char *rsp=client->wbuf+client->wbufp+wbufc0;
  int rspc=client->wbufc-wbufc0;
  if ((rspc>=12)&&!memcmp(rsp,"HTTP/1.1 200",12)) {
    int rspp=0;
    for (;;) {
      if (rspp>rspc-4) break;
      if (!memcmp(rsp+rspp,"\r\n\r\n",4)) {
        client->wbufc=wbufc0+rspp+4;
        return 0;
      }
    }
  }
  return 0;
}

/* Serve request.
 */
 
int http_serve_split(
  struct http_client *client,
  const char *method,int methodc,
  const char *path,int pathc,
  const char *query,int queryc,
  const void *body,int bodyc
) {
  if (!method) methodc=0; else if (methodc<0) { methodc=0; while (method[methodc]) methodc++; }
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (!query) queryc=0; else if (queryc<0) { queryc=0; while (query[queryc]) queryc++; }
  if (!body||(bodyc<0)) bodyc=0;
  int err=-1;
  int wbufc0=client->wbufc;
  if ((methodc==3)&&!memcasecmp(method,"GET",3)) err=http_serve_get(client,path,pathc,query,queryc);
  else if ((methodc==4)&&!memcasecmp(method,"POST",4)) err=http_serve_post(client,path,pathc,query,queryc,body,bodyc);
  else if ((methodc==4)&&!memcasecmp(method,"HEAD",4)) err=http_serve_head(client,path,pathc,query,queryc);
  if (err<0) {
    client->wbufc=wbufc0;
    if (http_client_wbuf_append(client,"HTTP/1.1 500 Error\r\n\r\n",-1)<0) return -1;
  }
  return 0;
}
