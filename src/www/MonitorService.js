/* MonitorService.js
 * When enabled, we manage recurring calls to `POST /state` to read current input state, and publish to our subscribers.
 * Note that monitoring is expensive. We're going to call at coarse intervals, and users should not leave us enabled except when needed.
 */
 
import { Comm } from "./Comm.js";

export class MonitorService {
  static getDependencies() {
    return [Comm, Window];
  }
  constructor(comm, window) {
    this.comm = comm;
    this.window = window;
    
    this.nextListenerId = 1;
    this.listeners = []; // {cb,id}
    
    this.enabled = false;
    this.state = null; // Null if disabled, or the last response we got.
  }
  
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ cb, id });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p < 0) return;
    this.listeners.splice(p, 1);
  }
  
  broadcast(event) {
    for (const { cb } of this.listeners) cb(event);
  }
  
  enable(enable) {
    if (enable) {
      if (this.enabled) return;
      this.enabled = true;
      this.callNow();
    } else {
      if (!this.enabled) return;
      this.enabled = false;
      this.state = null;
      this.broadcast(null);
    }
  }
  
  callNow() {
    if (!this.enabled) return;
    this.comm.httpJson("POST", "/state").then(rsp => {
      if (!this.enabled) return;
      if (this.stateChanged(this.state, rsp)) { // Don't broadcast noops.
        this.state = rsp;
        this.broadcast(rsp);
      }
      this.window.setTimeout(() => this.callNow(), 500);
    }).catch(e => console.error(e));
  }
  
  stateChanged(a, b) {
    if (a === b) return false;
    if (!a || !b) return true;
    if (a.logical !== b.logical) return true;
    if (a.axes.length !== b.axes.length) return true;
    if (a.buttons.length !== b.buttons.length) return true;
    for (let i=a.axes.length; i-->0; ) if (a.axes[i] !== b.axes[i]) return true;
    for (let i=a.buttons.length; i-->0; ) if (a.buttons[i] !== b.buttons[i]) return true;
    return false;
  }
}

MonitorService.singleton = true;
