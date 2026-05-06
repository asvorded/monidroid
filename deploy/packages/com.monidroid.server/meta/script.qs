function Component() {

}

Component.prototype.createOperations = function() {
    component.createOperations();

    let os = systemInfo.kernelType;
    
    if (os === "linux") {
        // Install .service file
        // TODO: hardcoded systemd path
        component.addOperation("Copy", "monidroid.service", "/etc/systemd/system");
    } 
}