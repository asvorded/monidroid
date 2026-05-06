import { app, BrowserWindow, dialog, ipcMain, Menu, nativeTheme, Notification, Tray } from "electron/main";
import path from "path";
import fs from "fs";
import service from "./wsservice";
import { ControlIpc, IpcMessages, PanelOptions } from "../common/control.types";
import { ServerStateOptions } from "../common/wsservice.types";

let tray: Tray;

const configPath = path.resolve(app.getPath('userData'), "monidroid.json");
const preloadPath = path.resolve(__dirname, "preload.js");
const staticPath = path.resolve(__dirname, "static");

const defaultConfig: PanelOptions = {
  theme: 'system',
  notifications: true,
}

if (!fs.existsSync(configPath)) {
  fs.writeFileSync(configPath, JSON.stringify(defaultConfig, null, 2));
}

let config: PanelOptions = JSON.parse(fs.readFileSync(configPath).toString())

function createWindow(at: string) {
  const window = new BrowserWindow({
    titleBarStyle: "hidden",
    titleBarOverlay: {
      height: 32,
    },
    hasShadow: true,
    webPreferences: {
      preload: preloadPath,
    }
  });
  // Using hash router
  if (!app.isPackaged) {
    window.loadURL('http://localhost:14769#' + at);
  } else {
    window.loadFile("index.html", { hash: at });
  }
}

function openWindow(at: string) {
  const wins = BrowserWindow.getAllWindows()
  if (wins.length === 0) {
    createWindow(at);
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
  tray = new Tray(path.resolve(staticPath, "logo.png"));
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
      label: "Shutdown",
      click: async () => {
        const { response } = await dialog.showMessageBox({
          title: "Monidroid",
          message: "Are you sure to shutdown the server?",
          type: 'question',
          buttons: [ "Yes", "No" ],
          defaultId: 1,
        });
        if (response == 0) {
          service.shutdown();
          app.quit();
        }
      }
    },
    { type: "separator" },
    {
      label: "Quit",
      click: () => {
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
  ipcMain.handle(ControlIpc.DisconnectClient, (_, id: string) => service.forceDisconnect(id));

  ipcMain.handle(ControlIpc.SetServerState, (_, options: ServerStateOptions) => service.setServerState(options));
  ipcMain.on(ControlIpc.Shutdown, () => {
    service.shutdown();
    app.quit();
  });

  // Event hadlers
  service.onConnected = () => emitToWindows(ControlIpc.Connected);
  service.onConnectionLost = () => emitToWindows(ControlIpc.ConnectionLost);
  service.onClientConnected = (client, presents) => {
    if (config.notifications && !presents) {
      new Notification({
        title: "Monidroid",
        body: `Client ${client.name} connected`,
        icon: path.resolve(staticPath,
          nativeTheme.shouldUseDarkColorsForSystemIntegratedUI ? 'connected-dark.png' : 'connected-light.png')
      }).show();
    }
    emitToWindows(ControlIpc.ClientConnected, client, presents);
  }
  service.onClientDisconnected = (client, removed) => {
    if (config.notifications && !removed) {
      new Notification({
        title: "Monidroid",
        body: `Client ${client.name} disconnected`,
        icon: path.resolve(staticPath,
          nativeTheme.shouldUseDarkColorsForSystemIntegratedUI ? 'disconnected-dark.png' : 'disconnected-light.png')
      }).show();
    }
    emitToWindows(ControlIpc.ClientDisconnected, client.id, removed);
  }

  // Panel specific handlers
  ipcMain.handle(ControlIpc.GetOptions, (): PanelOptions => config);
  ipcMain.handle(ControlIpc.SetOptions, (_, options: Partial<PanelOptions>) => {
    if (options.theme !== undefined) {
      nativeTheme.themeSource = options.theme;
      config.theme = options.theme;
    }
    if (options.notifications !== undefined) {
      config.notifications = options.notifications;
    }
    fs.writeFileSync(configPath, JSON.stringify(config, null, 2));
  });

  // Initialization from options
  nativeTheme.themeSource = config.theme;

  // Open as tray application in release
  if (!app.isPackaged) {
    createWindow("");
  }
  initTray();
});

app.on('window-all-closed', () => { });

app.on('will-quit', () => {
  fs.writeFileSync(configPath, JSON.stringify(config, null, 2));
})