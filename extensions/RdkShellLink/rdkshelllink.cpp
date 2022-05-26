#include "rdkshelllink.h"
#include <algorithm>

static std::map<wl_display*, std::weak_ptr<DisplayInfo>> storage;
static std::mutex mutex;

std::shared_ptr<DisplayInfo> link(wl_display* display)
{
	std::lock_guard<std::mutex> guard(mutex);

	// clean up unused entries
	{
		auto it = storage.begin();
		while (it != storage.end())
		{
			if (it->first != display && it->second.expired())
			{
				it = storage.erase(it);
			}
			else
			{
				it++;
			}
		}
	}

	// returns empty weak_ptr if element does not exist, calling lock() on that returns empty shared_ptr
	auto &entry = storage[display];
	auto shared = entry.lock();

	if (!shared) // pointer holds no data
	{
		shared = std::make_shared<DisplayInfo>();
		entry = shared;
	}

	return shared;
}

