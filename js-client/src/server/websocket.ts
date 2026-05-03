export const wsProtocol = {
  PORT: 14768,
  ALL: 'clients/all',
  CLIENT_CONNECTED: 'clients/new',
  CLIENT_DISCONNECTED: 'clients/disconnected',
  SERVER_STATE: 'config/serverState',
  SERVER_CONFIG: 'config/all',
  SHUTDOWN: 'confog/shutdown',
} as const;

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
