#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <wayland-server.h>

using SurfaceId = uint32_t;

struct SurfaceInfo
{
    bool visible;
    bool overlay;
};

struct DisplayInfo
{
    std::mutex mutex;
    bool visible_by_default = true;
    std::map<SurfaceId, SurfaceInfo> surface;
};

// NOTE: Depending how Westeros is configured we will observe different initialization sequences
// This is here to ensure that the first user creates the entry and the last user cleans it up
// NOTE: Users should keep the received pointers alive for the duration of their own lifetime
std::shared_ptr<DisplayInfo> link(wl_display *display);
