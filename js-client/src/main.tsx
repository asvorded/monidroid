import '@gravity-ui/uikit/styles/fonts.css';
import '@gravity-ui/uikit/styles/styles.css';

import './theme.css';

import { createRoot } from 'react-dom/client';
import App from './App';
import { createBrowserRouter } from 'react-router';
import HomePage from './pages/HomePage';
import { RouterProvider } from 'react-router/dom';
import SettingsPage from './pages/SettingsPage';
import DevicePage from './pages/DevicePage';
import { CustomThemeProvider } from './hooks/useAppTheme';
import service from './services/wsservice';

const router = createBrowserRouter([
  {
    path: '/',
    Component: App,
    children: [
      {
        index: true,
        Component: HomePage,
        loader: async () => await service.getServerInfo(),
      },
      { path: 'settings', Component: SettingsPage },
      {
        path: 'devices/:id',
        Component: DevicePage,
        loader: ({params}) => service.getClient(params.id!!),
      },
    ],
  },
]);

const root = document.getElementById('root')!;
createRoot(root).render(
  <CustomThemeProvider>
    <RouterProvider router={router} />
  </CustomThemeProvider>
);
