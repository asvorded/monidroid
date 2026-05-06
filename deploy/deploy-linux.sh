#!/usr/bin/bash

if ! [ -d "deploy" ]; then
    echo "Must be run from the project root";
    exit -1;
fi

# Config
MD_VERSION=0.1.0;

ROOT=$PWD;

DEPLOY_SRC_DIR=${ROOT}/deploy;

DEPLOY_OUT_DIR=${ROOT}/deploy;

CMAKE_SERVER_INSTALL_PREFIX=${DEPLOY_OUT_DIR}/packages;
SERVER_INSTALL_PREFIX=${DEPLOY_OUT_DIR}/packages/com.monidroid.server/data;
CONTROL_INSTALL_PREFIX=${DEPLOY_OUT_DIR}/packages/com.monidroid.control/data;

DEPLOY_NAME=monidroid-linux-${MD_VERSION}-setup;

# Build and copy server
cmake -DCMAKE_BUILD_TYPE=Release -DMD_DEPLOY=TRUE -B build --install-prefix ${CMAKE_SERVER_INSTALL_PREFIX} &&
cmake --build build --target monidroid-server &&
cmake --install build &&

# Copy .service file
cp ${DEPLOY_SRC_DIR}/linux/monidroid.service ${SERVER_INSTALL_PREFIX} &&

# Build and copy control panel
cd control &&
npm install &&
npm run app-build &&
mkdir ${CONTROL_INSTALL_PREFIX}; # If directory created, ignore
cp -r out/monidroid-control-linux-x64 ${CONTROL_INSTALL_PREFIX} &&
cd .. &&

# Make installer executable
binarycreator -c ${DEPLOY_SRC_DIR}/config/config.xml -p ${DEPLOY_SRC_DIR}/packages ${DEPLOY_OUT_DIR}/${DEPLOY_NAME};