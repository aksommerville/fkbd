/* DevicesService.js
 * Caches the list of devices.
 */
 
import { Comm } from "./Comm.js";

export class DevicesService {
  static getDependencies() {
    return [Comm];
  }
  constructor(comm) {
    this.comm = comm;
    
    this.nextListenerId = 1;
    this.listeners = []; // {cb,id}
    this.devices = [];
    this.connectedDevice = null;
    this.connectedDeviceListeners = []; // {cb,id}
    
    this.refresh(false);
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
  
  broadcast() {
    for (const { cb } of this.listeners) cb(this.devices);
  }
  
  refresh(hard) {
    this.comm.httpJson("POST", hard ? "/scan" : "/devices").then(rsp => {
      this.devices = rsp;
      this.broadcast();
    }).catch(e => console.error(e));
  }
  
  listenConnectedDevice(cb) {
    const id = this.nextListenerId++;
    this.connectedDeviceListeners.push({ cb, id });
    return id;
  }
  
  unlistenConnectedDevice(id) {
    const p = this.connectedDeviceListeners.findIndex(l => l.id === id);
    if (p < 0) return;
    this.connectedDeviceListeners.splice(p, 1);
  }
  
  broadcastConnectedDevice() {
    for (const { cb } of this.connectedDeviceListeners) cb(this.connectedDevice);
  }
  
  setConnectedDevice(path) {
    if (path) {
      this.connectedDevice = this.devices.find(d => d.path === path);
    } else {
      this.connectedDevice = null;
    }
    this.broadcastConnectedDevice();
  }
}

DevicesService.singleton = true;
