export interface ControlPanel {
  shutdown: () => void
}

declare global {
  interface Window {
    controlPanel: ControlPanel
  }
}
