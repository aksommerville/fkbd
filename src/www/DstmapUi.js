/* DstmapUi.js
 * Shows the logical controller and its assignments to the fake keyboard.
 */
 
import { Dom } from "./Dom.js";
import { Comm } from "./Comm.js";
import { DevicesService } from "./DevicesService.js";
import { K } from "./Constants.js";
import { PressKeyModal } from "./PressKeyModal.js";
import { MonitorService } from "./MonitorService.js";

export class DstmapUi {
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
    
    this.dstmap = []; // 32 ints. Linux EV_KEY codes indexed by fkbd button id (ie K.BTNIX_*)
    
    this.buildUi();
    
    this.connectedDeviceListener = this.devicesService.listenConnectedDevice(d => this.onConnect(d));
    this.monitorListener = this.monitorService.listen(e => this.onMonitor(e));
    
    this.comm.httpJson("POST", "/dstmap").then(rsp => {
      this.dstmap = rsp;
      this.resetButtonNames();
    }).catch(e => console.error(e));
  }
  
  onRemoveFromDom() {
    this.devicesService.unlistenConnectedDevice(this.connectedDeviceListener);
    this.monitorService.unlisten(this.monitorListener);
  }
  
  onConnect(device) {
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", "To fake keyboard:");
    /* I've mapped it out as a bunch of 2x1 objects hung on a 12x7 grid, and it all fits nicely.
     * Actually let's call it 9 rows, with gaps below the triggers and above the sticks.
     */
    const gamepad = this.dom.spawn(this.element, "DIV", ["gamepad"]);
    const _ = (ix, x, y) => { // It will occupy (x,y) and (x+1,y).
      const value = `${this.shortIntermediateButtonName(ix)} => ${this.shortKeyName(this.dstmap[ix])}`;
      const field = this.dom.spawn(gamepad, "INPUT", { type: "button", value, "data-index": ix, "on-click": () => this.onClickButton(ix) });
      field.style.gridColumnStart = x + 1; // one-based coords, yuck.
      field.style.gridColumnEnd = x + 3;
      field.style.gridRowStart = y + 1;
      field.style.gridRowEnd = y + 2;
    };
    _(K.BTNIX_LEFT,  0,3);
    _(K.BTNIX_RIGHT, 2,3);
    _(K.BTNIX_UP,    1,2);
    _(K.BTNIX_DOWN,  1,4);
    _(K.BTNIX_SOUTH, 9,4);
    _(K.BTNIX_WEST,  8,3);
    _(K.BTNIX_EAST, 10,3);
    _(K.BTNIX_NORTH, 9,2);
    _(K.BTNIX_L1,    0,0);
    _(K.BTNIX_R1,   10,0);
    _(K.BTNIX_L2,    2,0);
    _(K.BTNIX_R2,    8,0);
    _(K.BTNIX_AUX1,  6,4);
    _(K.BTNIX_AUX2,  4,4);
    _(K.BTNIX_AUX3,  5,2);
    _(K.BTNIX_LP,    2,7);
    _(K.BTNIX_RP,    8,7);
    _(K.BTNIX_LL,    0,7);
    _(K.BTNIX_LR,    4,7);
    _(K.BTNIX_LU,    2,6);
    _(K.BTNIX_LD,    2,8);
    _(K.BTNIX_RL,    6,7);
    _(K.BTNIX_RR,   10,7);
    _(K.BTNIX_RU,    8,6);
    _(K.BTNIX_RD,    8,8);
  }
  
  resetButtonNames() {
    for (const button of this.element.querySelectorAll("input[type='button']")) {
      let ix = button.getAttribute("data-index");
      if (!ix) continue;
      if (isNaN(ix = +ix)) continue;
      button.value = `${this.shortIntermediateButtonName(ix)} => ${this.shortKeyName(this.dstmap[ix])}`;
    }
  }
  
  shortIntermediateButtonName(ix) {
    return [
      "L", "R", "U", "D",
      "S", "W", "E", "N",
      "L1", "R1", "L2", "R2",
      "AUX1", "AUX2", "AUX3", "?",
      "LP", "RP", "?", "?",
      "LL", "LR", "LU", "LD",
      "RL", "RR", "RU", "RD",
    ][ix] || "?";
  }
  
  longIntermediateButtonName(ix) {
    return [
      "Dpad Left", "Dpad Right", "Dpad Up", "Dpad Down",
      "S", "W", "E", "N",
      "L1", "R1", "L2", "R2",
      "AUX1", "AUX2", "AUX3", "?",
      "LP", "RP", "?", "?",
      "LL", "LR", "LU", "LD",
      "RL", "RR", "RU", "RD",
    ][ix] || "?";
  }
  
  shortKeyName(code) {
    return PressKeyModal.nameForKeyCode(code);
  }
  
  onClickButton(ix) {
    ix = +ix;
    if (isNaN(ix) || (ix < 0) || (ix >= 32)) return;
    const modal = this.dom.spawnModal(PressKeyModal);
    modal.setup(this.longIntermediateButtonName(ix), this.dstmap[ix]);
    modal.result.then(rsp => {
      if ((typeof(rsp) !== "number") || (rsp < 0) || (rsp > 0xffff)) return; // Null if cancelled (not reject).
      if (this.dstmap[ix] === rsp) return;
      this.dstmap[ix] = rsp;
      this.resetButtonNames();
      this.comm.httpJson("POST", "/dstmap", null, null, JSON.stringify(this.dstmap)).then(rsp => {
      }).catch(e => console.error(e));
    }).catch(e => console.error(e));
  }
  
  onMonitor(event) {
    /* Use (event.logical) to highlight our buttons.
     */
    for (const element of this.element.querySelectorAll(".monitor")) element.classList.remove("monitor");
    if (event?.logical) {
      for (let ix=0; ix<32; ix++) {
        if (event.logical & (1 << ix)) {
          const element = this.element.querySelector(`input[data-index='${ix}']`);
          if (element) element.classList.add("monitor");
        }
      }
    }
  }
}
