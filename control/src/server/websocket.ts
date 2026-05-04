export const WS_PORT = 14768;

export const wsProtocol = {
  ALL_CLIENTS: '/clients/all',
  CLIENT_CONNECTED: '/clients/new',
  CLIENT_DISCONNECTED: '/clients/disconnected',
  DISCONNECT_CLIENT: '/clients/disconnect',
  SERVER_STATE: '/config/serverState',
  SERVER_CONFIG: '/config/all',
  SHUTDOWN: '/config/shutdown',
} as const;

export type ProtocolMessages = typeof wsProtocol[keyof typeof wsProtocol];

export type UUID = ReturnType<typeof crypto.randomUUID>;

type ConnectionType = "wifi" | "usb";

export type ServerInfo = {
  version: string,
  enabled: boolean,
  hostname: string,
  addresses: string[],
}

export type Device = {
  id: string,
  address: string,
  connectionType: ConnectionType,
  name: string,
  connectedAt: number
}

export type ServerStateOptions = {
  enable: boolean,
}

export type ServerState = {
  enabled: boolean
}

export type MessageHandlers = {
  [K in ProtocolMessages]?: (data: Extract<ServerEvent, { message: K }>) => void;
}

export type ServerRequest = {
  message: ProtocolMessages,
  requestId: UUID,
  data: any,
}

export type ClientMessage = {
  message: ProtocolMessages
}

export type ServerMessage = {
  requestId: UUID,
  data: any,
  error?: string,
} | ServerEvent;

export type ServerEvent = {
  message: typeof wsProtocol.CLIENT_CONNECTED,
  client: Device,
} | {
  message: typeof wsProtocol.CLIENT_DISCONNECTED,
  id: string,
}

export type ShutdownOptions = { }
