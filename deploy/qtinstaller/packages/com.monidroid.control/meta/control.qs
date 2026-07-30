function Component() {

}

Component.prototype.createOperations = function() {
    // Unpack files by installer first
    component.createOperations();

    // Now install
    if (systemInfo.kernelType === "linux") {
        installOnLinux();
    } else if (systemInfo.kernelType === "winnt") {
        installWin32();
    }
}

function installOnLinux() {
    component.addOperation("Replace", "@TargetDir@/monidroid.desktop", "${TARGET_DIR}", "@TargetDir@");

    component.addElevatedOperation(
        "Execute",
        "mv", "@TargetDir@/monidroid.desktop", "/usr/share/applications",
        "UNDOEXECUTE",
        "rm", "-f", "/usr/share/applications/monidroid.desktop"
    );

    installer.setValue("RunProgram", "@TargetDir@/control/monidroid-control");
    installer.setValue("RunProgramArguments", "--no-sandbox");
}

function installWin32() {
    component.addOperation(
        "CreateShortcut", "@TargetDir@/control/monidroid-control.exe", "@StartMenuDir@/Monidroid Control Panel.lnk"
    );

    installer.setValue("RunProgram", "@TargetDir@/control/monidroid-control.exe");
}