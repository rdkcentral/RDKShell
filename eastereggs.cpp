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

#include "eastereggs.h"
#include "rdkshelljson.h"
#include <iostream>
#include "logger.h"
#include "compositorcontroller.h"
#include "rdkshell.h"
#include "rdkshelldata.h"
#include "linuxkeys.h"

#include <map>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <algorithm>

#define RDKSHELL_720_EASTER_EGG_FILE "/tmp/rdkshell720"

namespace RdkShell
{
    static std::vector<EasterEgg> sEasterEggs;
    static bool sMatchedAnyEasterEgg = false;
    static double sEasterEggResolveTime = 0.0;

    EasterEgg::EasterEgg (std::vector<RdkShellEasterEggKeyDetails>& details, std::string name, uint32_t timeout, std::string actionJson):mKeyDetails(details), mName(name), mTimeout(timeout), mActionJson(actionJson), mCurrentKeyIndex(0), mTotalUsedTime(0.0), mSatisfied(false)
    {
    }

    void EasterEgg::checkKey(uint32_t keyCode, uint32_t flags, double time)
    {
        mTotalUsedTime += time;
        struct RdkShellEasterEggKeyDetails& keyToCheck = mKeyDetails[mCurrentKeyIndex];
        bool emptyFlagsMatched = false;
        if ((keyToCheck.keyModifiers == 0) && (flags == 0))
        {
            emptyFlagsMatched = true;
        }
        if ((keyToCheck.keyCode == keyCode) && ((true == emptyFlagsMatched) || (keyToCheck.keyModifiers & flags)) && (keyToCheck.keyHoldTime <= time) && (mTotalUsedTime <= mTimeout))
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Debug, "Easter Eggs - Matched portion key: %u modifier:%u", keyToCheck.keyCode, keyToCheck.keyModifiers);
            size_t numberOfKeys = mKeyDetails.size();
            if (mCurrentKeyIndex == (numberOfKeys - 1))
            {
                mSatisfied = true;
                mTotalUsedTime = 0.0;
                sMatchedAnyEasterEgg = true;
            }
            mCurrentKeyIndex = (mCurrentKeyIndex+1)%numberOfKeys;
        }
        else
        {
            mSatisfied = false;
            mCurrentKeyIndex = 0;
            mTotalUsedTime = 0.0;
        }
    }

    void EasterEgg::toggleForce720()
    {
        std::ifstream file720(RDKSHELL_720_EASTER_EGG_FILE);
        if (file720.good())
        {
            std::cout << "removing 720 restriction \n";
            remove( RDKSHELL_720_EASTER_EGG_FILE );
        }
        else
        {
            std::cout << "adding 720 restriction \n";
            std::ofstream outputFile(RDKSHELL_720_EASTER_EGG_FILE);
            outputFile.close();
        }
        system("systemctl restart wpeframework &");
    }

    bool EasterEgg::invokeEvent()
    {
        bool ret = false;
        if (mSatisfied)
        {
            std::vector<std::map<std::string, RdkShell::RdkShellData>> eventData(1);
            eventData[0] = std::map<std::string, RdkShell::RdkShellData>();
            eventData[0]["name"] = mName;
            eventData[0]["action"] = mActionJson;
            if (mName == "RDKSHELL_FORCE_720")
            {
                std::cout << "about to toggle force 720 easter egg\n";
                toggleForce720();
            }
            else
            {
                RdkShell::CompositorController::sendEvent("onEasterEgg", eventData);
            }
            ret = true;
        }
        mSatisfied = false;
        return ret;
    }

    void EasterEgg::reset()
    {
        mSatisfied = false;
        mCurrentKeyIndex = 0;
        mTotalUsedTime = 0.0;
    }

    size_t EasterEgg::numberOfKeys()
    {
        return mKeyDetails.size();
    }

    bool compareKeySize (EasterEgg& first, EasterEgg& second)
    {
      return first.numberOfKeys() >= second.numberOfKeys()?true:false;
    }

    void populateEasterEggDetails()
    {
        //populate from the easter egg configuration file
        const char* easterEggFile = getenv("RDKSHELL_EASTER_EGG_FILE");
        if (easterEggFile)
        {
            rapidjson::Document document;
            bool ret = RdkShell::RdkShellJson::readJsonFile(easterEggFile, document);
            if (false == ret)
            {
                std::cout << "RDKShell easter egg file read error : [unable to open/read file (" <<  easterEggFile << ")]\n";
                return;
            }
      
            if (document.HasMember("easterEggs"))
            {
                const rapidjson::Value& jsonValue = document["easterEggs"];
      
                if (jsonValue.IsArray())
                {
                    for (rapidjson::SizeType k = 0; k < jsonValue.Size(); k++)
                    {
                        std::string name("");
                        uint32_t timeout = 0;
                        std::string actionJson("");
      
                        const rapidjson::Value& easterEggEntry = jsonValue[k];
      
                        //check for entry validity
                        if (!(easterEggEntry.IsObject() && easterEggEntry.HasMember("sequence") && easterEggEntry.HasMember("timeout") && easterEggEntry.HasMember("name")))
                        {
                            std::cout << "one of easter egg entry is of wrong format or not having sequence/timeout/name parameter" << std::endl;
                            continue;
                        }
      
                        //populate easter egg sequence
                        const rapidjson::Value& easterEggSequence = easterEggEntry["sequence"];
                        if (!easterEggSequence.IsArray())
                        {
                            std::cout << "one of easter egg entry has non-array type sequence" << std::endl;
                            continue;
                        }
      
                        std::vector<RdkShellEasterEggKeyDetails> keyDetailsList;
      
                        for (rapidjson::SizeType j = 0; j < easterEggSequence.Size(); j++)
                        {
                            uint32_t keyCode = 0;
                            uint32_t keyHoldTime = 0;
                            uint32_t keyModifiers = 0;
      
                            const rapidjson::Value& easterEggKeyDetail = easterEggSequence[j];
                            if (!(easterEggKeyDetail.IsObject() && easterEggKeyDetail.HasMember("keyCode")))
                            {
                                std::cout << "Ignoring easter egg entry because of format issues of sequence parameter\n";
                                continue;
                            }
      
                            const rapidjson::Value& keyCodeValue = easterEggKeyDetail["keyCode"];
                            if (keyCodeValue.IsUint())
                            {
                                keyCode = keyCodeValue.GetUint();
                            }
                            else
                            {
                                std::cout << "Ignoring easter egg entry because of wrong type of keycode\n";
                                continue;
                            }
      
                            if (easterEggKeyDetail.HasMember("hold"))
                            {
                                keyHoldTime = easterEggKeyDetail["hold"].GetUint();
                            }
                            if (easterEggKeyDetail.HasMember("modifiers"))
                            {
                                keyModifiers = easterEggKeyDetail["modifiers"].GetUint();
                            }
                            
                            struct RdkShellEasterEggKeyDetails keyDetail;
                            keyDetail.keyCode = keyCode;
                            keyDetail.keyHoldTime = keyHoldTime;
                            keyDetail.keyModifiers = keyModifiers;
                            keyDetailsList.push_back(keyDetail);
                        }
      
                        //populate easter egg name
                        const rapidjson::Value& easterEggNameValue = easterEggEntry["name"];
                        if (!easterEggNameValue.IsString())
                        {
                            std::cout << "one of easter egg entry has non-string type name" << std::endl;
                            continue;
                        }
                        name = easterEggNameValue.GetString();
      
                        //populate easter egg timeout
                        const rapidjson::Value& easterEggTimeoutValue = easterEggEntry["timeout"];
                        if (!easterEggTimeoutValue.IsUint())
                        {
                            std::cout << "one of easter egg entry has non-integer type timeout" << std::endl;
                            continue;
                        }
                        timeout = easterEggTimeoutValue.GetUint();
      
                        //populate easter egg action
                        if (easterEggEntry.HasMember("action"))
                        {
                            const rapidjson::Value& easterEggActionValue = easterEggEntry["action"];
                            if (!easterEggActionValue.IsObject())
                            {
                                std::cout << "one of easter egg entry has non-object type action" << std::endl;
                                continue;
                            }
                            rapidjson::StringBuffer actionBuffer ;
                            rapidjson::Writer<rapidjson::StringBuffer> writer(actionBuffer) ;
                            easterEggActionValue.Accept (writer) ;
                            actionJson =  actionBuffer.GetString();
                        }
      
                        EasterEgg easterEggObject(keyDetailsList, name, timeout, actionJson);
                        sEasterEggs.push_back(easterEggObject);
                    }
                }
                else
                {
                  std::cout << "Ignored file read due to easterEggs entry not present";
                }
            }
        }
        else
        {
          std::cout << "Ignored file read due to easter egg environment variable not set\n";
        }
        std::sort<std::vector<EasterEgg>::iterator>(sEasterEggs.begin(), sEasterEggs.end(), compareKeySize);
    }
    
    void checkEasterEggs(uint32_t keyCode, uint32_t flags, double keyPressTime)
    {
        if ((keyCode == RDKSHELL_KEY_CTRL) || (keyCode == RDKSHELL_KEY_ALT) || (keyCode == RDKSHELL_KEY_SHIFT))
        {
           return;
        }
        for (int i=0; i < sEasterEggs.size(); i++)
        {
           EasterEgg& easterEgg = sEasterEggs[i];
           easterEgg.checkKey(keyCode, flags, keyPressTime);
        }
        if (sMatchedAnyEasterEgg)
        {
            sEasterEggResolveTime = RdkShell::seconds() + 1.0;
        }
    }

    void resolveWaitingEasterEggs()
    {
        if (!sMatchedAnyEasterEgg)
        {
            return;
        }
        double currentTime = RdkShell::seconds();
        if (currentTime > sEasterEggResolveTime)
        {
            bool invokedEvent = false;
            for (int i=0; i<sEasterEggs.size(); i++)
            {
                EasterEgg& easterEgg = sEasterEggs[i];
                if ((false == invokedEvent) && (true == easterEgg.invokeEvent()))
                {
                    invokedEvent = true;
                }
                easterEgg.reset();
            }
            sMatchedAnyEasterEgg = false;
            sEasterEggResolveTime = 0.0;
        }
        // perform reset 
    }
}
