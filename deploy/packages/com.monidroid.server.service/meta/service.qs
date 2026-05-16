function Component() {

}

Component.prototype.createOperations
    = systemInfo.kernelType === "linux" ? setupSystemd
    : systemInfo.kernelType === "winnt" ? setupWindowsService
    : undefined

function setupSystemd() {
    // Unpace here
    component.createOperations();

    // Now install
    component.addOperation("Replace", "@TargetDir@/monidroid.service", "${TARGET_DIR}", "@TargetDir@");

    component.addElevatedOperation(
        "Execute",
        "cp", "@TargetDir@/monidroid.service", "/etc/systemd/system",
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