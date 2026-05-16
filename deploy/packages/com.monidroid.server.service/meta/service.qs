function Component() {
    if (systemInfo.kernelType === "winnt") {
        component.setValue("Dependencies", "com.monidroid.driver");
    }
}

Component.prototype.createOperations = function() {
    // Unpack files first by installer
    component.createOperations();

    // Now install

    if (systemInfo.kernelType === "linux") {
        setupSystemd();
    } else if (systemInfo.kernelType === "winnt") {
        setupWindowsService();
    }
}

function setupSystemd() {
    component.addOperation("Replace", "@TargetDir@/monidroid.service", "${TARGET_DIR}", "@TargetDir@");

    component.addElevatedOperation(
        "Execute",
        "mv", "@TargetDir@/monidroid.service", "/etc/systemd/system",
        "UNDOEXECUTE",
        "rm", "-f", "/etc/systemd/system/monidroid.service"
    );
    component.addElevatedOperation(
        "Execute",
        "systemctl", "--system", "enable", "monidroid",
        "UNDOEXECUTE",
        "systemctl", "--system", "disable", "monidroid"
    );
    component.addElevatedOperation(
        "Execute",
        "systemctl", "--system", "start", "monidroid",
        "UNDOEXECUTE",
        "systemctl", "--system", "stop", "monidroid"
    );
}

function setupWindowsService() {

}