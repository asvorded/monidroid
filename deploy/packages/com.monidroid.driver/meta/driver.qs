function Component() {
    if (systemInfo.kernelType === "winnt") {
        component.setValue("ForcedInstallation", "true");
    } else if (systemInfo.kernelType === "linux") {
        const desc = "WARNING: this driver is incompatible with \"evdi-dkms\" package.";

        component.setValue("Description", desc);
    }
}

Component.prototype.createOperations = function() {
    // Unpack files by installer first
    component.createOperations();

    // Now install
    if (systemInfo.kernelType === "linux") {
        installCustomEvdi();
    } else if (systemInfo.kernelType === "winnt") {
        installIddCxDriver();
    }
}

function installCustomEvdi() {
    // 1. Detect package manager and kernel release
    let packageManager;
    for (const pm of ["dnf", "apt", "pacman"]) {
        if (installer.execute("which", [pm])[1] == 0) {
            packageManager = pm;
            break;
        }
    }
    
    // 2. Get build dependencies
    let libPath;
    console.log("Installing dkms...");
    switch (packageManager) {
    case "apt":
        component.addElevatedOperation("Execute", "apt", "install", "-y", "dkms");
        libPath = "/usr/lib";
        break;
    case "dnf":
        component.addElevatedOperation("Execute", "dnf", "install", "-y", "dkms");
        libPath = "/usr/lib64";
        break;
    case "pacman":
        component.addElevatedOperation("Execute", "pacman", "-S", "--noconfirm", "--needed", "dkms");
        libPath = "/usr/lib";
        break;
    default:
        throw new Error("Unsupported distro");
    }

    // 3. Build with dkms
    const evdiVersion = "1.14.15";
    const evdiAbi = "1";

    console.log("Building driver with dkms...");
    component.addElevatedOperation(
        "Execute",
        "dkms", "install", "@TargetDir@/driver",
        "UNDOEXECUTE",
        "dkms", "remove", "evdi/" + evdiVersion, "--all"
    );

    // Server will be restarting until libevdi becomes available
    component.addElevatedOperation("Copy",
        `@TargetDir@/libevdi/libevdi.so.${evdiVersion}`, libPath
    );
    component.addElevatedOperation(
        "Execute",
        "ln", "-sf", `${libPath}/libevdi.so.${evdiVersion}`, `${libPath}/libevdi.so.${evdiAbi}`,
        "UNDOEXECUTE",
        "rm", `${libPath}/libevdi.so.${evdiAbi}`
    );
    component.addElevatedOperation(
        "Execute",
        "ln", "-sf", `${libPath}/libevdi.so.${evdiAbi}`, `${libPath}/libevdi.so`,
        "UNDOEXECUTE",
        "rm", `${libPath}/libevdi.so`
    );
}

function installIddCxDriver() {

}