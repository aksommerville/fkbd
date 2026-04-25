#include "fkbd.h"
#include <sys/socket.h>
#include <arpa/inet.h>

/* Initialize global service.
 */
 
int http_init() {
  if (g.http_server>=0) return -1;
  
  g.http_server=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if (g.http_server<0) {
    fprintf(stderr,"%s: Failed to create server socket.\n",g.exename);
    return -2;
  }
  
  int one=1;
  setsockopt(g.http_server,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
  
  struct sockaddr_in saddr={
    .sin_family=AF_INET,
    .sin_port=htons(g.http_port),
  };
  memcpy(&saddr.sin_addr,"\x7f\x00\x00\x01",4);
  if (bind(g.http_server,(struct sockaddr*)&saddr,sizeof(saddr))<0) {
    fprintf(stderr,"%s: Failed to bind server to localhost:%d\n",g.exename,g.http_port);
    return -2;
  }
  if (listen(g.http_server,10)<0) {
    fprintf(stderr,"%s: Failed to open server on port %d.\n",g.exename,g.http_port);
    return -2;
  }
  fprintf(stderr,"%s: Serving HTTP on port %d, localhost only.\n",g.exename,g.http_port);
  
  return 0;
}

/* Accept one incoming connection.
 */

int http_accept() {

  int fd=accept(g.http_server,0,0);
  if (fd<0) {
    fprintf(stderr,"%s: accept() failed\n",g.exename);
    return -2;
  }
  
  if (g.http_clientc>=g.http_clienta) {
    int na=g.http_clienta+8;
    if (na>INT_MAX/sizeof(struct http_client)) { close(fd); return -1; }
    void *nv=realloc(g.http_clientv,sizeof(struct http_client)*na);
    if (!nv) { close(fd); return -1; }
    g.http_clientv=nv;
    g.http_clienta=na;
  }
  
  struct http_client *client=g.http_clientv+g.http_clientc++;
  memset(client,0,sizeof(struct http_client));
  client->fd=fd;
  
  return 0;
}

/* Clean up client.
 */
 
void http_client_cleanup(struct http_client *client) {
  if (client->fd>=0) close(client->fd);
  if (client->rbuf) free(client->rbuf);
  if (client->wbuf) free(client->wbuf);
}

/* Read into client.
 */
 
int http_client_read(struct http_client *client) {
  if (client->fd<0) return -1;
  
  if (client->rbufp+client->rbufc>=client->rbufa) {
    if (client->rbufp) {
      memmove(client->rbuf,client->rbuf+client->rbufp,client->rbufc);
      client->rbufp=0;
    } else {
      if (client->rbufa>0x01000000) return -1; // nope, just nope.
      int na=client->rbufa+8192;
      void *nv=realloc(client->rbuf,na);
      if (!nv) return -1;
      client->rbuf=nv;
      client->rbufa=na;
    }
  }
  
  int err=read(client->fd,client->rbuf+client->rbufp+client->rbufc,client->rbufa-client->rbufc-client->rbufp);
  if (err<=0) {
    if (!client->rbufc) { // Closed with nothing buffered: Close quietly.
      close(client->fd);
      client->fd=-1;
      return 0;
    }
    return -1;
  }
  client->rbufc+=err;
  
  while (client->rbufc>0) {
    int srcc=http_request_measure(client->rbuf+client->rbufp,client->rbufc);
    if (srcc<1) break;
    int err=http_serve(client,client->rbuf+client->rbufp,srcc);
    if (err<0) return err;
    client->rbufp+=srcc;
    client->rbufc-=srcc;
  }

  return 0;
}

/* Write from client.
 */
 
int http_client_write(struct http_client *client) {
  if (client->fd<0) return -1;
  if (client->wbufc<1) return 0;
  int err=write(client->fd,client->wbuf+client->wbufp,client->wbufc);
  if (err<=0) return -1;
  if ((client->wbufc-=err)<=0) {
    client->wbufp=0;
    client->wbufc=0;
  } else {
    client->wbufp+=err;
  }
  return 0;
}

/* Measure line.
 */
 
int http_measure_line(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<2) return 0;
  int srcp=0;
  while (srcp<srcc) {
    if (src[srcp++]==0x0a) return srcp; // Assume there was a CR in front of it. Whatever.
  }
  return 0; // No LF.
}

/* Case-insensitive memcmp.
 */
 
int memcasecmp(const char *a,const char *b,int c) {
  for (;c-->0;a++,b++) {
    char cha=*a; if ((cha>=0x41)&&(cha<=0x5a)) cha+=0x20;
    char chb=*b; if ((chb>=0x41)&&(chb<=0x5a)) chb+=0x20;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
  return 0;
}

/* Strip header key and return value as unsigned decimal integer.
 */
 
int http_read_header_int(const char *src,int srcc) {
  if (srcc<1) return -1;
  while (srcc&&(*src++!=':')) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  if (srcc<1) return -1;
  int v=0;
  for (;srcc-->0;src++) {
    if ((*src<'0')||(*src>'9')) return -1;
    v*=10;
    v+=(*src)-'0';
    if (v>999999999) return -1;
  }
  return v;
}

/* Measure HTTP request.
 */

int http_request_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; // Skip any leading space, esp newlines.
  // Require and retain Request-Line.
  const char *rline=src+srcp;
  int rlinec=http_measure_line(src+srcp,srcc-srcp);
  if (rlinec<1) return 0; // Request-Line incomplete.
  srcp+=rlinec;
  // Read each header line until we find a blank. Capture Content-Length.
  // We do not allow chunked encoding or compression.
  int bodylen=0;
  for (;;) {
    const char *hline=src+srcp;
    int hlinec=http_measure_line(src+srcp,srcc-srcp);
    if (hlinec<1) return 0; // Headers incomplete.
    srcp+=hlinec;
    while (hlinec&&((unsigned char)hline[0]<=0x20)) { hline++; hlinec--; }
    while (hlinec&&((unsigned char)hline[hlinec-1]<=0x20)) hlinec--;
    if (!hlinec) break; // A line of all whitespace (ie just the newline) means end of headers.
    if (!bodylen&&(hlinec>=14)&&!memcasecmp(hline,"Content-Length",14)) {
      if ((bodylen=http_read_header_int(hline,hlinec))<0) return -1;
    }
  }
  if (bodylen) {
    if (srcp>srcc-bodylen) return 0; // Body incomplete.
    srcp+=bodylen;
    // And there's usually whitespace after the body. I don't know what the spec says about that, but we trim it at the start.
  }
  return srcp;
}

/* Serve HTTP request.
 * (src) is normally resident in our (rbuf) during this call, so don't touch rbuf!
 * On success, the response should be queued into (wbuf).
 * All service is synchronous.
 */
 
int http_serve(struct http_client *client,const char *src,int srcc) {
  if (!client||!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; // Skip any leading space, esp newlines.
  // Require and retain Request-Line.
  const char *rline=src+srcp;
  int rlinec=http_measure_line(src+srcp,srcc-srcp);
  if (rlinec<1) return -1; // Request-Line incomplete.
  srcp+=rlinec;
  // Read each header line until we find a blank. Capture Content-Length.
  // We do not allow chunked encoding or compression.
  // No other header is interesting.
  int bodylen=0;
  const void *body=0;
  for (;;) {
    const char *hline=src+srcp;
    int hlinec=http_measure_line(src+srcp,srcc-srcp);
    if (hlinec<1) return -1; // Headers incomplete.
    srcp+=hlinec;
    while (hlinec&&((unsigned char)hline[0]<=0x20)) { hline++; hlinec--; }
    while (hlinec&&((unsigned char)hline[hlinec-1]<=0x20)) hlinec--;
    if (!hlinec) break; // A line of all whitespace (ie just the newline) means end of headers.
    if (!bodylen&&(hlinec>=14)&&!memcasecmp(hline,"Content-Length",14)) {
      if ((bodylen=http_read_header_int(hline,hlinec))<0) return -1;
    }
  }
  if (bodylen) {
    if (srcp>srcc-bodylen) return -1; // Body incomplete.
    body=src+srcp;
  }
  // Split (rline) into method, path, and query. Discard protocol.
  int rlinep=0;
  const char *method=rline+rlinep;
  int methodc=0;
  while ((rlinep<rlinec)&&((unsigned char)rline[rlinep++]>0x20)) methodc++;
  while ((rlinep<rlinec)&&((unsigned char)rline[rlinep]<=0x20)) rlinep++;
  const char *path=rline+rlinep;
  int pathc=0;
  while ((rlinep<rlinec)&&(rline[rlinep]!='?')&&((unsigned char)rline[rlinep++]>0x20)) pathc++;
  const char *query=0;
  int queryc=0;
  if ((rlinep<rlinec)&&(rline[rlinep]=='?')) {
    rlinep++;
    query=rline+rlinep;
    while ((rlinep<rlinec)&&((unsigned char)rline[rlinep++]>0x20)) queryc++;
  }
  return http_serve_split(client,method,methodc,path,pathc,query,queryc,body,bodylen);
}

/* Append to output buffer.
 */
 
int http_client_wbuf_append(struct http_client *client,const char *src,int srcc) {
  if (!client) return -1;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (client->wbufp+client->wbufc>client->wbufa-srcc) {
    if (client->wbufp) {
      memmove(client->wbuf,client->wbuf+client->wbufp,client->wbufc);
      client->wbufp=0;
    }
    if (client->wbufc>client->wbufa-srcc) {
      if (client->wbufc>INT_MAX-srcc) return -1;
      int na=client->wbufc+srcc;
      if (na<INT_MAX-1024) na=(na+1024)&~1023;
      void *nv=realloc(client->wbuf,na);
      if (!nv) return -1;
      client->wbuf=nv;
      client->wbufa=na;
    }
  }
  memcpy(client->wbuf+client->wbufp+client->wbufc,src,srcc);
  client->wbufc+=srcc;
  return 0;
}

int http_client_wbuf_require(struct http_client *client) {
  if (!client) return -1;
  if (client->wbufp+client->wbufc<client->wbufa) return 0;
  if (client->wbufp) {
    memmove(client->wbuf,client->wbuf+client->wbufp,client->wbufc);
    client->wbufp=0;
  } else {
    if (client->wbufa>0x01000000) return -1;
    int na=client->wbufa+8192;
    void *nv=realloc(client->wbuf,na);
    if (!nv) return -1;
    client->wbuf=nv;
    client->wbufa=na;
  }
  return 0;
}
