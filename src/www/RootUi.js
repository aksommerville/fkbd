/* RootUi.js
 * Our UI shouldn't be so big or complex.
 * Maybe the whole thing will fit in one controller.
 */
 
import { Dom } from "./Dom.js";
import { Comm } from "./Comm.js";

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, Window, "nonce"];
  }
  constructor(element, dom, comm, window, nonce) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.window = window;
    this.nonce = nonce;
    
    this.connected = false;
    this.devicePath = "";
    
    this.buildUi();
    
    //this.heartbeatListener = this.comm.listen(connected => this.onHeartbeat(connected));
    
    this.refreshDeviceList();
  }
  
  onRemoveFromDom() {
    //this.comm.unlisten(this.heartbeatListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const topbar = this.dom.spawn(this.element, "DIV", ["topbar"]);
    this.dom.spawn(topbar, "INPUT", { type: "button", value: "Shutdown...", "on-click": () => this.onShutdown() });
    this.dom.spawn(topbar, "INPUT", { type: "button", value: this.connected ? "Disconnect" : "Connect", id: `connect-${this.nonce}`, "on-click": () => this.onConnect() });
    
    const deviceSelect = this.dom.spawn(topbar, "SELECT", { id: `devices-${this.nonce}`, "on-change": () => this.onDeviceSelected() });
  }
  
  refreshDeviceList() {
    this.comm.httpJson("POST", "/devices").then(rsp => {
      console.log(`TODO RootUi repopulate device list`, rsp);
      const select = this.element.querySelector(`#devices-${this.nonce}`);
      select.innerHTML = "";
      this.dom.spawn(select, "OPTION", { value: "", disabled: "disabled" }, "Choose a device...");
      for (const device of rsp) {
        this.dom.spawn(select, "OPTION", { value: device.path }, device.name);
      }
      select.value = this.devicePath;
    }).catch(e => console.error(e));
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
      this.comm.httpText("POST", "/connect")
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
    const conbtn = this.element.querySelector(`#connect-${this.nonce}`);
    if (this.connected = connected) {
      this.window.document.title = "FKBD";
      conbtn.value = "Disconnect";
    } else {
      this.window.document.title = "fkbd";
      conbtn.value = "Connect";
    }
  }
  
  onDeviceSelected() {
    const select = this.element.querySelector(`#devices-${this.nonce}`);
    const value = select.value;
    this.devicePath = select.value;
    // Choosing a device implicitly connects it, and disconnects any existing device.
    if (this.connected) {
      this.onConnect().then(() => this.onConnect());
    } else {
      this.onConnect();
    }
  }
}
