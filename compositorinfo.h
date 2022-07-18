#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <map>

#include "rdkshellevents.h"
#include "rdkcompositor.h"

namespace RdkShell
{
    struct KeyListenerInfo
    {
        KeyListenerInfo() : keyCode(-1), flags(0), activate(false), propagate(true) {}
        uint32_t keyCode;
        uint32_t flags;
        bool activate;
        bool propagate;
    };

    struct CompositorInfo
    {
        CompositorInfo() : name(), compositor(nullptr), eventListeners(), mimeType() {}
        std::string name;
        std::shared_ptr<RdkCompositor> compositor;
        std::map<uint32_t, std::vector<KeyListenerInfo>> keyListenerInfo;
        std::vector<std::shared_ptr<RdkShellEventListener>> eventListeners;
        std::string mimeType;
        bool autoDestroy;
        bool isGroup = false;
    };

    typedef std::vector<CompositorInfo> CompositorList;
    typedef CompositorList::iterator CompositorListIterator;
}