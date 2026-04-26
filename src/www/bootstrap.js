import { Injector } from "./Injector.js";
import { Dom } from "./Dom.js";
import { Comm } from "./Comm.js";
import { RootUi } from "./RootUi.js";

/* Startup.
 */

window.addEventListener("load", () => {
  const injector = new Injector(window);
  const dom = injector.instantiate(Dom);
  const comm = injector.instantiate(Comm);
  document.body.innerHTML = "";
  const rootUi = dom.spawnController(document.body, RootUi);
}, { once: true });
