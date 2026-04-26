/* RootUi.js
 * Our UI shouldn't be so big or complex.
 * Maybe the whole thing will fit in one controller.
 */
 
import { Dom } from "./Dom.js";
import { Comm } from "./Comm.js";
import { DevicesService } from "./DevicesService.js";
import { SrcmapUi } from "./SrcmapUi.js";
import { DstmapUi } from "./DstmapUi.js";

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, Window, "nonce", DevicesService];
  }
  constructor(element, dom, comm, window, nonce, devicesService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.window = window;
    this.nonce = nonce;
    this.devicesService = devicesService;
    
    this.connected = false;
    this.devicePath = "";
    
    this.buildUi();
    
    this.devicesListener = this.devicesService.listen(rsp => this.onDevicesChanged(rsp));
    this.populateDevicesList(this.devicesService.devices);
    
    this.comm.httpText("POST", "/devpath").then(rsp => {
      if (rsp) {
        this.devicePath = rsp;
        this.element.querySelector(`#devices-${this.nonce}`).value = this.devicePath;
        this.onConnected(true);
      } else {
        this.onConnected(false);
      }
    }).catch(e => console.error(e));
  }
  
  onRemoveFromDom() {
    this.devicesService.unlisten(this.devicesListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const topbar = this.dom.spawn(this.element, "DIV", ["topbar"]);
    this.dom.spawn(topbar, "INPUT", { type: "button", value: "Shutdown...", "on-click": () => this.onShutdown() });
    this.dom.spawn(topbar, "INPUT", { type: "button", value: this.connected ? "Disconnect" : "Connect", id: `connect-${this.nonce}`, "on-click": () => this.onConnect() });
    this.dom.spawn(topbar, "INPUT", { type: "button", value: "Scan", "on-click": () => this.onScan() });
    
    const deviceSelect = this.dom.spawn(topbar, "SELECT", { id: `devices-${this.nonce}`, "on-change": () => this.onDeviceSelected() });
    
    const bottom = this.dom.spawn(this.element, "DIV", ["bottom"]);
    const srcmap = this.dom.spawnController(bottom, SrcmapUi);
    const dstmap = this.dom.spawnController(bottom, DstmapUi);
  }
  
  populateDevicesList(devices) {
    const select = this.element.querySelector(`#devices-${this.nonce}`);
    select.innerHTML = "";
    this.dom.spawn(select, "OPTION", { value: "", disabled: "disabled" }, "Choose a device...");
    for (const device of devices) {
      this.dom.spawn(select, "OPTION", { value: device.path }, device.name);
    }
    select.value = this.devicePath;
  }
  
  /* Events.
   *********************************************************/
  
  onShutdown() {
    // "Do you want me to do the thing you just told me to do?" The consequences are severe enough that we'll confirm first.
    const result = this.window.prompt("Really kill the server? Enter anything but empty to confirm.");
    if (!result) return;
    console.log(`*** RootUi killing server per user request. ***`);
    this.comm.httpText("POST", "/shutdown").catch(e => {}); // Very likely to fail. The server doesn't wait for its response to go out before shutting down.
    // Was tempted to navigate to "about:blank", but don't: I might restart the server, and then want to just ctl-R this tab.
    this.element.innerHTML = ""; // Leaving any UI in place would be misleading.
    this.window.document.title = "fkbd - defunct";
  }
  
  onConnect() {
    if (this.connected) {
      return this.comm.httpText("POST", "/connect")
        .then(() => this.onConnected(false))
        .catch(e => console.error(e));
    } else if (this.devicePath) {
      const body = { path: this.devicePath };
      return this.comm.httpText("POST", "/connect", null, null, JSON.stringify(body))
        .then(() => this.onConnected(true))
        .catch(e => {});
    } else {
      return Promise.reject("No device selected");
    }
  }
  
  onConnected(connected) {
    connected = !!connected;
    if (connected === this.connected) return;
    // Button's label indicates whether connected, and also we change document.title. Can see at a glance whether it's on, as I'm browsing games.
    const conbtn = this.element.querySelector(`#connect-${this.nonce}`);
    if (this.connected = connected) {
      this.window.document.title = "FKBD";
      conbtn.value = "Disconnect";
      this.devicesService.setConnectedDevice(this.devicePath);
      //TODO I'm not seeing srcmap when connected for the first time (fresh server)
      // Maybe every time device changes? I've testing with just one. ...YES every time we change device, no need to reset anything to repro.
    } else {
      this.window.document.title = "fkbd";
      conbtn.value = "Connect";
      this.devicesService.setConnectedDevice(null);
    }
  }
  
  onDeviceSelected() {
    const select = this.element.querySelector(`#devices-${this.nonce}`);
    const value = select.value;
    this.devicePath = select.value;
    // Choosing a device implicitly connects it, and disconnects any existing device.
    let connect;
    if (this.connected) {
      connect = this.onConnect().then(() => this.onConnect());
    } else {
      connect = this.onConnect();
    }
    connect.catch(e => console.error(e));
  }
  
  onDevicesChanged(rsp) {
    this.populateDevicesList(rsp);
  }
  
  onScan() {
    this.devicesService.refresh(true);
  }
}
