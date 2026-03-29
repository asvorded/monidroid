#pragma once

#include <memory>
#include <stdint.h>

struct MonitorMode {
	int width;
	int height;
	int refreshRate;
};

using ColorType = uint32_t;

enum class MDStatus {
	Ok, NotAvailable, ModeChanged, BufferTooSmall, Error
};

struct MonitorContext;
struct MonitorContextDeleter {
	void operator()(MonitorContext* p) const {
		delete p;
	}
};
using Monitor = std::unique_ptr<MonitorContext, MonitorContextDeleter>;


struct AdapterContext;
using Adapter = std::shared_ptr<AdapterContext>;

void videoHealthCheck() noexcept(false);

Adapter openAdapter();

Monitor adapterConnectMonitor(const Adapter& adapter, const MonitorMode& info);

MDStatus monitorRequestFrame(const Monitor& monitor, std::unique_ptr<ColorType[]>& buffer, int& bufferPixSize) noexcept(false);

void monitorRequestMode(const Monitor& monitor, MonitorMode& infoOut, bool cached);

/// `monitor` will be reset after this operation
void monitorDisconnect(Monitor& monitor);
