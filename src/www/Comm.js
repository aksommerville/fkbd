/* Comm.js
 * Fetch wrapper.
 * We also manage recurring calls to POST /heartbeat. XXX
 */

/*XXX
const HEARTBEAT_INTERVAL_LIVING = 1000;
const HEARTBEAT_INTERVAL_DEAD =  10000;
/**/
 
export class Comm {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
    
    /*XXX
    this.listeners = []; // { id, cb }
    this.listenerIdNext = 1;
    this.connected = false; // Per heartbeat.
    this.heartbeatCall = null; // Promise|null
    this.heartbeatTimeout = null;
    this.heartbeatInterval = HEARTBEAT_INTERVAL_LIVING;
    this.updateHeartbeat();
    /**/
  }
  
  httpBinary(method, path, query, headers, body) { return this.http(method, path, query, headers, body, "arrayBuffer"); }
  httpText(method, path, query, headers, body) { return this.http(method, path, query, headers, body, "text"); }
  httpJson(method, path, query, headers, body) { return this.http(method, path, query, headers, body, "json"); }
  
  http(method, path, query, headers, body, responseType) {
    const options = {
      method,
    };
    if (headers) options.headers = headers;
    if (body) options.body = body;
    return this.window.fetch(this.composeHttpPath(path, query), options).then(rsp => {
      if (!rsp.ok) throw rsp;
      switch (responseType) {
        case "arrayBuffer": return rsp.arrayBuffer();
        case "text": return rsp.text();
        case "json": return rsp.json();
      }
      return rsp;
    });
  }
  
  composeHttpPath(path, query) {
    if (query) {
      let sep = "?";
      for (const k of Object.keys(query)) {
        path += sep;
        sep = "&";
        path += this.window.encodeURIComponent(k);
        path += "=";
        path += this.window.encodeURIComponent(query[k]);
      }
    }
    return path;
  }
  /*XXX do we really need this heartbeat? i already hate it
  listen(cb) {
    const id = this.listenerIdNext++;
    this.listeners.push({ id, cb });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p < 0) return;
    this.listeners.splice(p, 1);
  }
  
  updateHeartbeat() {
    if (this.heartbeatCall) return; // Already in flight.
    if (this.heartbeatTimeout) return; // Already waiting.
    this.heartbeatTimeout = this.window.setTimeout(() => {
      this.heartbeatTimeout = null;
      this.heartbeatCall = this.httpText("POST", "/heartbeat").then(rsp => {
        this.heartbeatInterval = HEARTBEAT_INTERVAL_LIVING;
        this.setConnected(rsp === "1");
      }).catch(e => {
        console.error(`Heartbeat failed. Assuming server is dead.`);
        this.heartbeatInterval = HEARTBEAT_INTERVAL_DEAD;
        this.setConnected(false);
      }).then(() => {
        this.heartbeatCall = null;
        this.updateHeartbeat();
      });
    }, this.heartbeatInterval);
  }
  
  setConnected(connected) {
    if (connected === this.connected) return;
    this.connected = connected;
    for (const { cb } of this.listeners) cb(connected);
  }
  /**/
}

Comm.singleton = true;
