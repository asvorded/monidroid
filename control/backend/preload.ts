import { contextBridge, ipcRenderer } from "electron";

import { ControlPanel } from "../common/control";
import { ControlIpc } from "../common/ipc.types";

const api: ControlPanel = {
  getServerInfo: () => ipcRenderer.invoke(ControlIpc.GetServerConfig),
  getAllClients: () => ipcRenderer.invoke(ControlIpc.GetAllClients),
  getClient: (id) => ipcRenderer.invoke(ControlIpc.GetClient, id),
  setServerState: (options) => ipcRenderer.invoke(ControlIpc.SetServerState, options),
  
  on: (event, cb: (...args: any[]) => void) => {
    ipcRenderer.removeAllListeners(event);
    ipcRenderer.on(event, (_e, ...args) => cb(...args));
  },
  off: (event) => {
    ipcRenderer.removeAllListeners(event);
  },

  shutdown: () => ipcRenderer.send(ControlIpc.Shutdown)
};

contextBridge.exposeInMainWorld("controlPanel", api);