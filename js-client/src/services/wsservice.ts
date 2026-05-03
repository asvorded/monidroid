import { io } from "socket.io-client";

import {
  AllClientsResponse, ClientConnectedEvent,
  ClientDisconnectedEvent, Device, ServerInfo, wsProtocol
} from "../server/websocket";

const socket = io(`http://localhost:${wsProtocol.PORT}`);

const URL = `http://localhost:${wsProtocol.PORT}`;

const devices: Map<string, Device> = new Map<string, Device>();

async function getServerInfo(): Promise<ServerInfo> {
  let res = await fetch(URL + wsProtocol.SERVER_CONFIG, {

  });

  return {};
}

function getAllClients(): Promise<Device[]> {
  devices.clear();
  
  return new Promise((resolve, reject) => {
    socket.emit(wsProtocol.ALL, (response: AllClientsResponse) => {
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

const service = {
  getAllClients,
  registerOnClientConnected,
  registerOnClientDisconnected,
  unregisterAll,
};

export default service;
