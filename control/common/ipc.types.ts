import { WsProtocol } from "./wsservice.types";

export const ControlIpc = {
  NotificationsOptions: "app/notifications-options",
  GetClient: "app/get-client",
  Connected: "app/connected",
  ConnectionLost: "app/connection-lost",
  ...WsProtocol
} as const;

export type IpcMessages = typeof ControlIpc[keyof typeof ControlIpc];