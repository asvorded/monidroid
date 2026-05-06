import '@gravity-ui/uikit/styles/fonts.css';
import '@gravity-ui/uikit/styles/styles.css';

import './theme.css';

import { createRoot } from 'react-dom/client';
import App from './App';
import { createHashRouter, redirect } from 'react-router';
import HomePage from './pages/HomePage';
import { RouterProvider } from 'react-router/dom';
import SettingsPage from './pages/SettingsPage';
import DevicePage from './pages/DevicePage';
import { CustomThemeProvider } from './hooks/useAppTheme';
import service from './services/control';
import { StrictMode } from 'react';
import { Device, ServerInfo } from '../common/wsservice.types';

const router = createHashRouter([
  {
    path: '/',
    Component: App,
    children: [
      {
        index: true,
        Component: HomePage,
        // TODO: when server disabled
        loader: async (): Promise<ServerInfo> => {
          try {
            return await service.getServerInfo();
          } catch {
            return {
              version: "",
              enabled: false,
              hostname: "",
              addresses: [],
            };
          }
        },
      },
      {
        path: 'settings',
        Component: SettingsPage,
        loader: async () => await service.getOptions(),
      },
      {
        path: 'devices/:id',
        Component: DevicePage,
        loader: async ({params}): Promise<Device> => {
          try {
            const dev = await service.getClient(params.id!!);
            if (dev === undefined) {
              throw redirect("/");
            } else {
              return dev;
            }
          } catch {
            throw redirect("/");
          }
        },
      },
    ],
  },
]);

const root = document.getElementById('root')!;
createRoot(root).render(
  <StrictMode>
    <CustomThemeProvider>
      <RouterProvider router={router} />
    </CustomThemeProvider>
  </StrictMode>
);
