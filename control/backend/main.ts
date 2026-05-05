import { app, BrowserWindow, ipcMain, Menu, Notification, Tray } from "electron";
import path from "path";
import service from "./wsservice";
import { ControlIpc, IpcMessages, PanelOptions } from "../common/control.types";
import { ServerStateOptions } from "../common/wsservice.types";

let tray: Tray;

function createWindow(at: string) {
  const window = new BrowserWindow({
    titleBarStyle: "hidden",
    titleBarOverlay: {
      height: 32,
    },
    hasShadow: true,
    webPreferences: {
      preload: path.join(__dirname, "preload.js"),
    }
  });
  if (!app.isPackaged) {
    window.loadURL('http://localhost:14769' + at);
  } else {
    window.loadFile("index.html" + at);
  }
}

function openWindow(at: string) {
  const wins = BrowserWindow.getAllWindows()
  if (wins.length === 0) {
    createWindow("");
  } else {
    wins[0].focus();
  }
}

function emitToWindows(msg: IpcMessages, ...args: any[]) {
  for (const w of BrowserWindow.getAllWindows()) {
    w.webContents.send(msg, ...args);
  }
}

function initTray() {
  tray = new Tray(path.join(app.getAppPath(), "static", "logo.png"));
  tray.setToolTip("Monidroid control panel");
  tray.setContextMenu(Menu.buildFromTemplate([
    {
      label: "Open panel",
      click: () => openWindow(""),
    },
    {
      label: "Settings...",
      click: () => openWindow("/settings"),
    },
    { type: "separator" },
    {
      label: "Exit panel",
      click: () => {
        app.quit();
      }
    },
    {
      label: "Shutdown",
      click: () => {
        service.shutdown();
        app.quit();
      }
    },
  ]));
}

app.whenReady().then(() => {
  // Request handlers
  ipcMain.handle(ControlIpc.GetServerConfig, () => service.getServerInfo());
  ipcMain.handle(ControlIpc.GetAllClients, () => service.getAllClients());
  ipcMain.handle(ControlIpc.GetClient, (_, id: string) => service.getClient(id));
  ipcMain.handle(ControlIpc.SetServerState, (_, options: ServerStateOptions) => service.setServerState(options));
  ipcMain.on(ControlIpc.Shutdown, () => {
    service.shutdown();
    app.quit();
  });

  // Event hadlers
  service.onConnected = () => emitToWindows(ControlIpc.Connected);
  service.onConnectionLost = () => emitToWindows(ControlIpc.ConnectionLost);
  service.onClientConnected = (...args) => emitToWindows(ControlIpc.ClientConnected, ...args);
  service.onClientDisconnected = (...args) => emitToWindows(ControlIpc.ClientDisconnected, ...args);

  // Panel specific handlers
  // TODO
  ipcMain.on(ControlIpc.GetOptions, (): PanelOptions => ({ theme: 'system', notifications: true }));

  // Open as tray application in release
  if (!app.isPackaged) {
    createWindow("");
  }
  initTray();
});

app.on('window-all-closed', () => { });