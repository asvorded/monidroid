export const wsProtocol = {
  PORT: 14768,
  ALL_CLIENTS: 'clients/all',
  CLIENT_CONNECTED: 'clients/new',
  CLIENT_DISCONNECTED: 'clients/disconnected',
  DISCONNECT_CLIENT: 'clients/disconnect',
  SERVER_STATE: 'config/serverState',
  SERVER_CONFIG: 'config/all',
  SHUTDOWN: 'config/shutdown',
} as const;

export type ServerInfo = {
  version: string,
  enabled: boolean,
  hostname: string,
  addresses: string[],
};

type ConnectionType = "wifi" | "usb";

export type Device = {
  id: string,
  address: string,
  connectionType: ConnectionType,
  name: string,
  connectedAt: number
};

export type AllClientsResponse = {
  error?: string,
  clients: Device[],
};

export type ClientConnectedEvent = {
  client: Device,
};

export type ClientDisconnectedEvent = {
  id: string,
};
