import {CircleQuestion, LogoAndroid, LogoLinux} from '@gravity-ui/icons';
import { Button, ButtonButtonProps, Icon, Text } from "@gravity-ui/uikit";
import { Device } from './server/websocket';
import { NavLink, NavLinkProps } from 'react-router';

export const MenuButton = ({ icon, text, className, to }: {
  icon: any, text: string
} & ButtonButtonProps & NavLinkProps) => (
  <NavLink to={to}>
    {({ isActive }) => (
      <Button className={className} width="max" view="flat" pin="circle-circle" size="l"
        selected={isActive}
        style={{
          justifyContent: 'flex-start',
        }}
      >
        <Icon data={icon} />
        <Text>{text}</Text>
      </Button>
    )}
  </NavLink>
);

export const ClientButton = ({ device }: { device: Device }) => (
  <NavLink to={`/devices/${device.id}`}>
    {({ isActive }) => (
      <Button width="max" view="flat" pin="circle-circle" size="l"
        selected={isActive}
        style={{
          justifyContent: 'flex-start',
        }}
      >
        <Icon data={LogoAndroid} />
        <Text>{device.name}</Text>
      </Button>
    )}
  </NavLink>
);
