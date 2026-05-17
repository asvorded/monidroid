function Component() {

}

Component.prototype.createOperations = function() {
    // Unpack files by installer first
    component.createOperations();

    // Now install
    if (systemInfo.kernelType === "linux") {
        installDesktopFiles();
    } else if (systemInfo.kernelType === "winnt") {
        installShortcuts();
    }
}

function installDesktopFiles() {
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

function installShortcuts() {

}