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

#include "linuxinput.h"
#include "rdkshelljson.h"
#include "inputdevicetypes.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>

namespace RdkShell
{
    std::vector<LinuxInputDevice> gRdkShellInputDevices;
    std::vector<IrInputDeviceTypeMapping> gIrInputDeviceTypeMapping;

    void readInputDevicesConfiguration()
    {
        const char* inputDeviceConfigurationFile = getenv("RDKSHELL_INPUT_DEVICES_CONFIG");
        if (inputDeviceConfigurationFile)
        {
            std::cout << "readInputDevicesConfiguration: read configuration from: '" << inputDeviceConfigurationFile << "'"<< std::endl;

            rapidjson::Document document;
            bool ret = RdkShell::RdkShellJson::readJsonFile(inputDeviceConfigurationFile, document);
            if (ret == false)
            {
                std::cout << "RDKShell input devices configuration read error : [unable to open/read file (" <<  inputDeviceConfigurationFile << ")]\n";
                return;
            }

            if (document.HasMember("inputDevices"))
            {
                const rapidjson::Value& inputDevices = document["inputDevices"];

                if (inputDevices.IsArray())
                {
                    for (rapidjson::SizeType i = 0; i < inputDevices.Size(); i++)
                    {
                        const rapidjson::Value& mapEntry = inputDevices[i];
                        if (mapEntry.IsObject() &&
                            mapEntry.HasMember("deviceType") &&
                            mapEntry.HasMember("deviceMode"))
                        {
                            LinuxInputDevice inputDeviceEntry = {};

                            //vendor
                            if (mapEntry.HasMember("vendor"))
                            {
                                const rapidjson::Value& vendorValue = mapEntry["vendor"];
                                if (vendorValue.IsString())
                                {
                                    inputDeviceEntry.vendor = static_cast<uint16_t>(std::strtoul(vendorValue.GetString(),nullptr,16));
                                }
                                else
                                {
                                    std::cout << "Ignoring inputDevices entry because of format issues of vendor\n";
                                    continue;
                                }
                            }
                            else
                            {
                                inputDeviceEntry.vendor = 0;
                            }


                            //product
                            if (mapEntry.HasMember("product"))
                            {
                                const rapidjson::Value& productValue = mapEntry["product"];
                                if (productValue.IsString())
                                {
                                    inputDeviceEntry.product = static_cast<uint16_t>(std::strtoul(productValue.GetString(),nullptr,16));
                                }
                                else
                                {
                                    std::cout << "Ignoring inputDevices entry because of format issues of product\n";
                                    continue;
                                }
                            }
                            else
                            {
                                inputDeviceEntry.product = 0;
                            }


                            //devicePath
                            if (mapEntry.HasMember("devicePath"))
                            {
                                const rapidjson::Value& devicePathValue = mapEntry["devicePath"];
                                if (devicePathValue.IsString())
                                {
                                    inputDeviceEntry.devicePath = devicePathValue.GetString();
                                }
                                else
                                {
                                    std::cout << "Ignoring inputDevices entry because of format issues of devicePathValue\n";
                                    continue;
                                }
                            }
                            else
                            {
                                inputDeviceEntry.devicePath = {};
                            }


                            //deviceType
                            const rapidjson::Value& deviceTypeValue = mapEntry["deviceType"];
                            if (deviceTypeValue.IsString())
                            {
                                inputDeviceEntry.deviceType = static_cast<uint8_t>(std::strtoul(deviceTypeValue.GetString(),nullptr,16));
                            }
                            else
                            {
                                std::cout << "Ignoring inputDevices entry because of format issues of deviceType\n";
                                continue;
                            }

                            //deviceMode
                            const rapidjson::Value& deviceModeValue = mapEntry["deviceMode"];
                            if (deviceModeValue.IsString())
                            {
                                inputDeviceEntry.deviceMode = static_cast<uint8_t>(std::strtoul(deviceModeValue.GetString(),nullptr,16));
                            }
                            else
                            {
                                std::cout << "Ignoring inputDevices entry because of format issues of deviceMode\n";
                                continue;
                            }


                            std::cout << "inputDevice add entry: " <<
                                                       "{ product: 0x" << std::hex << std::setw(4) << std::setfill('0') << inputDeviceEntry.product <<
                                                       ", vendor: 0x"  << std::hex << std::setw(4) << std::setfill('0') << inputDeviceEntry.vendor <<
                                                       ", deviceType: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(inputDeviceEntry.deviceType) <<
                                                       ", deviceMode: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(inputDeviceEntry.deviceMode) <<
                                                       ", devicePath: '" << inputDeviceEntry.devicePath <<  "'"  << " }" << std::endl;

                            gRdkShellInputDevices.push_back(inputDeviceEntry);
                        }
                        else
                        {
                            std::cout << "Ignoring input device entry because of format issues of input device entry\n";
                            continue;
                        }
                    }
                }
            }
            else
            {
                std::cout << "Ignored file read due to inputDevices entry not present";
            }

            if (document.HasMember("irInputDeviceTypeMapping"))
            {
                const rapidjson::Value& irDeviceTypeMapping = document["irInputDeviceTypeMapping"];

                if (irDeviceTypeMapping.IsArray())
                {
                    for (rapidjson::SizeType i = 0; i < irDeviceTypeMapping.Size(); i++)
                    {
                        const rapidjson::Value& irDeviceTypeEntry = irDeviceTypeMapping[i];

                        if (irDeviceTypeEntry.IsObject() &&
                            irDeviceTypeEntry.HasMember("filterCode") &&
                            irDeviceTypeEntry.HasMember("deviceType"))
                        {
                           IrInputDeviceTypeMapping irDeviceMapping = {};

                           const rapidjson::Value& filterCodeValue = irDeviceTypeEntry["filterCode"];
                           if (filterCodeValue.IsUint())
                           {
                               irDeviceMapping.filterCode = filterCodeValue.GetUint();
                           }
                           else
                           {
                               std::cout << "Ignoring irInputDeviceTypeMapping entry because of format issues of filterCode\n";
                               continue;
                           }

                           const rapidjson::Value& typeValue = irDeviceTypeEntry["deviceType"];
                           if (typeValue.IsString())
                           {
                               irDeviceMapping.deviceType = static_cast<uint8_t>(std::strtoul(typeValue.GetString(),nullptr,16));
                           }
                           else
                           {
                               std::cout << "Ignoring irInputDeviceTypeMapping entry because of format issues of deviceType\n";
                               continue;
                           }

                           std::cout << "irDeviceTypeMapping add entry: " <<
                                        "{ filterCode: " << std::dec << static_cast<uint16_t>(irDeviceMapping.filterCode) <<
                                        ", deviceType: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(irDeviceMapping.deviceType) << " }" << std::endl;

                           gIrInputDeviceTypeMapping.push_back(irDeviceMapping);
                        }
                    }
                }
            }
            else
            {
                std::cout << "irDeviceTypeMapping entry not present";
            }
        }
        else
        {
            std::cout << "Ignored file read due to input devices environment variable not set\n";
        }
    }

    void inputDeviceTypeAndMode(const uint16_t vendor, const uint16_t product, const std::string& devicePath, uint8_t& type, uint8_t& mode)
    {
        if (vendor == 0x0 and product == 0x0 and devicePath.empty() == false)
        {
            auto it = std::find_if(std::begin(gRdkShellInputDevices), std::end(gRdkShellInputDevices),
                        [devicePath](const RdkShell::LinuxInputDevice& e)
                        {
                            return e.devicePath == devicePath;
                        });

            if (it != std::end(gRdkShellInputDevices))
            {
               type = it->deviceType;
               mode = it->deviceMode;
            }
        }
        else
        {
            auto it = std::find_if(std::begin(gRdkShellInputDevices), std::end(gRdkShellInputDevices),
                        [vendor, product](const RdkShell::LinuxInputDevice& e)
                        {
                            return e.vendor == vendor and e.product == product;
                        });

            if (it != std::end(gRdkShellInputDevices))
            {
               type = it->deviceType;
               mode = it->deviceMode;
            }
        }
    }

    void irDeviceType(const uint8_t filterCode, uint8_t& type)
    {
        type = static_cast<uint8_t>(RdkShell::DeviceType::Generic_IR);

        auto it = std::find_if(std::begin(gIrInputDeviceTypeMapping), std::end(gIrInputDeviceTypeMapping),
                [filterCode](const RdkShell::IrInputDeviceTypeMapping& e)
                {
                    return e.filterCode == filterCode;
                });

        if (it != std::end(gIrInputDeviceTypeMapping))
        {
            type = it->deviceType;
        }
    }
}
