function Component() {
    if (systemInfo.kernelType === "linux") {
        const desc = "Own distro-independent graphics adapter driver (kernel module). \
Warning: when using distro-provided EVDI module (\"evdi-dkms\"), monitor may display wrong colors.";

        component.setValue("ForcedInstallation", "false");
        component.setValue("Checkable", "true");
        component.setValue("Description", desc);
    }
}