import { app, BrowserWindow, ipcMain, Notification } from "electron";

function createWindow() {
  const window = new BrowserWindow({
    titleBarStyle: "hidden",
    titleBarOverlay: {
      height: 32,
    },
    hasShadow: true
  });
  window.loadURL('http://localhost:14769');
}

app.whenReady().then(() => {
  createWindow();
})

app.on('window-all-closed', () => {
  app.quit();
})