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

#include <typeindex>
#include <string>

namespace RdkShell
{
    union RdkShellDataInfo
    {
        bool        booleanData;
        int8_t      integer8Data;
        int32_t     integer32Data;
        int64_t     integer64Data;
        uint8_t     unsignedInteger8Data;
        uint32_t    unsignedInteger32Data;
        uint64_t    unsignedInteger64Data;
        float       floatData;
        double      doubleData;
        std::string stringData;
        void*       pointerData;
        RdkShellDataInfo() {}
        ~RdkShellDataInfo() {}
    };

    class RdkShellData
    {
        public:
            RdkShellData();
            RdkShellData(bool data);
            RdkShellData(int8_t data);
            RdkShellData(int32_t data);
            RdkShellData(int64_t data);
            RdkShellData(uint8_t data);
            RdkShellData(uint32_t data);
            RdkShellData(uint64_t data);
            RdkShellData(float data);
            RdkShellData(double data);
            RdkShellData(std::string data);
            RdkShellData(void* data);
            
            bool toBoolean() const;
            int8_t toInteger8() const;
            int32_t toInteger32() const;
            int64_t toInteger64() const;
            uint8_t toUnsignedInteger8() const;
            uint32_t toUnsignedInteger32() const;
            uint64_t toUnsignedInteger64() const;
            float toFloat() const;
            double toDouble() const;
            std::string toString() const;
            void* toVoidPointer() const;

            RdkShellData& operator=(bool value);
            RdkShellData& operator=(int8_t value);
            RdkShellData& operator=(int32_t value);
            RdkShellData& operator=(int64_t value);
            RdkShellData& operator=(uint8_t value);
            RdkShellData& operator=(uint32_t value);
            RdkShellData& operator=(uint64_t value);
            RdkShellData& operator=(float value);
            RdkShellData& operator=(double value);
            RdkShellData& operator=(const char* value);
            RdkShellData& operator=(const std::string& value);
            RdkShellData& operator=(void* value);
            RdkShellData& operator=(const RdkShellData& value);

            std::type_index dataTypeIndex();

        private:
            std::type_index mDataTypeIndex;
            RdkShellDataInfo mData;

            void setData(std::type_index typeIndex, void* data);
    };
}