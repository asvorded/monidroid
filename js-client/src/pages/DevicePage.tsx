import { Button, Icon, Text } from "@gravity-ui/uikit";
import { useEffect, useRef, useState } from "react";

import AndroidLogo from "../../static/android.svg?react";

const DevicePage = () => {
  const startTimeRef = useRef(Date.now());
  const [uptime, setUptime] = useState(0); // мс

  useEffect(() => {
    const interval = setInterval(() => {
      setUptime(Date.now() - startTimeRef.current);
    }, 1000);

    return () => clearInterval(interval);
  }, []);

  const formatTime = (ms: number) => {
    const totalSeconds = Math.floor(ms / 1000);

    const hours = Math.floor(totalSeconds / 3600);
    const minutes = Math.floor((totalSeconds % 3600) / 60);
    const seconds = totalSeconds % 60;

    return `${String(hours).padStart(2, "0")}:${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}`;
  };

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
        <Text className="block text-center pl-4 pr-4" variant="display-3">Device name</Text>
        <div className="m-4 grid grid-cols-2 gap-1">
          <Text variant="body-2">IP address</Text>
          <Text className="justify-self-end" variant="body-2">0.0.0.0</Text>
          <Text variant="body-2">Uptime</Text>
          <Text className="justify-self-end" variant="body-2">{formatTime(uptime)}</Text>
        </div>
        {/* <Button className="self-center" view="outlined-danger" size="l">Block device</Button> */}
      </div>
    </div>
  );
};

export default DevicePage;