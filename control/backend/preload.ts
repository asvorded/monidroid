import { app, BrowserWindow, contextBridge } from "electron";

import { ControlPanel } from "../control"

const api: ControlPanel = {
  shutdown: () => {
    app.quit();
  },
}

contextBridge.exposeInMainWorld("control", api);
