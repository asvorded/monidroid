import { io } from "socket.io-client";

import {
  AllClientsResponse, ClientConnectedEvent,
  ClientDisconnectedEvent, Device, ServerInfo, wsProtocol
} from "../server/websocket";

const URL = `http://localhost:${wsProtocol.PORT}/`;

const socket = io(URL);

const devices: Map<string, Device> = new Map<string, Device>();

const testDevices: Device[] = Array(5).fill(0).map((v, i) => ({
    id: 'dev_' + i,
    connectionType: i % 2 == 0 ? 'wifi' : 'usb',
    address: '192.168.1.' + i,
    name: 'azaa ' + i,
    connectedAt: Date.now()
  })
);

async function getServerInfo(): Promise<ServerInfo> {
  return {
    version: "0.1.0",
    enabled: true,
    hostname: "mypc",
    addresses: [
      "1.1.1.1",
      "2.2.2.2"
    ]
  }
  return await socket.emitWithAck(wsProtocol.SERVER_CONFIG) as ServerInfo;
}

function getAllClients(): Promise<Device[]> {
  devices.clear();

  return Promise.resolve(testDevices);
  
  return new Promise((resolve, reject) => {
    socket.emit(wsProtocol.ALL_CLIENTS, (response: AllClientsResponse) => {
      if (!response.error) {
        for (const c of response.clients) {
          devices.set(c.id, c);
        }
        resolve(Array.from(devices.values()));
      } else {
        reject({ message: response.error });
      }
    });
  });
}

function getClient(id: string): Device | undefined {
  return testDevices.find(e => e.id == id);
  return devices.get(id);
}

function registerOnClientConnected(
  callback: (client: Device, alreadyPresent: boolean) => void
): boolean {
  socket.removeAllListeners(wsProtocol.CLIENT_CONNECTED);

  socket.on(wsProtocol.CLIENT_CONNECTED, (e: ClientConnectedEvent) => {
    if (!devices.has(e.client.id)) {
      devices.set(e.client.id, e.client);
      callback(e.client, false);
    } else {
      callback(e.client, true);
    }
  });

  return true;
}

function registerOnClientDisconnected(
  callback: (id: string, alreadyRemoved: boolean) => void
): boolean {
  socket.removeAllListeners(wsProtocol.CLIENT_DISCONNECTED);
  
  socket.on(wsProtocol.CLIENT_DISCONNECTED, (e: ClientDisconnectedEvent) => {
    if (devices.has(e.id)) {
      devices.delete(e.id);
      callback(e.id, false);
    } else {
      callback(e.id, true);
    }
  });

  return true;
}

function unregisterAll() {
  for (const ev in wsProtocol) {
    socket.removeAllListeners(ev);
  }
}

function setServerState(enable: boolean) {
  socket.emit(wsProtocol.SERVER_STATE, { enable: enable });
}

function shutdown() {
  socket.emit(wsProtocol.SHUTDOWN);
  // TODO: close window and stop process
}

const service = {
  getServerInfo,
  getAllClients,
  getClient,
  registerOnClientConnected,
  registerOnClientDisconnected,
  unregisterAll,
};

export default service;
