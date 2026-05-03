import {CircleQuestion, LogoAndroid, LogoLinux, Power} from '@gravity-ui/icons';
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

export const ShutdownButton = ({className} : ButtonButtonProps) => (
    <Button width="max" view="outlined-danger" pin="circle-circle" size="l"
      className={className}
      style={{
        justifyContent: 'flex-start',
      }}
    >
      <Icon data={Power} />
      <Text>Shutdown</Text>
    </Button>
);
