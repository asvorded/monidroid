#!/usr/bin/bash

function message()
{
    echo;
    echo $1;
    echo;
}

if ! [ -d "deploy" ]; then
    echo "Must be run from the project root";
    exit -1;
fi

# Config
MD_VERSION=0.1.0;

ROOT=$PWD;

DEPLOY_SRC_DIR=${ROOT}/deploy;

DEPLOY_OUT_DIR=${ROOT}/deploy;

SERVER_INSTALL_PREFIX=${DEPLOY_OUT_DIR}/packages/com.monidroid.server/data;
CONTROL_INSTALL_PREFIX=${DEPLOY_OUT_DIR}/packages/com.monidroid.control/data;

DEPLOY_NAME=monidroid-linux-${MD_VERSION}-setup;

# Clean files
message "Cleaning files before deploy...";

rm -rv ${DEPLOY_OUT_DIR}/packages/*/data/*;

# Build and copy server
message "Building the server...";

cmake -DCMAKE_BUILD_TYPE=Release -DMD_DEPLOY=TRUE -B ${DEPLOY_SRC_DIR}/build --install-prefix ${SERVER_INSTALL_PREFIX} &&
cmake --build ${DEPLOY_SRC_DIR}/build --target monidroid-server &&
cmake --install ${DEPLOY_SRC_DIR}/build &&

# Copy .service file
cp ${DEPLOY_SRC_DIR}/linux/monidroid.service ${SERVER_INSTALL_PREFIX} &&

# Build and copy control panel
message "Building the control panel...";

cd control &&
npm install &&
npm run app-build &&
mkdir ${CONTROL_INSTALL_PREFIX}; # If directory created, ignore
cp -r out/monidroid-control-linux-x64/* ${CONTROL_INSTALL_PREFIX} &&
cd .. &&

# Make installer executable
message "Making an installer...";

binarycreator -c ${DEPLOY_SRC_DIR}/config/config.xml -p ${DEPLOY_SRC_DIR}/packages ${DEPLOY_OUT_DIR}/${DEPLOY_NAME};