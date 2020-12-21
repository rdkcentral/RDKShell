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

#ifndef RDKSHELL_EASTER_EGG_H
#define RDKSHELL_EASTER_EGG_H

#include <string>
#include "rdkshellevents.h"

namespace RdkShell
{
    struct RdkShellEasterEggKeyDetails
    {
        uint32_t keyCode;
        uint32_t keyModifiers;
        uint32_t keyHoldTime;
    };

    class EasterEgg
    {
        public:
             EasterEgg (std::vector<RdkShellEasterEggKeyDetails>& details, std::string name, uint32_t timeout, std::string actionJson);
             void checkKey(uint32_t keyCode, uint32_t flags, double keyPressTime);
        private:
            void toggleForce720();
            std::vector<RdkShellEasterEggKeyDetails> mKeyDetails;
            std::string mName;
            uint32_t mTimeout; 
            std::string mActionJson;
            uint32_t mCurrentKeyIndex;
            double mTotalUsedTime;
    };

    void populateEasterEggDetails();
    void checkEasterEggs(uint32_t keyCode, uint32_t flags, double time);
}

#endif //RDKSHELL_EASTER_EGG_H
