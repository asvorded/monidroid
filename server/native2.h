#pragma once

#include <string>

#include "monidroid/edid.h"

using namespace Monidroid;

enum class FrameStatus;

struct FrameMapInfo;

struct MonitorContext2 {
	FrameStatus requestFrame();
	MonitorMode requestMode(bool cached);

	void mapCurrent(FrameMapInfo &mapInfo);
	void unmap();

	void disconnect();
};

struct AdapterContext2 {
	static void healthCheck();
	static AdapterContext2* open();

	MonitorContext2* connectMonitor(const std::string& modelName, const MonitorMode& info);
};
