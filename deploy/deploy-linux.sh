#!/usr/bin/bash

set -e;

if [ ! -d "deploy" ]; then
    echo "Must be run from the project root";
    exit -1;
fi

# Config
MD_VERSION=0.1.0;
# MD_DEPLOY_DATE=$(date +"%Y-%m-%d");
MD_DEPLOY_DATE="2026-04-30";

DEPLOY_DIR=${PWD}/deploy;

SERVER_INSTALL_PREFIX=${DEPLOY_DIR}/packages/com.monidroid.server/data;
SERVER_AUTOSTART_INSTALL_PREFIX=${DEPLOY_DIR}/packages/com.monidroid.server.service/data;
DRIVER_INSTALL_PREFIX=${DEPLOY_DIR}/packages/com.monidroid.driver/data;
CONTROL_INSTALL_PREFIX=${DEPLOY_DIR}/packages/com.monidroid.control/data;
CONTROL_AUTOSTART_INSTALL_PREFIX=${DEPLOY_DIR}/packages/com.monidroid.control.autostart/data;

DEPLOY_NAME=monidroid-linux-${MD_VERSION}-setup;

function message()
{
    echo;
    echo $1;
    echo;
}

# Clean files
message "Cleaning files before deploy...";

rm -r ${DEPLOY_DIR}/packages/*/data/*;

# Set versions
message "Setting the version in .xml files...";

find ${DEPLOY_DIR} -name package.xml -o -name config.xml | while read -r package; do
    sed -i -E "s|(<Version>)[^<]*(</Version>)|\1${MD_VERSION}\2|g" ${package};
    sed -i -E "s|(<ReleaseDate>)[^<]*(</ReleaseDate>)|\1${MD_DEPLOY_DATE}\2|g" ${package};
done

# Build and copy server
message "Building the server...";

cmake -DCMAKE_BUILD_TYPE=Release -DMD_DEPLOY=TRUE -B ${DEPLOY_DIR}/build --install-prefix ${SERVER_INSTALL_PREFIX};
cmake --build ${DEPLOY_DIR}/build --target monidroid-server;
cmake --install ${DEPLOY_DIR}/build;
cp $(which adb) ${SERVER_INSTALL_PREFIX};

# Copy .service file
mkdir -p ${SERVER_AUTOSTART_INSTALL_PREFIX};
cp ${DEPLOY_DIR}/linux/monidroid.service ${SERVER_AUTOSTART_INSTALL_PREFIX};

# Build libevdi
message "Building libevdi...";

cd evdi/library;
make;
make LIBDIR=${DRIVER_INSTALL_PREFIX}/libevdi install;
cd ../..;

# Put driver source files
message "Copying driver source files...";

mkdir -p ${DRIVER_INSTALL_PREFIX}/driver;
cp -r evdi/module/* ${DRIVER_INSTALL_PREFIX}/driver;

# Build and copy control panel
message "Building the control panel...";

cd control;
npm install;
npm run app-build;
mkdir -p ${CONTROL_INSTALL_PREFIX}/control;
cp -r out/monidroid-control-linux-x64/* ${CONTROL_INSTALL_PREFIX}/control;
cd ..;

cp ${DEPLOY_DIR}/linux/monidroid.desktop ${CONTROL_INSTALL_PREFIX};
cp control/static/logo.png ${CONTROL_INSTALL_PREFIX};

# Make installer executable
message "Making an installer...";

binarycreator -c ${DEPLOY_DIR}/config/config.xml -p ${DEPLOY_DIR}/packages ${DEPLOY_DIR}/${DEPLOY_NAME};