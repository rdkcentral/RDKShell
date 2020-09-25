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

#ifndef RDKSHELL_CHANNEL_H
#define RDKSHELL_CHANNEL_H

#include "communicationhandler.h"
#include "rdkshelldata.h"
#include "rdkshelleventlistener.h"
#include <map>
#include <vector>

namespace RdkShell
{
    class RdkShellChannel
    {
        public:
            ~RdkShellChannel();
            static RdkShellChannel* instance();
            void initialize();
            bool callMethod(std::string& name, std::map<std::string, RdkShellData>& params);
            bool callMethodWithResult(std::string& name, std::map<std::string, RdkShellData>& params, std::map<std::string, RdkShellData>& result);
            void processMessages(int wait = 0);
            void registerForEvent(std::string& name, RdkShellEventListener* listener);
            void unregisterEvent(std::string& name, RdkShellEventListener* listener);

        private:
            RdkShellChannel();
            void processMessages(int wait, std::map<std::string, RdkShellData>& responseData);
            static RdkShellChannel* mInstance;
            CommunicationHandler* mCommunicationHandler;
            bool mIsConnectedToServer;
            int mResponseTimeout;
            std::map<std::string, std::vector<RdkShellEventListener*>> mEventHandlers;
            int mMessageId;
    };
}

#endif //RDKSHELL_CHANNEL_H
