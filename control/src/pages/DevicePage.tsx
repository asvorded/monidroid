import { Button, Icon, Text } from "@gravity-ui/uikit";
import { useEffect, useRef, useState } from "react";

import AndroidLogo from "../../static/android.svg?react";
import { useLoaderData } from "react-router";
import { Device } from "../services/wsservice.types";

function formatTime (ms: number) {
  const totalSeconds = Math.floor(ms / 1000);

  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;

  return `${String(hours).padStart(2, "0")}:${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}`;
}

const DevicePage = () => {
  const device = useLoaderData<Device>();

  const startTimeRef = useRef(device.connectedAt);
  const [uptime, setUptime] = useState(Date.now() - startTimeRef.current); // ms

  useEffect(() => {
    const interval = setInterval(() => {
      setUptime(Date.now() - startTimeRef.current);
    }, 1000);

    return () => clearInterval(interval);
  }, []);

  return (
    <div className="flex flex-col justify-center items-center h-full">
      <div className="ripple-container">
        <div className="p-6 device-logo">
          <Icon data={AndroidLogo} size={72}></Icon>
        </div>
        <span className="ripple"></span>
        <span className="ripple" style={{ animationDelay: '2.25s' }}></span>
        <span className="ripple" style={{ animationDelay: '1.5s' }}></span>
        <span className="ripple" style={{ animationDelay: '0.75s' }}></span>
      </div>
        
      <div className="flex flex-col mt-6">
        <Text className="block text-center pl-4 pr-4" variant="display-3">{device.name}</Text>
        <div className="m-4 grid grid-cols-2 gap-1">
          <Text variant="body-2">Connection type</Text>
          <Text className="justify-self-end" variant="body-2">{ device.connectionType == "wifi" ? "Wi-Fi" : "USB" }</Text>
          {device.connectionType == "wifi" ? (
            <>
              <Text variant="body-2">IP address</Text>
              <Text className="justify-self-end" variant="body-2">{device.address}</Text>
            </>
          ) : null}
          <Text variant="body-2">Uptime</Text>
          <Text className="justify-self-end" variant="body-2">{formatTime(uptime)}</Text>
        </div>
      </div>
    </div>
  );
};

export default DevicePage;