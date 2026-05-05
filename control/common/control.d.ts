import { ControlIpc, IpcMessages } from "./ipc.types"
import { Device, ServerInfo, ServerState, ServerStateOptions } from "./wsservice.types"

export interface ControlPanel {
  getServerInfo(): Promise<ServerInfo>,

  getAllClients(): Promise<Device[]>,

  getClient(id: string): Promise<Device | undefined>,

  setServerState(options: ServerStateOptions): Promise<ServerState>,

  on(event: typeof ControlIpc.ClientConnected, cb: (client: Device, alreadyPresents: boolean) => void): void,
  on(event: typeof ControlIpc.ClientDisconnected, cb: (id: string, alreadyRemoved: boolean) => void): void,
  on(event: typeof ControlIpc.Connected, cb: () => void): void,
  on(event: typeof ControlIpc.ConnectionLost, cb: () => void): void,

  off(event: IpcMessages): void

  shutdown: () => void
}

declare global {
  interface Window {
    controlPanel: ControlPanel
  }
}
