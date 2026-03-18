import { app, BrowserWindow, ipcMain, Notification } from "electron";

function createWindow() {
  const window = new BrowserWindow({});
  window.loadURL('http://localhost:14764');
}

app.whenReady().then(() => {
  createWindow();
})

app.on('window-all-closed', () => {
  app.quit();
})