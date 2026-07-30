function Component() {

}

Component.prototype.createOperations = function() {
    // Unpack files by installer first
    component.createOperations();

    // Now install
    if (systemInfo.kernelType === "linux") {
        component.addElevatedOperation(
            "Execute",
            "ln", "-sf", "/usr/share/applications/monidroid.desktop", "/etc/xdg/autostart/monidroid.desktop",
            "UNDOEXECUTE",
            "rm", "-f", "/etc/xdg/autostart/monidroid.desktop"
        );
    } else if (systemInfo.kernelType === "winnt") {
        component.addElevatedOperation(
            "Execute", [
            "reg", "add", "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", "/v", "Monidroid Control Panel",
                "/t", "REG_SZ", "/d", (installer.value("TargetDir") + "/control/monidroid-control.exe").replace(/\//g, "\\"), "/f",
            "UNDOEXECUTE",
            "reg", "delete", "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", "/v", "Monidroid Control Panel", "/f" ]
        )
    }
}
