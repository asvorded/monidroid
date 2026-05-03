import { app, BrowserWindow, ipcMain, Menu, Notification, Tray } from "electron";
import path from "path";

let tray: Tray;

function createWindow(at: string) {
  const window = new BrowserWindow({
    titleBarStyle: "hidden",
    titleBarOverlay: {
      height: 32,
    },
    hasShadow: true,
  });
  window.loadURL('http://localhost:14769' + at);
}

function initTray() {
  tray = new Tray(path.join(app.getAppPath(), "static", "logo.png"));
  tray.setToolTip("Monidroid control panel");
  tray.setContextMenu(Menu.buildFromTemplate([
    {
      label: "Open panel",
      click: () => {
        const wins = BrowserWindow.getAllWindows()
        if (wins.length === 0) {
          createWindow("")
        } else {
          wins[0].focus()
        }
      }
    },
    {
      label: "Settings...",
      click: () => {
        const wins = BrowserWindow.getAllWindows()
        if (wins.length === 0) {
          createWindow("/settings")
        } else {
          wins[0].focus()
        }
      }
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
        // TODO
      }
    },
  ]));
}

app.whenReady().then(() => {
  createWindow("");
  initTray();
});

app.on('window-all-closed', () => {
  
});
