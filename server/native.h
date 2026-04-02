#pragma once

#include <memory>
#include <string>

#include "monidroid/edid.h"

using namespace Monidroid;

using ColorType = u32;

enum class MDStatus {
	Ok, NotAvailable, ModeChanged, BufferTooSmall, Error
};

struct MonitorContext;
struct MonitorContextDeleter {
	void operator()(MonitorContext* p) const;
};
using Monitor = std::unique_ptr<MonitorContext, MonitorContextDeleter>;

struct AdapterContext;
using Adapter = std::shared_ptr<AdapterContext>;

void videoHealthCheck() noexcept(false);

Adapter openAdapter();

Monitor adapterConnectMonitor(const Adapter& adapter, const std::string& modelName, const MonitorMode& info);

MDStatus monitorRequestFrame(const Monitor& monitor, std::unique_ptr<ColorType[]>& buffer, unsigned int& bufferPixSize) noexcept(false);

void monitorRequestMode(const Monitor& monitor, MonitorMode& infoOut, bool cached);

/// `monitor` will be reset after this operation
void monitorDisconnect(Monitor& monitor);
