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

struct InputEvent
{
    uint32_t deviceId;
    uint32_t timestampMs;
    enum Type { InvalidEvent, KeyEvent, TouchPadEvent, SliderEvent } type;

    union Details
    {
        struct Key
        {
            int code;
            enum State { Pressed, Released, VirtualPress, VirtualRelease } state;
        } key;

        struct TouchPad
        {
            int x, y;
            enum State { Down, Up, Click } state;
        } touchpad;

        struct Slider
        {
            int x;
            enum State { Down, Up } state;
        } slider;

    } details;

    InputEvent()
    : deviceId(0), timestampMs(0), type(InvalidEvent), details()
    { }

    InputEvent(uint32_t id, uint32_t ts, Type t)
    : deviceId(id), timestampMs(ts), type(t), details()
    { }
};

}
