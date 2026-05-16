function Component() {
    if (systemInfo.kernelType === "linux") {
        const desc = "WARNING: to install the driver you must \
1) remove any \"evdi-dkms\" package from your system, \
2) install header sources for your current kernel.";

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
    switch (packageManager) {
    case "apt":
        component.addElevatedOperation("Execute", "apt", "install", "-y", "dkms");
        break;
    case "dnf":
        component.addElevatedOperation("Execute", "dnf", "install", "-y", "dkms");
        break;
    case "pacman":
        component.addElevatedOperation("Execute", "pacman", "-S", "--noconfirm", "--needed", "dkms");
        break;
    default:
        throw new Error("Unsupported distro");
    }

    // 3. Build with dkms
    const evdiVersion = "1.14.15";
    const evdiAbi = "1";

    component.addElevatedOperation(
        "Execute",
        "dkms", "install", "@TargetDir@/driver",
        "UNDOEXECUTE",
        "dkms", "remove", "evdi/" + evdiVersion, "--all"
    );

    component.addElevatedOperation("Copy",
        "@TargetDir@/libevdi/libevdi.so." + evdiVersion, "/usr/lib"
    );
    component.addElevatedOperation(
        "Execute",
        "ln", "-sf", "/usr/lib/libevdi.so." + evdiVersion, "/usr/lib/libevdi.so." + evdiAbi,
        "UNDOEXECUTE",
        "rm", "/usr/lib/libevdi.so." + evdiAbi
    );
    component.addElevatedOperation(
        "Execute",
        "ln", "-sf", "/usr/lib/libevdi.so." + evdiAbi, "/usr/lib/libevdi.so",
        "UNDOEXECUTE",
        "rm", "/usr/lib/libevdi.so"
    );
}

function installIddCxDriver() {

}