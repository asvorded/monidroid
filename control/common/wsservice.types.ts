export const WsPort = 14768;

export const WsProtocol = {
  GetAllClients: 'clients/all',
  ClientConnected: 'clients/new',
  ClientDisconnected: 'clients/disconnected',
  DisconnectClient: 'clients/disconnect',
  SetServerState: 'config/serverState',
  GetServerConfig: 'config/all',
  Shutdown: 'config/shutdown',
} as const;

export type ProtocolMessages = typeof WsProtocol[keyof typeof WsProtocol];

export type UUID = ReturnType<typeof crypto.randomUUID>;

export type MessageHandlers = {
  [K in ProtocolMessages]?: (data: Extract<ServerMessage, { message: K }>) => void;
}

export type ServerRequest = {
  message: ProtocolMessages,
  requestId: UUID,
  data: any,
}

export type ServerResponse = {
  requestId: UUID,
  data: any,
} | {
  requestId: UUID
  error: string,
} | ServerMessage;

export type ClientMessage = {
  message: typeof WsProtocol.Shutdown,
}

export type ServerMessage = {
  message: typeof WsProtocol.ClientConnected,
  client: Device,
} | {
  message: typeof WsProtocol.ClientDisconnected,
  id: string,
}

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

export type ShutdownOptions = { }

