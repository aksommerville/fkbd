/* PressKeyModal.js
 */
 
import { Dom } from "./Dom.js";
import { K } from "./Constants.js";

export class PressKeyModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.index = 0;
    this.keycode = 0;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(srcname, keycode) {
    this.srcname = srcname;
    this.keycode = keycode;
    
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", `Pick output for ${this.srcname}, currently key ${this.keycode}`);
    
    /* A US keyboard fits pretty nice in a 27x7 table, with some margins and 3 mouse buttons on the right.
     * TODO I don't think "210" in the top row is right. That's Print Screen.
     */
    const table = this.dom.spawn(this.element, "TABLE", { "on-click": e => this.onClick(e) });
    this.spawnKeys(table, [1, 0, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 87, 88, 0, 210, 70, 119]);
    this.spawnKeys(table, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 69, 98, 55, 74]);
    this.spawnKeys(table, [41, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0, 110, 102, 104, 0, 71, 72, 73, [78, 1, 2], 0, [272, 1, 2], [274, 1, 2], [273, 1, 2]]);
    this.spawnKeys(table, [15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 43, 0, 111, 107, 109, 0, 75, 76, 77]);
    this.spawnKeys(table, [58, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, [28, 2, 1], 0, 0, 0, 0, 0, 79, 80, 81, [96, 1, 2]]);
    this.spawnKeys(table, [[42, 2, 1], 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, [54, 2, 1], 0, 0, 103, 0, 0, [82, 2, 1], 83]);
    this.spawnKeys(table, [[29, 2, 1], [125, 2, 1], [56, 2, 1], [57, 4, 1], [100, 2, 1], [97, 2, 1], 0, 105, 108, 106]);
  }
  
  /* (keys) is an array of integer (linux key code), or [keycode, width, height] if not (1,1).
   */
  spawnKeys(table, keys) {
    const tr = this.dom.spawn(table, "TR");
    for (const key of keys) {
      let code=key, w=1, h=1;
      if (key instanceof Array) {
        code = key[0];
        w = key[1];
        h = key[2];
      }
      if (!code) { // Code zero or undefined means a placeholder.
        this.dom.spawn(tr, "TD");
        continue;
      }
      const attr = {
        "data-code": code,
      };
      if (w > 1) attr.colspan = w;
      if (h > 1) attr.rowspan = h; //TODO is rowspan a real thing?
      const css = ["key"];
      if (code === this.keycode) css.push("highlight");
      const td = this.dom.spawn(tr, "TD", css, attr, PressKeyModal.nameForKeyCode(code));
    }
  }
  
  static nameForKeyCode(code) {
    // The Linux key codes don't have much rhyme or reason. I think they're based on some ancient protocol passed down by the elders.
    return [
      null, "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "BS", "TAB",
      "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "ENT", "CTL", "A", "S",
      "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", "SHF", "\\", "Z", "X", "C", "V",
      "B", "N", "M", ",", ".", "/", "SHF", "*", "ALT", "SPC", "CAP", "F1", "F2", "F3", "F4", "F5",
      "F6", "F7", "F8", "F9", "F10", "NL", "SL", "7", "8", "9", "-", "4", "5", "6", "+", "1",
      "2", "3", "0", ".", null, null, null, "F11", "F12", null, null, null, null, null, null, null,
      "ENT", "CTL", "/", "SRQ", "ALT", "LF", "HOME", "U", "PU", "L", "R", "END", "D", "PD", "INS", "DEL",
    ][code] || (code || 0).toString();
  }
  
  onClick(event) {
    const target = event?.target;
    if (!target?.classList?.contains("key")) return;
    const code = +target.getAttribute("data-code");
    if (isNaN(code)) return;
    this.resolve(code);
    this.element.remove();
  }
}
