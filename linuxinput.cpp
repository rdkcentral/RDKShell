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
                    for (rapidjson::SizeType k = 0; k < inputDevices.Size(); k++)
                    {
                        const rapidjson::Value& mapEntry = inputDevices[k];
                        if (mapEntry.IsObject() && mapEntry.HasMember("vendor") && mapEntry.HasMember("product") && mapEntry.HasMember("deviceType"))
                        {
                            LinuxInputDevice inputDevice;

                            //vendor
                            const rapidjson::Value& vendorValue = mapEntry["vendor"];
                            if (vendorValue.IsString())
                            {
                                inputDevice.vendor = static_cast<uint16_t>(std::strtoul(vendorValue.GetString(),nullptr,16));
                            }
                            else
                            {
                                std::cout << "ignoring vendor entry because of format issues of vendor\n";
                                continue;
                            }

                            //product
                            const rapidjson::Value& productValue = mapEntry["product"];
                            if (productValue.IsString())
                            {
                                inputDevice.product = static_cast<uint16_t>(std::strtoul(productValue.GetString(),nullptr,16));
                            }
                            else
                            {
                                std::cout << "ignoring product entry because of format issues of product\n";
                                continue;
                            }

                            //deviceType
                            const rapidjson::Value& deviceTypeValue = mapEntry["deviceType"];
                            if (deviceTypeValue.IsString())
                            {
                                inputDevice.deviceType = static_cast<uint8_t>(std::strtoul(deviceTypeValue.GetString(),nullptr,10));
                            }
                            else
                            {
                                std::cout << "ignoring deviceType entry because of format issues of deviceType\n";
                                continue;
                            }

                            //deviceMode
                            const rapidjson::Value& deviceModeValue = mapEntry["deviceMode"];
                            if (deviceModeValue.IsString())
                            {
                                inputDevice.deviceMode = static_cast<uint8_t>(std::strtoul(deviceModeValue.GetString(),nullptr,10));
                            }
                            else
                            {
                                std::cout << "ignoring deviceMode entry because of format issues of deviceMode\n";
                                continue;
                            }

                            std::cout << "readInputDevicesConfiguration store { product: 0x" << std::hex << std::setw(4) << std::setfill('0') << inputDevice.product <<
                                                       ", vendor: 0x"  << std::hex << std::setw(4) << std::setfill('0') << inputDevice.vendor <<
                                                       ", deviceType: " << std::to_string(inputDevice.deviceType) << 
                                                       ", deviceMode: " << std::to_string(inputDevice.deviceMode) << " }" << std::endl;

                            gRdkShellInputDevices.push_back(inputDevice);
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
        }
        else
        {
            std::cout << "Ignored file read due to input devices environment variable not set\n";
        }
    }

    void inputDeviceTypeAndMode(uint16_t vendor, uint16_t product, uint8_t& type, uint8_t& mode)
    {
        type = static_cast<uint8_t>(RdkShell::DeviceType::GenericLinuxInputDev);
        mode = 0x03;

        auto it = std::find_if(std::begin(gRdkShellInputDevices), std::end(gRdkShellInputDevices),
                [vendor, product](const RdkShell::LinuxInputDevice& record)
                {
                    return record.vendor == vendor and record.product == product;
                });

        if (it != std::end(gRdkShellInputDevices))
        {
            type = it->deviceType;
            mode = it->deviceMode;
        }
    }

    std::vector<LinuxInputDevice> inputDevices()
    {
        return gRdkShellInputDevices;
    }
}
