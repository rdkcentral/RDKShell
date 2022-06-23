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

#include "permissions.h"
#include "rdkshelljson.h"
#include "logger.h"

#include <sstream>
#include <map>
#include <algorithm>

namespace 
{
    std::map<std::string, std::vector<std::string>> gClientsPermittedExtensions;
    std::vector<std::string> gClientsDefaultPermittedExtensions;
    std::string gDefaultRenderer;
}

namespace RdkShell
{
    void readPermissionsConfiguration()
    {
        const char* configFilePath = getenv("RDKSHELL_CLIENT_PERMISSIONS_CONFIG");
        if (configFilePath == nullptr)
        {
            Logger::log(LogLevel::Warn, "%s: RDKSHELL_CLIENT_PERMISSIONS_CONFIG not specified\n", __func__);
            return;
        }
        Logger::log(LogLevel::Information, "Reading Permissions config from: %s", configFilePath);

        rapidjson::Document document;
        bool ret = RdkShellJson::readJsonFile(configFilePath, document);
        if (ret == false)
        {
            Logger::log(LogLevel::Error, "%s: unabled to read permission configuration file: '%s'\n", __func__, configFilePath);
            return;
        }

        if (document.HasMember("clients"))
        {
            const rapidjson::Value& clients = document["clients"];

            if (clients.IsArray())
            {
                for (rapidjson::SizeType i = 0; i < clients.Size(); ++i)
                {
                    const rapidjson::Value& entry = clients[i];
                    if (entry.IsObject() &&
                        entry.HasMember("client") &&
                        entry.HasMember("extensions"))
                    {
                        const rapidjson::Value& extensionsValue = entry["extensions"];
                        
                        if (extensionsValue.IsArray())
                        {
                            std::vector<std::string> entryExtensions;

                            for (rapidjson::SizeType j = 0; j < extensionsValue.Size(); ++j)
                            {
                                const rapidjson::Value& extesionValue = extensionsValue[j];
                                if (extesionValue.IsString())
                                {
                                    entryExtensions.push_back(extesionValue.GetString());
                                }
                                else
                                {
                                    Logger::log(LogLevel::Error, "%s: incorrect extensions\n", __func__);
                                    continue;
                                }                                
                            }

                            const rapidjson::Value& clientValue = entry["client"];
                            if (clientValue.IsString())
                            {
                                std::string entryClient = clientValue.GetString();
                                std::transform(entryClient.begin(), entryClient.end(), entryClient.begin(),
                                    [](unsigned char c){ return std::tolower(c); });

                                if (gClientsPermittedExtensions.count(entryClient) == 0)
                                {
                                    gClientsPermittedExtensions[entryClient] = entryExtensions;
                                }
                                else
                                {
                                    Logger::log(LogLevel::Error, "%s: multiple entries for the same client\n", __func__);
                                    continue;
                                }
                            }
                            else
                            {
                                Logger::log(LogLevel::Error, "%s: incorrect client\n", __func__);
                                continue;
                            }
                        }
                        else
                        {
                            Logger::log(LogLevel::Error, "%s: extensions entry not an array\n", __func__);
                            continue;
                        }
                    }
                    else
                    {
                        Logger::log(LogLevel::Error, "%s: incorrect config structure\n", __func__);
                        continue;
                    } 
                }
            }
            else
            {
                Logger::log(LogLevel::Error, "%s: \"clients\" not an array\n", __func__);
            }            
        }
        
        if (document.HasMember("default"))
        {
            const rapidjson::Value& defaultValue = document["default"];

            if (defaultValue.IsObject())
            {
                if (defaultValue.HasMember("extensions"))
                {
                    const rapidjson::Value& extensionsValue = defaultValue["extensions"];

                    if (extensionsValue.IsArray())
                    {
                        for (rapidjson::SizeType j = 0; j < extensionsValue.Size(); ++j)
                        {
                            const rapidjson::Value& extesionValue = extensionsValue[j];
                            if (extesionValue.IsString())
                            {
                                gClientsDefaultPermittedExtensions.push_back(extesionValue.GetString());
                            }
                            else
                            {
                                Logger::log(LogLevel::Error, "%s: incorrect extensions\n", __func__);
                                continue;
                            }
                        }
                    }
                    else
                    {
                        Logger::log(LogLevel::Error, "%s: extensions entry not an array\n", __func__);
                    }
                }

                if (defaultValue.HasMember("renderer"))
                {
                    const rapidjson::Value& defaultRenderer = defaultValue["renderer"];

                    if (defaultRenderer.IsString())
                    {
                        gDefaultRenderer = defaultRenderer.GetString();
                    }
                    else
                    {
                        Logger::log(LogLevel::Error, "%s: incorrect default renderer\n", __func__);
                    }
                }
            }
            else
            {
                Logger::log(LogLevel::Error, "%s: incorrect config structure\n", __func__);
            } 
        }
    }

    void getAllowedExtensions(const std::string& clientName, std::vector<std::string>& extensions)
    {
        if (gClientsPermittedExtensions.count(clientName) > 0)
        {
            extensions = gClientsPermittedExtensions[clientName];
        }
        else
        {
            extensions = gClientsDefaultPermittedExtensions;
        }        
    }

    std::string getRenderer()
    {
        return gDefaultRenderer;
    }

}

