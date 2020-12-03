#include "permissions.h"
#include "rdkshelljson.h"
#include "logger.h"

#include <sstream>
#include <map>
#include <algorithm>

namespace RdkShell
{
    std::map<std::string, std::vector<std::string>> gClientsPermittedExtensions;
    std::vector<std::string> gClientsDefaultPermittedExtensions;

    void readPermissionsConfiguration()
    {
        const char* configFilePath = getenv("RDKSHELL_CLIENT_PERMISSIONS_CONFIG");
        if (configFilePath == nullptr)
        {
            Logger::log(LogLevel::Warn, "%s: RDKSHELL_CLIENT_PERMISSIONS_CONFIG not specified\n", __func__);
            return;
        }

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

            if (defaultValue.IsObject() &&
                defaultValue.HasMember("extensions"))
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
            else
            {
                Logger::log(LogLevel::Error, "%s: incorrect config structure\n", __func__);
            } 
        }

        logPermissionsConfiguration();
    }

    void logPermissionsConfiguration()
    {
        std::stringstream ss;
        ss << __func__ << " default permissions: ";
        for (int i = 0; i < gClientsDefaultPermittedExtensions.size(); ++i)
        {
            ss << gClientsDefaultPermittedExtensions[i] << " ";
        }
        ss << "\n";

        ss << __func__ << " permissions:\n";
        for (auto it = gClientsPermittedExtensions.begin(); it != gClientsPermittedExtensions.end(); ++it)
        {
            ss << it->first << ": ";
            auto exts = it->second;
            for (int j = 0; j < exts.size(); ++j)
            {
                ss << exts[j] << " ";
            }
            ss << "\n";
        }

        Logger::log(LogLevel::Information, ss.str().c_str());
    }

    void getAllowedExtensions(const std::string& clientName, std::vector<std::string>& extensions)
    {
        extensions.clear();

        if (gClientsPermittedExtensions.count(clientName) > 0)
        {
            extensions = gClientsPermittedExtensions[clientName];
        }
        else
        {
            extensions = gClientsDefaultPermittedExtensions;
        }        
    }

}

