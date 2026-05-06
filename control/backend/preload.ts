import { contextBridge, ipcRenderer } from "electron/renderer";

import { ControlPanel, ControlIpc } from "../common/control.types";

const api: ControlPanel = {
  getServerInfo: () => ipcRenderer.invoke(ControlIpc.GetServerConfig),
  getAllClients: () => ipcRenderer.invoke(ControlIpc.GetAllClients),
  getClient: (id) => ipcRenderer.invoke(ControlIpc.GetClient, id),
  forceDisconnect: (id) => ipcRenderer.invoke(ControlIpc.DisconnectClient, id),

  setServerState: (options) => ipcRenderer.invoke(ControlIpc.SetServerState, options),
  
  on: (event, cb: (...args: any[]) => void) => {
    ipcRenderer.removeAllListeners(event);
    ipcRenderer.on(event, (_e, ...args) => cb(...args));
  },
  off: (event) => {
    ipcRenderer.removeAllListeners(event);
  },

  getOptions: () => ipcRenderer.invoke(ControlIpc.GetOptions),
  setOptions: (options) => ipcRenderer.invoke(ControlIpc.SetOptions, options),

  shutdown: () => ipcRenderer.send(ControlIpc.Shutdown)
};

contextBridge.exposeInMainWorld("controlPanel", api);