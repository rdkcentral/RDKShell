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

#pragma once

namespace RdkShell
{
    enum DeviceType
    {
       GenericLinuxInputDev = 0xfe, // Generic HID device (only used for testing)
       VNC_Client = 0xe0, // All input received from VNC server
       Generic_IR = 0xf0, // Infrared (EventSource::Infrared)
       FrontPanel = 0xff, // FrontPanel (EventSource::FrontPanel)
    };

    enum DeviceModeFlags
    {
       TrackPadDisabled = 0x01,
       SliderDisabled = 0x02,
       DirectionKeysDisabled = 0x04,
       TrickModeKeysDisabled = 0x08,
    };

}
