/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#ifndef RDKSHELL_EXTENDED_INPUT_MODULE_H
#define RDKSHELL_EXTENDED_INPUT_MODULE_H


#include <memory>
#include <thread>
#include <cinttypes>

#include "../../../rdkcompositor.h"

class RdkShellExtendedInputModule
{
public:
    RdkShellExtendedInputModule(std::shared_ptr<RdkShell::RdkCompositor> &client,
                   struct wl_client *wlClient,
                   struct wl_resource *wlRdkShellExtendedInput);
    ~RdkShellExtendedInputModule();


private:
    void onWesterosInputEvent(const RdkShell::InputEvent &event);
    static int onWaylandInputEventNotification(int fd, uint32_t mask, void *data);

private:
    std::weak_ptr<RdkShell::RdkCompositor> mClient;
    int mListenerTag;

    int mNotifierPipeFd;
    int mNotifierReadPipeFd;
    struct wl_event_source *mNotifierSource;

    std::thread::id mWaylandThreadId;
};



#endif // RDKSHELL_EXTENDED_INPUT_MODULE_H
