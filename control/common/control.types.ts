import { Device, ServerInfo, ServerState, ServerStateOptions, WsProtocol } from "./wsservice.types"

export const ControlIpc = {
  NotificationsOptions: "app/notifications-options",
  GetOptions: "app/get-options",
  SetOptions: "app/set-options",

  GetClient: "app/get-client",

  Connected: "app/connected",
  ConnectionLost: "app/connection-lost",

  ...WsProtocol
} as const;

export type IpcMessages = typeof ControlIpc[keyof typeof ControlIpc];

export type AppTheme = 'dark' | 'light' | 'system';

export type PanelOptions = {
  theme: AppTheme,
  notifications: boolean,
};

export interface ControlPanel {
  getServerInfo(): Promise<ServerInfo>,

  getAllClients(): Promise<Device[]>,

  getClient(id: string): Promise<Device | undefined>,

  forceDisconnect(id: string): Promise<void>,

  setServerState(options: ServerStateOptions): Promise<ServerState>,

  on(event: typeof ControlIpc.ClientConnected, cb: (client: Device, alreadyPresents: boolean) => void): void,
  on(event: typeof ControlIpc.ClientDisconnected, cb: (id: string, alreadyRemoved: boolean) => void): void,
  on(event: typeof ControlIpc.Connected, cb: () => void): void,
  on(event: typeof ControlIpc.ConnectionLost, cb: () => void): void,

  off(event: IpcMessages): void

  getOptions(): Promise<PanelOptions>,

  setOptions(options: Partial<PanelOptions>): Promise<void>,

  shutdown: () => void
}

declare global {
  interface Window {
    controlPanel: ControlPanel
  }
}
