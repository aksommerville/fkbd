/* SrcmapUi.js
 * Shows caps for the evdev device and allows mapping to the logical gamepad.
 */
 
import { Dom } from "./Dom.js";
import { Comm } from "./Comm.js";
import { DevicesService } from "./DevicesService.js";
import { K } from "./Constants.js";
import { MonitorService } from "./MonitorService.js";

export class SrcmapUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, DevicesService, "nonce", MonitorService];
  }
  constructor(element, dom, comm, devicesService, nonce, monitorService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.devicesService = devicesService;
    this.nonce = nonce;
    this.monitorService = monitorService;
    
    this.srcmap = [];
    this.buildUi();
    this.populateUi();
    
    this.connectedDeviceListener = this.devicesService.listenConnectedDevice(d => this.onConnect(d));
    this.monitorListener = this.monitorService.listen(e => this.onMonitor(e));
    this.refreshSrcmap();
  }
  
  onRemoveFromDom() {
    this.devicesService.unlistenConnectedDevice(this.connectedDeviceListener);
    this.monitorService.unlisten(this.monitorListener);
  }
  
  refreshSrcmap() {
    this.comm.httpJson("POST", "/srcmap").then(rsp => {
      this.srcmap = rsp;
      this.populateOutputs(rsp);
    }).catch(e => console.error(e));
  }
  
  onConnect(device) {
    this.populateUi();
    this.refreshSrcmap();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", ["deviceName"]);
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "Publish", "on-click": () => this.onPublish() });
    this.dom.spawn(this.element, "DIV", ["caps"]);
  }
  
  populateUi() {
    const device = this.devicesService.connectedDevice;
    this.element.querySelector(".deviceName").innerText = device?.name || "";
    const caps = this.element.querySelector(".caps");
    caps.innerHTML = "";
    if (device) {
      for (let i=0; i<device.axes.length; i++) {
        const axis = device.axes[i];
        const row = this.dom.spawn(caps, "DIV", ["row", "axis"], { "data-index": i });
        this.dom.spawn(row, "DIV", ["capName"], `Axis ${axis.id} ${axis.lo}..${axis.hi}`);
        const select = this.dom.spawn(row, "SELECT", {
          id: `axis-${this.nonce}-${axis.id}-dstbtnid`,
          "data-index": i,
          "data-srctype": K.EV_ABS,
          "data-code": axis.id,
          "data-lo": axis.lo, // Note these are the button's full reported range, not the mappings' srclo,srchi.
          "data-hi": axis.hi,
        });
        this.dom.spawn(select, "OPTION", { value: 0 }, "None");
        this.dom.spawn(select, "OPTION", { value: K.BTN_LEFT | K.BTN_RIGHT }, "Dpad Horz");
        this.dom.spawn(select, "OPTION", { value: K.BTN_UP | K.BTN_DOWN }, "Dpad Vert");
        this.dom.spawn(select, "OPTION", { value: K.BTN_LL | K.BTN_LR } , "LX");
        this.dom.spawn(select, "OPTION", { value: K.BTN_LU | K.BTN_LD } , "LY");
        this.dom.spawn(select, "OPTION", { value: K.BTN_RL | K.BTN_RR } , "RX");
        this.dom.spawn(select, "OPTION", { value: K.BTN_RU | K.BTN_RD } , "RY");
        // Axes can also map to single bits, eg analogue trigger buttons. I don't use any devices like that.
      }
      for (let i=0; i<device.buttons.length; i++) {
        const btnid = device.buttons[i];
        const row = this.dom.spawn(caps, "DIV", ["row", "button"], { "data-index": i });
        this.dom.spawn(row, "DIV", ["capName"], `Button ${btnid}`);
        const select = this.dom.spawn(row, "SELECT", {
          id: `btn-${this.nonce}-${btnid}-dstbtnid`,
          "data-index": i,
          "data-srctype": K.EV_KEY,
          "data-code": btnid,
        });
        this.dom.spawn(select, "OPTION", { value: 0 }, "None");
        this.dom.spawn(select, "OPTION", { value: K.BTN_LEFT }, "Left");
        this.dom.spawn(select, "OPTION", { value: K.BTN_RIGHT }, "Right");
        this.dom.spawn(select, "OPTION", { value: K.BTN_UP }, "Up");
        this.dom.spawn(select, "OPTION", { value: K.BTN_DOWN }, "Down");
        this.dom.spawn(select, "OPTION", { value: K.BTN_SOUTH }, "South");
        this.dom.spawn(select, "OPTION", { value: K.BTN_WEST }, "West");
        this.dom.spawn(select, "OPTION", { value: K.BTN_EAST }, "East");
        this.dom.spawn(select, "OPTION", { value: K.BTN_NORTH }, "North");
        this.dom.spawn(select, "OPTION", { value: K.BTN_L1 }, "L1");
        this.dom.spawn(select, "OPTION", { value: K.BTN_R1 }, "R1");
        this.dom.spawn(select, "OPTION", { value: K.BTN_L2 }, "L2");
        this.dom.spawn(select, "OPTION", { value: K.BTN_R2 }, "R2");
        this.dom.spawn(select, "OPTION", { value: K.BTN_AUX1 }, "Aux 1");
        this.dom.spawn(select, "OPTION", { value: K.BTN_AUX2 }, "Aux 2");
        this.dom.spawn(select, "OPTION", { value: K.BTN_AUX3 }, "Aux 3");
        this.dom.spawn(select, "OPTION", { value: K.BTN_LP }, "LP");
        this.dom.spawn(select, "OPTION", { value: K.BTN_RP }, "RP");
        // Doesn't make sense to offer LL et al; those are only sourced from analogue inputs.
      }
    }
  }
  
  populateOutputs(maps) {
    /* (maps) and our fields don't line up 1:1. There can be more than one map per field, eg for two-way axes.
     * So iterate all the fields, and for each, iterate the whole (maps) to collect the proper value for reporting.
     */
    for (const select of this.element.querySelectorAll("select")) {
      const srctype = +select.getAttribute("data-srctype");
      const code = +select.getAttribute("data-code");
      const value = this.combinedValueFromMaps(maps, srctype, code);
      select.value = value;
    }
  }
  
  combinedValueFromMaps(maps, type, code) {
    let v = 0;
    for (const map of maps) {
      if (map.type !== type) continue;
      if (map.code !== code) continue;
      v += map.dst;
    }
    return v;
  }
  
  isSingleBit(src) {
    if (!src) return false;
    let c = 0;
    for (let i=32; i-->0; src>>=1) {
      if (src & 1) {
        if (++c > 1) return false;
      }
    }
    return !!c;
  }
  
  onPublish() {
    const request = []; // {type,code,srclo,srchi,dst}
    for (const select of this.element.querySelectorAll("select")) {
      const type = +select.getAttribute("data-srctype");
      const code = +select.getAttribute("data-code");
      const dst = +select.value;
      if (isNaN(type) || isNaN(code) || isNaN(dst)) continue;
      const hwlo = +(select.getAttribute("data-lo") || -2147483648);
      const hwhi = +(select.getAttribute("data-hi") || 2147483647);
      switch (dst) { // Pick off the allowed multibit values.
        case (K.BTN_LEFT | K.BTN_RIGHT):
        case (K.BTN_UP | K.BTN_DOWN):
        case (K.BTN_LL | K.BTN_LR):
        case (K.BTN_LU | K.BTN_LD):
        case (K.BTN_RL | K.BTN_RR):
        case (K.BTN_RU | K.BTN_RD): {
            const dstlo = dst & (K.BTN_LEFT | K.BTN_UP | K.BTN_LL | K.BTN_LU | K.BTN_RL | K.BTN_RU);
            const dsthi = dst & (K.BTN_RIGHT | K.BTN_DOWN | K.BTN_LR | K.BTN_LD | K.BTN_RR | K.BTN_RD);
            const mid = (hwlo + hwhi) >> 1;
            let midlo = (hwlo + mid) >> 1;
            let midhi = (hwhi + mid) >> 1;
            if (midlo >= mid) midlo = mid - 1;
            if (midhi <= mid) midhi = mid + 1;
            request.push({
              type,
              code,
              srclo: -2147483648,
              srchi: midlo,
              dst: dstlo,
            });
            request.push({
              type,
              code,
              srclo: midhi,
              srchi: 2147483647,
              dst: dsthi,
            });
          } break;
        default: if (this.isSingleBit(dst)) {
            const srclo = 1;
            const srchi = 2147483647;
            request.push({ type, code, srclo, srchi, dst });
          } break;
      }
    }
    this.comm.httpJson("POST", "/srcmap", null, null, JSON.stringify(request)).then(rsp => {
      this.devicesService.refresh(); // We have the new content in (rsp) but meh, this is cleaner.
    }).catch(e => console.error(e));
  }
  
  onMonitor(event) {
    /* (event.axes) and (event.buttons) should be the same length as our axis and button rows.
     * Highlight the key for each nonzero field.
     * "nonzero" might not be correct for axes. I'll try not to worry about it.
     */
    for (const element of this.element.querySelectorAll(".monitor")) element.classList.remove("monitor");
    if (event) {
      if (event.axes) {
        for (let ix=event.axes.length; ix-->0; ) {
          if (event.axes[ix]) {
            const row = this.element.querySelector(`.row.axis[data-index='${ix}']`);
            if (row) row.classList.add("monitor");
          }
        }
      }
      if (event.buttons) {
        for (let ix=event.buttons.length; ix-->0; ) {
          if (event.buttons[ix]) {
            const row = this.element.querySelector(`.row.button[data-index='${ix}']`);
            if (row) row.classList.add("monitor");
          }
        }
      }
    }
  }
}
