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

#ifndef RDKSHELL_ESSOS_INSTANCE_H
#define RDKSHELL_ESSOS_INSTANCE_H

#include <essos.h>


namespace RdkShell
{
    class EssosInstance
    {
        public:
            ~EssosInstance();
            static EssosInstance* instance();
            void initialize(bool useWayland);
            void initialize(bool useWayland, uint32_t width, uint32_t height);
            void configureKeyInput(uint32_t initialDelay, uint32_t repeatInterval);
            void onKeyPress(uint32_t keyCode, unsigned long flags);
            void onKeyRelease(uint32_t keyCode, unsigned long flags);
            void onDisplaySizeChanged(uint32_t width, uint32_t height);
            void update();
            void resolution(uint32_t &width, uint32_t &height);
            void setResolution(uint32_t width, uint32_t height);


        private:
            EssosInstance();
            static EssosInstance* mInstance;
            EssCtx * mEssosContext;
            bool mUseWayland;
            uint32_t mWidth;
            uint32_t mHeight;
            bool mOverrideResolution;
            uint32_t mKeyInitialDelay;
            uint32_t mKeyRepeatInterval;
    };
}

#endif //RDKSHELL_ESSOS_INSTANCE_H
