import '@gravity-ui/uikit/styles/fonts.css';
import '@gravity-ui/uikit/styles/styles.css';

import './theme.css';

import { createRoot } from 'react-dom/client';
import App from './App';
import { createHashRouter } from 'react-router';
import HomePage from './pages/HomePage';
import { RouterProvider } from 'react-router/dom';
import SettingsPage from './pages/SettingsPage';
import DevicePage from './pages/DevicePage';
import { CustomThemeProvider } from './hooks/useAppTheme';
import service from './services/control';
import { StrictMode } from 'react';

const router = createHashRouter([
  {
    path: '/',
    Component: App,
    children: [
      {
        index: true,
        Component: HomePage,
        loader: async () => await service.getServerInfo(),
      },
      {
        path: 'settings',
        Component: SettingsPage,
        loader: async () => await service.getOptions(),
      },
      {
        path: 'devices/:id',
        Component: DevicePage,
        loader: async ({params}) => await service.getClient(params.id!!),
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
