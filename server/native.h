#pragma once

#include <memory>
#include <string>

#include "monidroid/edid.h"

using namespace Monidroid;

using ColorType = u32;

enum class MDStatus : int {
	Ok = 0,
	ModeChanged,
	MonitorOff,
	Error = -1,
	BufferTooSmall = -2,
	NotAvailable = -3, 
};

inline constexpr bool MDSuccess(MDStatus status) {
	return (int)status >= 0;
}

struct MonitorContext;
struct MonitorContextDeleter {
	void operator()(MonitorContext* p) const;
};
using Monitor = std::unique_ptr<MonitorContext, MonitorContextDeleter>;

struct AdapterContext;
using Adapter = std::shared_ptr<AdapterContext>;

struct FrameMapInfo {
	ColorType* data;
	unsigned int width;
	unsigned int height;
	unsigned int stride;
};

void videoHealthCheck() noexcept(false);

Adapter openAdapter();

Monitor adapterConnectMonitor(const Adapter& adapter, const std::string& modelName, const MonitorMode& info);

MDStatus monitorRequestFrame(const Monitor& monitor);

MonitorMode monitorRequestMode(const Monitor& monitor, bool cached);

void monitorMapCurrent(const Monitor& self, FrameMapInfo& mapInfo);
void monitorUnmap(const Monitor& self);

/// `monitor` will be reset after this operation
void monitorDisconnect(Monitor& monitor);