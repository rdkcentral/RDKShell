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

#include "compositorcontroller.h"

#include "essosinstance.h"
#include "animation.h"
#include "rdkshell.h"
#include "application.h"
#include "logger.h"
#include "linuxkeys.h"
#include "eastereggs.h"
#include "rdkcompositornested.h"
#include "rdkcompositorsurface.h"
#include "string.h"
#include "rdkshellimage.h"
#include "rdkshellrect.h"
#include "cursor.h"
#include "scene.h"
#include <iostream>
#include <map>
#include <ctime>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define RDKSHELL_DEFAULT_INACTIVITY_TIMEOUT_IN_SECONDS 15*60
#define RDKSHELL_WILDCARD_KEY_CODE 255
#define RDKSHELL_WATERMARK_ID 65536

namespace RdkShell
{
    struct KeyInterceptInfo
    {
        KeyInterceptInfo() : keyCode(-1), flags(0), compositor(nullptr) {}
        uint32_t keyCode;
        uint32_t flags;
        std::shared_ptr<RdkCompositor> compositor;
    };

    enum RdkShellCompositorType
    {
        NESTED,
        SURFACE
    };

    struct WatermarkImage
    {
        WatermarkImage(uint32_t imageId, uint32_t imageZOrder): id(imageId), zorder(imageZOrder), image(nullptr) {}
        uint32_t id;
        uint32_t zorder;
        std::shared_ptr<RdkShell::Image> image;
    };

    struct KeyRepeatConfig
    {
        KeyRepeatConfig() : enabled(false), initialDelay(500), repeatInterval(250) {}
        int initialDelay;
        int repeatInterval;
        bool enabled;
    };

    std::shared_ptr<RdkCompositor> gFocusedCompositor;
    std::vector<std::shared_ptr<RdkCompositor>> gPendingKeyUpListeners;
    std::vector<std::shared_ptr<RdkCompositor>> gDeletedCompositors;

    static std::map<uint32_t, std::vector<KeyInterceptInfo>> gKeyInterceptInfoMap;

    bool gEnableInactivityReporting = false;
    double gInactivityIntervalInSeconds = RDKSHELL_DEFAULT_INACTIVITY_TIMEOUT_IN_SECONDS;
    double gLastKeyEventTime = RdkShell::seconds();
    double gNextInactiveEventTime = RdkShell::seconds() + gInactivityIntervalInSeconds;
    uint32_t gLastKeyCode = 0;
    uint32_t gLastKeyModifiers = 0;
    uint64_t gLastKeyMetadata = 0;
    std::shared_ptr<RdkShellEventListener> gRdkShellEventListener;
    double gLastKeyPressStartTime = 0.0;
    double gLastKeyRepeatTime = 0.0;
    RdkShellCompositorType gRdkShellCompositorType = NESTED;
    std::shared_ptr<RdkShell::Image> gSplashImage = nullptr;
    bool gShowSplashImage = false;
    uint32_t gSplashDisplayTimeInSeconds = 0;
    double gSplashStartTime = 0;
    std::shared_ptr<RdkShell::Image> gRdkShellWatermarkImage = nullptr;
    std::vector<WatermarkImage> gWatermarkImages;
    bool gShowWatermarkImage = false;
    bool gAlwaysShowWatermarkImageOnTop = false;
    std::shared_ptr<RdkShell::Image> gFullScreenImage = nullptr;
    bool gShowFullScreenImage = false;
    std::string gCurrentFullScreenImage = "";
    uint32_t gPowerKeyCode = 0;
    uint32_t gFrontPanelButtonCode = 0;
    bool gPowerKeyEnabled = false;
    bool gPowerKeyReleaseReceived = false;
    bool gRdkShellPowerKeyReleaseOnlyEnabled = false;
    bool gIgnoreKeyInputEnabled = false;
    std::shared_ptr<Cursor> gCursor = nullptr;
    KeyRepeatConfig gKeyRepeatConfig;
    Scene gScene;

    std::string standardizeName(const std::string& clientName)
    {
        std::string displayName = clientName;
        std::transform(displayName.begin(), displayName.end(), displayName.begin(), [](unsigned char c){ return std::tolower(c); });
        return displayName;
    }
    
    bool interceptKey(uint32_t keycode, uint32_t flags, uint64_t metadata, bool isPressed)
    {
        if (gKeyInterceptInfoMap.find(keycode) == gKeyInterceptInfoMap.end())
        {
            Logger::log(Debug, "No intercepts found for key %d %s", keycode, isPressed ? "press" : "release");
            return false;
        }

        bool intercepted = false;
        auto& intercepts = gKeyInterceptInfoMap[keycode];

        for (auto& intercept : intercepts)
        {
            auto& compositor = intercept.compositor;
            if (intercept.flags == flags && compositor->getInputEventsEnabled())
            {
                intercepted = true;
                Logger::log(Debug, "Key %d intercepted by client %s", keycode, compositor->name().c_str());

                if (isPressed)
                {
                    compositor->onKeyPress(keycode, flags, metadata);
                }
                else
                {
                    compositor->onKeyRelease(keycode, flags, metadata);
                }
            }
        }

        return intercepted;
    }


    // NOTE: This is really difficult to understand
    void bubbleKey(uint32_t keycode, uint32_t flags, uint64_t metadata, bool isPressed)
    {
        if (isPressed)
        {
            gFocusedCompositor->onKeyPress(keycode, flags, metadata);
            // NOTE: gPendingKeyUpListeners?
        }
        else
        {
            gFocusedCompositor->onKeyRelease(keycode, flags, metadata);
        }

        auto nodes = gScene.clients();
        auto nodeIt = nodes.rbegin();

        #ifndef RDKSHELL_ENABLE_KEYBUBBING_TOP_MODE
        // NOTE: Start with the focused compositor
        for (; nodeIt != nodes.rend(); nodeIt++)
        {
          if (*nodeIt == gFocusedCompositor)
          {
            break;
          }
        }
        #else
        Logger::log(Debug, "Key bubbling is made from top application");
        #endif //RDKSHELL_ENABLE_KEYBUBBING_TOP_MODE

        bool activateCompositor = false, propagateKey = true, foundListener = false;
        bool stopPropagation = false;
        bool isFocusedCompositor = true;
        for (; nodeIt != nodes.rend(); nodeIt++)
        {
            auto compositor = (*nodeIt)->compositor();
            
            if (compositor->getInputEventsEnabled())
            {
                continue;
            }

          #ifdef RDKSHELL_ENABLE_KEYBUBBING_TOP_MODE
          if (compositor == gFocusedCompositor) // NOTE: Start from the top, but act on the focused compositor first, then skip it later
          {
              continue;
          }
          isFocusedCompositor = false;
          #endif //RDKSHELL_ENABLE_KEYBUBBING_TOP_MODE
          activateCompositor = false;
          propagateKey = true;
          foundListener = false;

          compositor->evaluateKeyListeners(keycode, flags, foundListener, activateCompositor, propagateKey);

          if (foundListener && !isFocusedCompositor)
          {
            Logger::log(Debug, "Key %d sent to listener %s", keycode, compositor->name().c_str());
            if (isPressed)
            {
              compositor->onKeyPress(keycode, flags, metadata);
              gPendingKeyUpListeners.push_back(compositor);
            }
            else
            {
              compositor->onKeyRelease(keycode, flags, metadata);
            }
          }
          isFocusedCompositor = false;
          if (activateCompositor && gFocusedCompositor != compositor)
          {
                if (gFocusedCompositor->isKeyPressed())
                {
                    gPendingKeyUpListeners.push_back(gFocusedCompositor);
                }
                
                Logger::log(LogLevel::Information,  "rdkshell_focus bubbleKey: the focused client is now %s . previous: %s", compositor->name().c_str(), gFocusedCompositor->name().c_str());
                gFocusedCompositor = compositor;

                if (gRdkShellEventListener)
                {
                    gRdkShellEventListener->onApplicationActivated(compositor->name());
                }
          }

          //propagate is false, stopping here
          if (false == propagateKey)
          {
            break;
          }
        }
    }

    void updateKeyRepeat()
    {
        if (gKeyRepeatConfig.enabled)
        {
            if (gLastKeyPressStartTime > 0.0)
            {
                double currentTime = RdkShell::seconds();
                if (gLastKeyRepeatTime == 0.0)
                {
                    if ((currentTime - gLastKeyPressStartTime) * 1000.0 > gKeyRepeatConfig.initialDelay)
                    {
                        CompositorController::onKeyPress(gLastKeyCode, gLastKeyModifiers, gLastKeyMetadata);
                        gLastKeyRepeatTime = currentTime;
                    }
                }
                else
                {
                    if ((currentTime - gLastKeyRepeatTime) * 1000.0 > gKeyRepeatConfig.repeatInterval)
                    {
                        CompositorController::onKeyPress(gLastKeyCode, gLastKeyModifiers, gLastKeyMetadata);
                        gLastKeyRepeatTime = currentTime;
                    }
                }

            }
        }
    }

    void CompositorController::initialize()
    {
        static bool sCompositorInitialized = false;
        if (sCompositorInitialized)
            return;

        char const *rdkshellPowerKey = getenv("RDKSHELL_POWER_KEY_CODE");

        if (rdkshellPowerKey)
        {
            int keyCode = atoi(rdkshellPowerKey);
            if (keyCode > 0)
            {
                gPowerKeyCode = (uint32_t)keyCode;
            }
        }
        Logger::log(LogLevel::Information,  "the power key is set to %d", gPowerKeyCode);

        char const *rdkshellPowerKeyEnable = getenv("RDKSHELL_ENABLE_POWER_KEY");

        if (rdkshellPowerKeyEnable)
        {
            int powerValue = atoi(rdkshellPowerKeyEnable);
            if (powerValue > 0)
            {
                gPowerKeyEnabled = true;
            }
        }

        Logger::log(LogLevel::Information,  "power key support enabled: %d", gPowerKeyEnabled);
 
        char const *rdkshellFrontPanelButton = getenv("RDKSHELL_FRONT_PANEL_BUTTON_CODE");

        if (rdkshellFrontPanelButton)
        {
            int keyCode = atoi(rdkshellFrontPanelButton);
            if (keyCode > 0)
            {
                gFrontPanelButtonCode = (uint32_t)keyCode;
            }
        }

        Logger::log(LogLevel::Information,  "the front panel code is set to %d", gFrontPanelButtonCode);

        char const *rdkshellPowerKeyReleaseOnlyEnabled = getenv("RDKSHELL_POWER_RELEASE_ONLY");

        if (rdkshellPowerKeyReleaseOnlyEnabled && (strcmp(rdkshellPowerKeyReleaseOnlyEnabled,"1") == 0))
        {
           gRdkShellPowerKeyReleaseOnlyEnabled  = true;
        }

        Logger::log(LogLevel::Information,  "rdkshell power key release only enabled %d", gRdkShellPowerKeyReleaseOnlyEnabled);

        char const *rdkshellKeyIgnore = getenv("RDKSHELL_ENABLE_KEY_IGNORE");
        Logger::log(LogLevel::Information,  "key ignore feature enabled status [%d]", (NULL==rdkshellKeyIgnore));
        if (NULL != rdkshellKeyIgnore)
        {
            int keyIgnoreValue = atoi(rdkshellKeyIgnore);
            Logger::log(LogLevel::Information,  "key ignore feature [%d]", keyIgnoreValue);
            if (keyIgnoreValue > 0)
            {
                gIgnoreKeyInputEnabled = true;
                Logger::log(LogLevel::Information,  "key ignore feature is enabled");
            }
        }

        char const *rdkshellCompositorType = getenv("RDKSHELL_COMPOSITOR_TYPE");

        if (NULL == rdkshellCompositorType)
        {
            Logger::log(LogLevel::Information,  "compositor type is empty, setting to nested by default");
        }
        else
        {
            if (strcmp(rdkshellCompositorType, "surface") == 0)
            {
                gRdkShellCompositorType = SURFACE;
                uint32_t width = 0;
                uint32_t height = 0;
                RdkShell::EssosInstance::instance()->resolution(width, height);
                RdkCompositorSurface::createMainCompositor("rdkshell_display", width, height);
            }
            else if (strcmp(rdkshellCompositorType, "nested") != 0)
            {
                Logger::log(LogLevel::Information,  "invalid compositor type, setting to nested by default ");
            }
        }

        const char* cursorImageName = getenv("RDKSHELL_CURSOR_IMAGE");
        if (cursorImageName == nullptr)
        {
            Logger::log(LogLevel::Information,  "cursor image not set");
        }
        else
        {
            gCursor = std::make_shared<Cursor>(std::string(cursorImageName));
        }

        sCompositorInitialized = true;
    }

    bool CompositorController::moveToFront(const std::string& client)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(LogLevel::Information,  "%s not found and cannot move to front", client.c_str());
            return false;
        }
        
        gScene.moveToFront(node);
        return true;
    }

    bool CompositorController::moveToBack(const std::string& client)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(LogLevel::Information,  "%s not found and cannot be moved to back", client.c_str());
            return false;
        }

        gScene.moveToBack(node);
        return true;
    }

    bool CompositorController::moveBehind(const std::string& client, const std::string& target)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(LogLevel::Information,  "%s not found and cannot be moved behind", client.c_str());
            return false;
        }

        auto targetNode = gScene.find(target);
        if (!node)
        {
            Logger::log(LogLevel::Information,  "%s not found and cannot be moved behind", target.c_str());
            return false;
        }

        gScene.moveBehind(node, targetNode);
        return true;
    }

    bool CompositorController::getFocused(std::string& client)
    {
        client = gFocusedCompositor ? gFocusedCompositor->name() : "";
        return true;
    }

    bool CompositorController::setFocus(const std::string& client)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot focus %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot focus %s, not a compositor", client.c_str());
            return false;
        }
        
        std::string previousFocusedClient = gFocusedCompositor ? gFocusedCompositor->name() : "none";
        Logger::log(LogLevel::Information,  "rdkshell_focus setFocus: the focused client is now %s.  previous: %s", compositor->name().c_str(), previousFocusedClient.c_str());

        if (gFocusedCompositor && gFocusedCompositor->isKeyPressed())
        {
            gPendingKeyUpListeners.push_back(gFocusedCompositor);
        }

        gFocusedCompositor = compositor;
        return true;
    }

    bool CompositorController::kill(const std::string& client)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot kill %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot kill %s, not a compositor", client.c_str());
            return false;
        }


        std::string clientDisplayName = standardizeName(client);
        RdkShell::Animator::instance()->stopAnimation(clientDisplayName);


        // cleanup key intercepts
        std::vector<std::map<uint32_t, std::vector<KeyInterceptInfo>>::iterator> emptyKeyCodeEntries;
        std::map<uint32_t, std::vector<KeyInterceptInfo>>::iterator entry = gKeyInterceptInfoMap.begin();
        while(entry != gKeyInterceptInfoMap.end())
        {
            std::vector<KeyInterceptInfo>& interceptMap = entry->second;
            std::vector<KeyInterceptInfo>::iterator interceptMapEntry=interceptMap.begin();
            while (interceptMapEntry != interceptMap.end())
            {
                if ((*interceptMapEntry).compositor == compositor)
                {
                    interceptMapEntry = interceptMap.erase(interceptMapEntry);
                }
                else
                {
                    interceptMapEntry++;
                }
            }
            if (interceptMap.size() == 0)
            {
                entry = gKeyInterceptInfoMap.erase(entry);
            }
            else
            {
                entry++;
            }
        }

        // TODO
        // cleanup key listeners
        compositor->removeAllKeyListeners();
        compositor->removeAllEventListeners();

        std::cout << "adding " << clientDisplayName << " to the deleted list\n";
        gDeletedCompositors.push_back(compositor);
        gScene.remove(compositor); // NOTE: what about other references?

        if (gFocusedCompositor == compositor)
        {
            // this may be changed to next available compositor
            gFocusedCompositor = nullptr;
            Logger::log(LogLevel::Information,  "rdkshell_focus kill: the focused client has been killed: %s.  there is no focused client.", clientDisplayName.c_str());
        }
        return true;
    }

    bool CompositorController::addKeyIntercept(const std::string& client, const uint32_t& keyCode, const uint32_t& flags)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot add key intercept for %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot add key intercept for %s, not a compositor", client.c_str());
            return false;
        }

        // Logger::log(LogLevel::Information,  "key intercept added " << keyCode << " flags " << flags << std::endl;

        struct KeyInterceptInfo info;
        info.keyCode = keyCode;
        info.flags = flags;
        info.compositor = compositor;

        auto& intercepts = gKeyInterceptInfoMap[keyCode];
        std::string clientDisplayName = standardizeName(client);
        
        bool found = false;
        for (auto& intercept : intercepts)
        {
            if (intercept.flags == flags && compositor->name() == clientDisplayName)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            intercepts.push_back(info);
        }
        
        return true;
    }

    bool CompositorController::removeKeyIntercept(const std::string& client, const uint32_t& keyCode, const uint32_t& flags)
    {
        // TODO
        if (keyCode == RDKSHELL_WILDCARD_KEY_CODE)
        {
            std::string clientDisplayName = standardizeName(client);
            for (std::map<uint32_t, std::vector<KeyInterceptInfo>>::iterator keyInterceptIterator = gKeyInterceptInfoMap.begin(); keyInterceptIterator != gKeyInterceptInfoMap.end(); keyInterceptIterator++)
            {
                std::vector<KeyInterceptInfo>& interceptInfo = keyInterceptIterator->second;
                std::vector<KeyInterceptInfo>::iterator interceptInfoIterator = interceptInfo.begin();
                while(interceptInfoIterator != interceptInfo.end())
                {
                    if ((*interceptInfoIterator).compositor->name() == clientDisplayName)
                    {
                         interceptInfoIterator = interceptInfo.erase(interceptInfoIterator);
                    }
                    else
                    {
                        interceptInfoIterator++;
                    }
                }
            }
        }
        if (client == "*")
        {
            std::vector<std::vector<KeyInterceptInfo>::iterator> keyMapEntries;
            std::map<uint32_t, std::vector<KeyInterceptInfo>>::iterator it = gKeyInterceptInfoMap.find(keyCode);
            if (it != gKeyInterceptInfoMap.end())
            {
              std::vector<KeyInterceptInfo>::iterator entry = gKeyInterceptInfoMap[keyCode].begin();
              while(entry != gKeyInterceptInfoMap[keyCode].end())
              {
                  if ((*entry).flags == flags)
                  {
                    if (entry->compositor->isKeyPressed())
                    {
                        gPendingKeyUpListeners.push_back(entry->compositor);
                    }
                    entry = gKeyInterceptInfoMap[keyCode].erase(entry);
                  }
                  else
                  {
                    entry++;
                  }
              }
              if ( gKeyInterceptInfoMap[keyCode].size() == 0)
              {
                 gKeyInterceptInfoMap.erase(keyCode);
              }
            }
            return true;
        }

        std::string clientDisplayName = standardizeName(client);

        auto node = gScene.find(clientDisplayName);
        if (!node)
        {
            return false; // TODO: logging
        }


        if (gKeyInterceptInfoMap.end() != gKeyInterceptInfoMap.find(keyCode))
        {
            bool isEntryAvailable = false;
            std::vector<KeyInterceptInfo>::iterator entryPos = gKeyInterceptInfoMap[keyCode].end();
            for (std::vector<KeyInterceptInfo>::iterator it = gKeyInterceptInfoMap[keyCode].begin() ; it != gKeyInterceptInfoMap[keyCode].end(); ++it)
            {
                if (it->flags == flags && it->compositor->name() == clientDisplayName)
                {
                    entryPos = it;
                    isEntryAvailable = true;
                    break;
                }
            }
            if (true == isEntryAvailable)
            {
                if (entryPos->compositor->isKeyPressed())
                {
                    gPendingKeyUpListeners.push_back(entryPos->compositor);
                }
                gKeyInterceptInfoMap[keyCode].erase(entryPos);
                if (gKeyInterceptInfoMap[keyCode].size() == 0)
                {
                    gKeyInterceptInfoMap.erase(keyCode);
                }
            }
        
            return true;
        }

        return false;
    }

    bool CompositorController::addKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags, std::map<std::string, RdkShellData> &listenerProperties)
    {
        // TODO
        bool activate = false, propagate = true;
        for ( const auto &property : listenerProperties)
        {
          if (property.first == "activate")
          {
            activate = property.second.toBoolean();
          }
          else if (property.first == "propagate")
          {
            propagate = property.second.toBoolean();
          }
        }

        Logger::log(LogLevel::Information,  "key listener added client: %s activate: %d propagate: %d RDKShell keyCode: %d flags: %d", client.c_str(), activate, propagate, keyCode, flags);

        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot add key listener for %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot add key listener for %s, not a compositor", client.c_str());
            return false;
        }

        compositor->addKeyListener(keyCode, flags, activate, propagate);
        return true;
    }

    bool CompositorController::addNativeKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags, std::map<std::string, RdkShellData> &listenerProperties)
    {
        uint32_t mappedKeyCode = 0, mappedFlags = 0;
        keyCodeFromWayland(keyCode, flags, mappedKeyCode, mappedFlags);

        Logger::log(LogLevel::Information,  "Native keyCode: %d flags: %d converted to RDKShell keyCode: %d flags: %d", keyCode, flags, mappedKeyCode, mappedFlags);

        return CompositorController::addKeyListener(client, mappedKeyCode, mappedFlags, listenerProperties);
    }

    bool CompositorController::removeKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags)
    {
        // TODO
        Logger::log(LogLevel::Information,  "key listener removed client: %s RDKShell keyCode %d flags %d", client.c_str(), keyCode, flags);

        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot remove key listener for %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot remove key listener for %s, not a compositor", client.c_str());
            return false;
        }

        compositor->removeKeyListener(keyCode, flags);
        return true;
    }

    bool CompositorController::removeNativeKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags)
    {
        uint32_t mappedKeyCode = 0, mappedFlags = 0;
        keyCodeFromWayland(keyCode, flags, mappedKeyCode, mappedFlags);

        Logger::log(LogLevel::Information,  "Native keyCode: %d flags: %d converted to RDKShell keyCode: %d flags: %d", keyCode, flags, mappedKeyCode, mappedFlags);

        return CompositorController::removeKeyListener(client, mappedKeyCode, mappedFlags);
    }

    // NOTE: doesn't update pending keys
    bool CompositorController::removeAllKeyIntercepts()
    {
        for (auto it = gKeyInterceptInfoMap.begin(); it != gKeyInterceptInfoMap.end(); ++it)
        {
            it->second.clear();
        }
        gKeyInterceptInfoMap.clear();
        return true;
    }

    // NOTE: same as above
    bool CompositorController::removeAllKeyListeners()
    {
        auto clients = gScene.clients();

        for (auto& client : clients)
        {
            client->compositor()->removeAllKeyListeners();
        }

        return true;
    }

    bool CompositorController::addKeyMetadataListener(const std::string& client)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot add key metadata listener for %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot add key metadata listener for %s, not a compositor", client.c_str());
            return false;
        }

        compositor->setKeyMetadataEnabled(true);
        return true;
    }

    bool CompositorController::removeKeyMetadataListener(const std::string& client)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot remove key metadata listener for %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot remove key metadata listener for %s, not a compositor", client.c_str());
            return false;
        }

        compositor->setKeyMetadataEnabled(false);
        return true;
    }

    bool CompositorController::injectKey(const uint32_t& keyCode, const uint32_t& flags)
    {
        CompositorController::onKeyPress(keyCode, flags, 0, false);
        CompositorController::onKeyRelease(keyCode, flags, 0, false);
        return true;
    }

    bool CompositorController::generateKey(const std::string& client, const uint32_t& keyCode, const uint32_t& flags, std::string virtualKey)
    {
        bool ret = false;
        uint32_t code = keyCode, modifiers = flags;
        if (!virtualKey.empty())
        {
            bool mappingPresent = keyCodeFromVirtual(virtualKey, code, modifiers);
            if (!mappingPresent)
            {
                std::cout << "virtual key mapping not present for " << virtualKey << std::endl;
                return false;
            }
        }

        if (client.empty())
        {
            CompositorController::onKeyPress(code, modifiers, 0, false);
            CompositorController::onKeyRelease(code, modifiers, 0, false);
            return true;
        }
        
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot generate key for %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot generate key for %s, not a compositor", client.c_str());
            return false;
        }

        compositor->onKeyPress(code, modifiers, 0);
        compositor->onKeyRelease(code, modifiers, 0);        
        return true;
    }

    bool CompositorController::getScreenResolution(uint32_t &width, uint32_t &height)
    {
        RdkShell::EssosInstance::instance()->resolution(width, height);
        return true;
    }

    bool CompositorController::setScreenResolution(const uint32_t width, const uint32_t height)
    {
        RdkShell::EssosInstance::instance()->setResolution(width, height);
        return true;
    }

    bool CompositorController::getClients(std::vector<std::string>& clients)
    {
        clients.clear();
        auto nodes = gScene.clients();
        
        for (auto it = nodes.rbegin(); it != nodes.rend(); it++)
        {
            clients.push_back((*it)->name());
        }

        return true;
    }

    bool CompositorController::getZOrder(std::vector<std::string>& clients)
    {
        return getClients(clients);
    }

    bool CompositorController::getBounds(const std::string& client, uint32_t &x, uint32_t &y, uint32_t &width, uint32_t &height)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot get bounds for %s, client not found", client.c_str());
            return false;
        }

        int32_t _x, _y;
        node->position(_x, _y);
        x = (uint32_t) _x;
        y = (uint32_t) _y;
        
        node->size(width, height);
        return true;
    }
    bool CompositorController::setBounds(const std::string& client, const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot set bounds for %s, client not found", client.c_str());
            return false;
        }

        node->setPosition(x, y);
        node->setSize(width, height);
        return true;
    }

    bool CompositorController::getVisibility(const std::string& client, bool& visible)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot get visibility for %s, client not found", client.c_str());
            return false;
        }

        node->visible(visible);
        return true;
    }

    bool CompositorController::setVisibility(const std::string& client, const bool visible)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot set visibility for %s, client not found", client.c_str());
            return false;
        }

        node->setVisible(visible);
        return true;
    }

    bool CompositorController::getOpacity(const std::string& client, unsigned int& opacity)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot get opacity for %s, client not found", client.c_str());
            return false;
        }

        double o;
        node->opacity(o);

        opacity = (unsigned int)(o * 100);
        opacity = opacity < 0 ? 0 
                : opacity > 100 ? 100 
                : opacity;

        return true;
    }

    bool CompositorController::setOpacity(const std::string& client, const unsigned int opacity)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot set opacity for %s, client not found", client.c_str());
            return false;
        }

        const double o = (double) opacity / 100.0;
        node->setOpacity(o);
        return true;
    }


    bool CompositorController::getScale(const std::string& client, double &scaleX, double &scaleY)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot get scale for %s, client not found", client.c_str());
            return false;
        }

        node->scale(scaleX, scaleY);
        return true;
    }

    bool CompositorController::setScale(const std::string& client, double scaleX, double scaleY)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot set scale for %s, client not found", client.c_str());
            return false;
        }

        node->setScale(scaleX, scaleY);
        return true;
    }

    bool CompositorController::getHolePunch(const std::string& client, bool& holePunch)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot get hole punch for %s, client not found", client.c_str());
            return false;
        }

        node->holePunch(holePunch);
        return true;
    }

    bool CompositorController::setHolePunch(const std::string& client, const bool holePunch)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot set hole punch for %s, client not found", client.c_str());
            return false;
        }

        node->setHolePunch(holePunch);
        Logger::log(LogLevel::Information, "hole punch for %s set to %s", client.c_str(), holePunch ? "true" : "false");
        return true;
    }

    bool CompositorController::scaleToFit(const std::string& client, const int32_t x, const int32_t y, const uint32_t width, const uint32_t height)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot set scale and position for %s, client not found", client.c_str());
            return false;
        }

        uint32_t currentWidth, currentHeight;
        node->size(currentWidth, currentHeight);

        const double scaleX = (double) width / (double) currentWidth;
        const double scaleY = (double) height / (double) currentHeight;
        
        node->setPosition(x, y);
        node->setScale(scaleX, scaleY);
        return true;
    }

    void CompositorController::onKeyPress(uint32_t keycode, uint32_t flags, uint64_t metadata, bool physicalKeyPress)
    {
        //Logger::log(LogLevel::Information,  "key press code " << keycode << " flags " << flags << std::endl;
        double currentTime = RdkShell::seconds();
        if ((true == physicalKeyPress) && (0.0 == gLastKeyPressStartTime))
        {
            gLastKeyPressStartTime = currentTime;
        }
        gLastKeyCode = keycode;
        gLastKeyModifiers = flags;
        gLastKeyMetadata = metadata;
        gLastKeyEventTime = currentTime;
        gNextInactiveEventTime = gLastKeyEventTime + gInactivityIntervalInSeconds;
        gLastKeyRepeatTime = 0.0;

        if ((keycode != 0) && ((keycode == gPowerKeyCode) || ((gFrontPanelButtonCode != 0) && (keycode == gFrontPanelButtonCode))) && (gPowerKeyReleaseReceived == false))
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Debug, "skip power key press");
            return;
        }

        bool isInterceptAvailable = interceptKey(keycode, flags, metadata, true);

        if (!isInterceptAvailable && gFocusedCompositor)
        {
            bubbleKey(keycode, flags, metadata, true);
        }
        else
        {
            std::string focusedClientName = gFocusedCompositor ? gFocusedCompositor->name() : "none";
            Logger::log(LogLevel::Information,  "rdkshell_focus key intercepted: %d focused client: %s", isInterceptAvailable, focusedClientName.c_str());
        }
        if (gRdkShellEventListener && physicalKeyPress)
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Debug, "sending the keyevent for key press");
            gRdkShellEventListener->onKeyEvent(keycode, flags, true);
        }
    }

    void CompositorController::onKeyRelease(uint32_t keycode, uint32_t flags, uint64_t metadata, bool physicalKeyPress)
    {
        //Logger::log(LogLevel::Information,  "key release code " << keycode << " flags " << flags << std::endl;
        if ((false == gRdkShellPowerKeyReleaseOnlyEnabled) && (keycode != 0) && ((keycode == gPowerKeyCode) || ((gFrontPanelButtonCode != 0) && (keycode == gFrontPanelButtonCode))))
        {
            gPowerKeyReleaseReceived = true;
            CompositorController::onKeyPress(keycode, flags, metadata, physicalKeyPress);
            gPowerKeyReleaseReceived = false;
        }

        if (true == physicalKeyPress)
        {
            double keyPressTime = RdkShell::seconds() - gLastKeyPressStartTime;
            checkEasterEggs(keycode, flags, keyPressTime);
            gLastKeyPressStartTime = 0.0;
        }
        gLastKeyCode = keycode;
        gLastKeyModifiers = flags;
        gLastKeyEventTime = RdkShell::seconds();
        gNextInactiveEventTime = gLastKeyEventTime + gInactivityIntervalInSeconds;

        if (!interceptKey(keycode, flags, metadata, false) && gFocusedCompositor)
        {
            bubbleKey(keycode, flags, metadata, false);
        }

        for ( const auto &compositor : gPendingKeyUpListeners )
        {
            compositor->onKeyRelease(keycode, flags, metadata); // NOTE: Why? In some scenarios this gets called three times for a single compositor
        }
        gPendingKeyUpListeners.clear();

        if (gRdkShellEventListener && physicalKeyPress)
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Debug, "sending the keyevent for key release");
            gRdkShellEventListener->onKeyEvent(keycode, flags, false);
        }

        if (keycode != 0 && gPowerKeyEnabled && keycode == gPowerKeyCode)
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Information, "power key pressed");
            if (gRdkShellEventListener)
            {
                RdkShell::Logger::log(RdkShell::LogLevel::Information, "sending the power key event");
                gRdkShellEventListener->onPowerKey();
            }
        }
    }

    void CompositorController::onPointerMotion(uint32_t x, uint32_t y)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Debug, "%s, x: %d, y: %d", __func__, x, y);

        if (gCursor)
        {
            gCursor->setPosition(x, y);
        }

        if (gFocusedCompositor)
        {
            gFocusedCompositor->onPointerMotion(x, y);
        }
    }

    void CompositorController::onPointerButtonPress(uint32_t keyCode, uint32_t x, uint32_t y)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Information, "%s, keycode: %d, x: %d, y: %d", __func__, keyCode, x, y);

        if (gCursor)
        {
            gCursor->setPosition(x, y);
        }

        if (gFocusedCompositor)
        {
            gFocusedCompositor->onPointerButtonPress(keyCode, x, y);
        }
    }

    void CompositorController::onPointerButtonRelease(uint32_t keyCode, uint32_t x, uint32_t y)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Information, "%s, keycode: %d, x: %d, y: %d", __func__, keyCode, x, y);

        if (gCursor)
        {
            gCursor->setPosition(x, y);
        }

        if (gFocusedCompositor)
        {
            gFocusedCompositor->onPointerButtonRelease(keyCode, x, y);
        }
    }

    bool CompositorController::createDisplay(const std::string& client, const std::string& displayName,
        uint32_t displayWidth, uint32_t displayHeight, bool virtualDisplayEnabled, uint32_t virtualWidth, uint32_t virtualHeight,
        bool topmost, bool focus , bool autodestroy)
    {
        // TODO
        Logger::log(LogLevel::Information,
            "rdkshell createDisplay client: %s, displayName: %s, res: %d x %d, virtualDisplayEnabled: %d, virtualRes: %d x %d, topmost: %d, focus: %d\n",
            client.c_str(), displayName.c_str(), displayWidth, displayHeight, virtualDisplayEnabled, virtualWidth, virtualHeight,
            topmost, focus);

        std::string clientDisplayName = standardizeName(client);
        std::string compositorDisplayName = !displayName.empty() ? displayName : clientDisplayName;

        // NOTE: Make sure that client name and display name are unique
        if (gScene.find(client))
        {
            Logger::log(LogLevel::Information,  "display with name %s already exists", client.c_str());
            return false;
        }


        std::shared_ptr<RdkCompositor> compositor;
        
        if (isSurfaceModeEnabled())
        {
            compositor = std::make_shared<RdkCompositorSurface>();
        }
        else
        {
            compositor = std::make_shared<RdkCompositorNested>();
        }

        compositor->setAutoDestroy(autodestroy);

        uint32_t width, height;
        RdkShell::EssosInstance::instance()->resolution(width, height);
        if (displayWidth > 0)
        {
            width = displayWidth;
        }
        if (displayHeight > 0)
        {
            height = displayHeight;
        }

        if (virtualDisplayEnabled)
        {
            if (virtualWidth == 0)
            {
                virtualWidth = width;
            }
            if (virtualHeight == 0)
            {
                virtualHeight = height;
            }
        }

        if (!compositor->createDisplay(compositorDisplayName, clientDisplayName, width, height,
                                       virtualDisplayEnabled, virtualWidth, virtualHeight))
        {
            return false;
        }

        if ((!topmost && gScene.nodes.size() == 0) || (topmost && focus))
        {
            gFocusedCompositor = compositor;
            Logger::log(LogLevel::Information,  "rdkshell_focus create: setting focus of first application created %s", gFocusedCompositor->name().c_str());
        }

        gScene.moveInto(topmost ? gScene.layerTopmost : gScene.layerDefault, compositor);
        return true;
    }

    static void drawWatermarkImages(RdkShellRect rect, bool drawWithRect=true)
    {
        if (!gShowWatermarkImage)
        {
            return;
        }
        std::vector<WatermarkImage>::iterator iter = gWatermarkImages.end();
        for (iter=gWatermarkImages.begin(); iter != gWatermarkImages.end(); iter++)
        {
            if (iter->image != nullptr)
            {		    
                if (drawWithRect)
                {
                    iter->image->draw(rect);
                }
                else
                {
                    iter->image->draw();
                }
	    }
        }
    }

    bool CompositorController::draw()
    {
        // TODO
        //first render deleted compositors to ensure there is no memory leak
        //mfnote: todo - come back and revisit this approach to prevent a memory leak

        for (auto& compositor : gDeletedCompositors)
        {
            RdkShellRect rect;
            bool needsHolePunch = false;
            std::string compositorName = "unknown";
            std::cout << "rendering deleted compositor " << compositorName << std::endl;
            compositor->draw(needsHolePunch, rect); // NOTE: This might cause a brief appearance of the client
        }
        gDeletedCompositors.clear();

        // for (auto reverseIterator = gCompositorList.rbegin(); reverseIterator != gCompositorList.rend(); reverseIterator++)
        // {
        //     bool needsHolePunch = false;
        //     RdkShellRect rect;
        //     reverseIterator->compositor->draw(needsHolePunch, rect);
        //     if (needsHolePunch && !gAlwaysShowWatermarkImageOnTop)
        //     {
        //         drawWatermarkImages(rect, true); // NOTE: handle this in scene
        //     }
        // }

        // for (auto reverseIterator = gTopmostCompositorList.rbegin(); reverseIterator != gTopmostCompositorList.rend(); reverseIterator++)
        // {
        //     bool needsHolePunch = false;
        //     RdkShellRect rect;
        //     reverseIterator->compositor->draw(needsHolePunch, rect);
        // }

        gScene.draw();

        if (gAlwaysShowWatermarkImageOnTop)
        {
            RdkShellRect rect;
            drawWatermarkImages(rect, false);
        }

        if (gShowWatermarkImage && gRdkShellWatermarkImage != nullptr)
        {
            gRdkShellWatermarkImage->draw();
        }

        if (gShowFullScreenImage && gFullScreenImage != nullptr)
        {
            gFullScreenImage->draw();
        }

        if (gCursor)
        {
            gCursor->draw();
        }

        if (gShowSplashImage && gSplashImage != nullptr)
        {
            if (gSplashDisplayTimeInSeconds > 0)
            {
                uint32_t splashShownTime = (uint32_t)(RdkShell::seconds() - gSplashStartTime);
                if (splashShownTime > gSplashDisplayTimeInSeconds)
                {
                    RdkShell::Logger::log(RdkShell::LogLevel::Information, "hiding the splash screen after a timeout: %u", gSplashDisplayTimeInSeconds );
                    hideSplashScreen();
                }
                else
                {
                    gSplashImage->draw();
                }
            }
            else
            {
                gSplashImage->draw();
            }
        }
	return true;
    }

    bool CompositorController::addAnimation(const std::string& client, double duration, std::map<std::string, RdkShellData> &animationProperties)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot add animation for %s, client not found", client.c_str());
            return false;
        }

        int32_t x = 0;
        int32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        double scaleX = 1.0;
        double scaleY = 1.0;
        double opacity = 1.0;
        double delay = 0.0;
        std::string tween = "linear";

        //retrieve the initial values in case they are not specified in the property set
        node->position(x, y);
        node->size(width, height);
        node->scale(scaleX, scaleY);
        node->opacity(opacity);

        for (const auto &property : animationProperties)
        {
            if (property.first == "x")
            {
                x = property.second.toInteger32();
            }
            else if (property.first == "y")
            {
                y = property.second.toInteger32();
            }
            else if (property.first == "w")
            {
                width = property.second.toUnsignedInteger32();
            }
            else if (property.first == "h")
            {
                height = property.second.toUnsignedInteger32();
            }
            else if (property.first == "sx")
            {
                scaleX = property.second.toDouble();
            }
            else if (property.first == "sy")
            {
                scaleY = property.second.toDouble();
            }
            else if (property.first == "a")
            {
                double opacityPercent = property.second.toDouble();
                opacity = opacityPercent / 100.0;
            }
            else if (property.first == "tween")
            {
                tween = property.second.toString();
            }
            else if (property.first == "delay")
            {
                delay = property.second.toDouble();
            }
        }

        RdkShell::Animation animation;
        animation.compositor = node;
        animation.endX = x;
        animation.endY = y;
        animation.endWidth = width;
        animation.endHeight = height;
        animation.endScaleX = scaleX;
        animation.endScaleY = scaleY;
        animation.endOpacity = opacity;
        animation.duration = duration;
        animation.name = client;
        animation.tween = tween;
        animation.delay = delay;
        RdkShell::Animator::instance()->addAnimation(animation);

        return true;
    }

    bool CompositorController::removeAnimation(const std::string& client)
    {
        RdkShell::Animator::instance()->fastForwardAnimation(client);
        return true;
    }

    bool CompositorController::update()
    {
        resolveWaitingEasterEggs();
        RdkShell::Animator::instance()->animate();
        updateKeyRepeat();

        if (gEnableInactivityReporting)
        {
            double currentTime = RdkShell::seconds();
            if (currentTime > gNextInactiveEventTime)
            {
                if (gRdkShellEventListener)
                {
                    gRdkShellEventListener->onUserInactive(getInactivityTimeInMinutes());
                }
              gNextInactiveEventTime = currentTime + gInactivityIntervalInSeconds;
            }
        }
        return true;
    }

    bool CompositorController::addListener(const std::string& client, std::shared_ptr<RdkShellEventListener> listener)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot add event listeners to %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot add event listeners to %s, not a compositor", client.c_str());
            return false;
        }

        compositor->addEventListener(listener);
        return true;
    }

    bool CompositorController::removeListener(const std::string& client, std::shared_ptr<RdkShellEventListener> listener)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot remove event listeners from %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot remove event listeners from %s, not a compositor", client.c_str());
            return false;
        }

        compositor->removeEventListener(listener);
        return true;
    }

    void CompositorController::setEventListener(std::shared_ptr<RdkShellEventListener> listener)
    {
        gRdkShellEventListener = listener;
    }

    void CompositorController::enableInactivityReporting(bool enable)
    {
        gEnableInactivityReporting = enable;
    }

    void CompositorController::setInactivityInterval(double minutes)
    {
        gInactivityIntervalInSeconds = minutes * 60;
        gNextInactiveEventTime = gLastKeyEventTime + gInactivityIntervalInSeconds;
    }

    void CompositorController::resetInactivityTime()
    {
        gLastKeyEventTime = RdkShell::seconds();
        gNextInactiveEventTime = RdkShell::seconds() + gInactivityIntervalInSeconds;
    }

    double CompositorController::getInactivityTimeInMinutes()
    {
        double inactiveTimeInSeconds = RdkShell::seconds() - gLastKeyEventTime;
        return (inactiveTimeInSeconds / 60.0);
    }

    bool CompositorController::launchApplication(const std::string& client, const std::string& uri, const std::string& mimeType,
        bool topmost, bool focus)
    {
        if (mimeType != RDKSHELL_APPLICATION_MIME_TYPE_NATIVE)
        {
            Logger::log(LogLevel::Information,  "unable to launch application.  mime type %d is not supported", mimeType);
            return false;
        }

        if (gScene.find(client))
        {
            Logger::log(LogLevel::Information,  "application with name %s is already launched", client.c_str());
            return false;
        }

        std::string clientDisplayName = standardizeName(client);
        std::shared_ptr<RdkCompositor> compositor;
        
        if (isSurfaceModeEnabled())
        {
            compositor = std::make_shared<RdkCompositorSurface>();
        }
        else
        {
            compositor = std::make_shared<RdkCompositorNested>();
        }

        compositor->setApplication(uri);
        compositor->setMimeType(mimeType);
        
        uint32_t width, height;
        RdkShell::EssosInstance::instance()->resolution(width, height);
        
        if (compositor->createDisplay(clientDisplayName, clientDisplayName, width, height, false, 0, 0))
        {
            if ((!topmost && gScene.nodes.size() == 0) || (topmost && focus))
            {
                gFocusedCompositor = compositor;
                Logger::log(LogLevel::Information,  "rdkshell_focus create: setting focus of first application created %s", gFocusedCompositor->name().c_str());
            }

            gScene.moveInto(topmost ? gScene.layerTopmost : gScene.layerDefault, compositor);
            return true;
        }
  
        return false;
    }

    bool CompositorController::suspendApplication(const std::string& client)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot suspend %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot suspend %s, not a compositor", client.c_str());
            return false;
        }

        bool result = compositor->suspendApplication();
        if (result)
        {
            Logger::log(LogLevel::Information,  "%s client has been suspended", client.c_str());
            node->setVisible(false);
        }
            
        return result;
    }

    bool CompositorController::resumeApplication(const std::string& client)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot resume %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot resume %s, not a compsitor", client.c_str());
            return false;
        }

        bool result = compositor->resumeApplication();
        if (result)
        {
            Logger::log(LogLevel::Information,  "%s client has been resumed", client.c_str());
            node->setVisible(true);
        }

        return result;
    }

    bool CompositorController::closeApplication(const std::string& client)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot close %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot close %s, not a compositor", client.c_str());
            return false;
        }

        compositor->closeApplication();
        return true;
    }

    bool CompositorController::getMimeType(const std::string& client, std::string& mimeType)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot get mimetype of %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot get mimetype of %s, not a compositor", client.c_str());
            return false;
        }

        compositor->setMimeType(mimeType);
        return true;
    }

    bool CompositorController::setMimeType(const std::string& client, const std::string& mimeType)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot set mimetype of %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot set mimetype of %s, not a compositor", client.c_str());
            return false;
        }

        compositor->setMimeType(mimeType);
        return true;
    }

    bool CompositorController::hideWatermark()
    {
        gShowWatermarkImage = false;
        std::vector<WatermarkImage>::iterator iter = gWatermarkImages.end();
        for (iter=gWatermarkImages.begin(); iter != gWatermarkImages.end(); iter++)
        {
            iter->image = nullptr;
        }
        gRdkShellWatermarkImage = nullptr;
        return true;
    }

    bool CompositorController::showWatermark()
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Information, "attempting to display watermark");
        if (nullptr == gRdkShellWatermarkImage)
        {
            const char* waterMarkFile = getenv("RDKSHELL_WATERMARK_IMAGE_PNG");
            if (waterMarkFile)
            {
                gRdkShellWatermarkImage = std::make_shared<RdkShell::Image>();
                bool imageLoaded = gRdkShellWatermarkImage->loadLocalFile(waterMarkFile);
                if (!imageLoaded)
                {
                    RdkShell::Logger::log(RdkShell::LogLevel::Error, "error loading watermark image: %s", waterMarkFile);
                    gRdkShellWatermarkImage = nullptr;
                }
            }
            else
            {
                RdkShell::Logger::log(RdkShell::LogLevel::Warn, "no watermark image specified");
            }
        }
        gShowWatermarkImage = true;
        return true;
    }

    bool CompositorController::hideFullScreenImage()
    {
        gShowFullScreenImage = false;
        gFullScreenImage = nullptr;
        return true;
    }

    bool CompositorController::showFullScreenImage(std::string file)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Information, "attempting to display full screen image");
        if ((nullptr != gFullScreenImage) && (file.compare(gCurrentFullScreenImage) == 0))
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Warn, "image %s is already loaded", file.c_str());
        }
        else
        {
            gFullScreenImage = std::make_shared<RdkShell::Image>();
            bool imageLoaded = gFullScreenImage->loadLocalFile(file);
            if (!imageLoaded)
            {
                RdkShell::Logger::log(RdkShell::LogLevel::Error, "error loading fullscreen image: %s", file);
                gFullScreenImage = nullptr;
                return false;
            }
        }
        gShowFullScreenImage = true;
        gCurrentFullScreenImage = file;
        return true;
    }

    bool CompositorController::hideSplashScreen()
    {
        gShowSplashImage = false;
        gSplashImage = nullptr;
        return true;
    }

    bool CompositorController::showSplashScreen(uint32_t displayTimeInSeconds)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Information, "attempting to display splash image with time: %u", displayTimeInSeconds);
        if (!gShowSplashImage)
        {
            const char* splashFile = getenv("RDKSHELL_SPLASH_IMAGE_JPEG");
            if (splashFile)
            {
                gSplashImage = std::make_shared<RdkShell::Image>();
                gShowSplashImage = gSplashImage->loadLocalFile(splashFile);
                if (!gShowSplashImage)
                {
                    RdkShell::Logger::log(RdkShell::LogLevel::Error, "error loading splash image: %s", splashFile);
                    gSplashImage = nullptr;
                    return false;
                }
            }
            else
            {
                RdkShell::Logger::log(RdkShell::LogLevel::Warn, "no splash image specified");
                return false;
            }
            gSplashDisplayTimeInSeconds = displayTimeInSeconds;
            gSplashStartTime = RdkShell::seconds();
        }
        return true;
    }

    bool CompositorController::setLogLevel(const std::string level)
    {
        Logger::setLogLevel(level.c_str());
        return true;
    }

    bool CompositorController::getLogLevel(std::string& level)
    {
        Logger::logLevel(level);
        return true;
    }

    bool CompositorController::isSurfaceModeEnabled()
    {
        return gRdkShellCompositorType == SURFACE;
    }

    bool CompositorController::sendEvent(const std::string& eventName, std::vector<std::map<std::string, RdkShellData>>& data)
    {
        if (!gRdkShellEventListener)
        {
            Logger::log(LogLevel::Information,  "event listener is not present and unable to send event ", eventName.c_str());
            return false;
        }

        if (eventName.compare(RDKSHELL_EVENT_DEVICE_LOW_RAM_WARNING) == 0)
        {
            int32_t freeKb = -1;
            if (!data.empty())
            {
                freeKb = data[0]["freeKb"].toInteger32();
            }
            gRdkShellEventListener->onDeviceLowRamWarning(freeKb);
        }
        else if (eventName.compare(RDKSHELL_EVENT_DEVICE_CRITICALLY_LOW_RAM_WARNING) == 0)
        {
            int32_t freeKb = -1;
            if (!data.empty())
            {
                freeKb = data[0]["freeKb"].toInteger32();
            }
            gRdkShellEventListener->onDeviceCriticallyLowRamWarning(freeKb);
        }
        else if (eventName.compare(RDKSHELL_EVENT_DEVICE_LOW_RAM_WARNING_CLEARED) == 0)
        {
            int32_t freeKb = -1;
            if (!data.empty())
            {
                freeKb = data[0]["freeKb"].toInteger32();
            }
            gRdkShellEventListener->onDeviceLowRamWarningCleared(freeKb);
        }
        else if (eventName.compare(RDKSHELL_EVENT_DEVICE_CRITICALLY_LOW_RAM_WARNING_CLEARED) == 0)
        {
            int32_t freeKb = -1;
            if (!data.empty())
            {
                freeKb = data[0]["freeKb"].toInteger32();
            }
            gRdkShellEventListener->onDeviceCriticallyLowRamWarningCleared(freeKb);
        }
        else if (eventName.compare(RDKSHELL_EVENT_ANIMATION) == 0)
        {
            if (!data.empty())
            {
              gRdkShellEventListener->onAnimation(data);
            }
        }
        else if (eventName.compare(RDKSHELL_EVENT_EASTER_EGG) == 0)
        {
            std::string name(""), actionJson("");
            if (!data.empty())
            {
                name = data[0]["name"].toString();
                actionJson = data[0]["action"].toString();
            }
            gRdkShellEventListener->onEasterEgg(name, actionJson);
        }
        return true;
    }

    bool CompositorController::enableKeyRepeats(bool enable)
    {
        RdkShell::EssosInstance::instance()->setKeyRepeats(enable);
        return true;
    }

    bool CompositorController::getKeyRepeatsEnabled(bool& enable)
    {
        RdkShell::EssosInstance::instance()->keyRepeats(enable);
        return true;
    }

    bool CompositorController::setTopmost(const std::string& client, bool topmost, bool focus)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot set topmost for %s, client not found", client.c_str());
            return false;
        }

        auto targetLayer = topmost ? gScene.layerTopmost : gScene.layerDefault;
        
        if (node->root() == targetLayer)
        {
            return false; // TODO: logging
        }

        gScene.moveInto(targetLayer, node);

        if (topmost && focus)
        {
            setFocus(client);
        }

        return true;
    }

    bool CompositorController::getTopmost(std::string& client)
    {
        auto clients = gScene.clients();
        client = clients.size() > 0 ? clients.back()->name() : ""; 
        return true;
    }

    bool CompositorController::getVirtualResolution(const std::string& client, uint32_t &virtualWidth, uint32_t &virtualHeight)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            Logger::log(Information, "Cannot get virtual resolution of %s, client not found", client.c_str());
            return false;
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            Logger::log(Information, "Cannot get virtual resolution of %s, not a compositor", client.c_str());
            return false;
        }
        
        compositor->getVirtualResolution(virtualWidth, virtualHeight);
        return true;
    }

    bool CompositorController::setVirtualResolution(const std::string& client, const uint32_t virtualWidth, const uint32_t virtualHeight)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            return false; // TODO: logging
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            return false; // TODO: logging
        }
        
        compositor->setVirtualResolution(virtualWidth, virtualHeight);
        return true;
    }

    bool CompositorController::enableVirtualDisplay(const std::string& client, const bool enable)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            return false; // TODO: logging
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            return false; // TODO: logging
        }
        
        compositor->enableVirtualDisplay(enable);
        return true;
    }

    bool CompositorController::getVirtualDisplayEnabled(const std::string& client, bool &enabled)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            return false; // TODO: logging
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            return false; // TODO: logging
        }
        
        enabled = compositor->getVirtualDisplayEnabled();
        return true;
    }

    bool CompositorController::getLastKeyPress(uint32_t &keyCode, uint32_t &modifiers, uint64_t &timestampInSeconds)
    {
        uint64_t timeSinceLastKeyPress = RdkShell::seconds() - gLastKeyEventTime;
        time_t currentTimeInSeconds = time(0);
        timestampInSeconds = (uint64_t)currentTimeInSeconds - (uint64_t)timeSinceLastKeyPress;
        keyCode = gLastKeyCode;
        modifiers = gLastKeyModifiers;
        return true;
    }


    bool CompositorController::ignoreKeyInputs(bool ignore)
    {
        bool ret = false;
        if (gIgnoreKeyInputEnabled)
        {
            RdkShell::EssosInstance::instance()->ignoreKeyInputs(ignore);
            ret = true;
        }
        else
        {
            Logger::log(LogLevel::Information,  "key inputs ignore feature is not enabled");
        }
        return ret;
    }

    bool CompositorController::updateWatermarkImage(uint32_t imageId, int32_t key, int32_t imageSize)
    {
        bool ret = true;
        key_t sharedMemoryKey = key;
        int shmid;
        char *imageData;
        if ((shmid = shmget(sharedMemoryKey, imageSize, 0644 | IPC_CREAT)) == -1)
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Error, "error accessing image data segment");
            return false;
        }
        imageData = (char*) shmat(shmid, NULL, 0);
        std::vector<WatermarkImage>::iterator iter = gWatermarkImages.end();
        for (iter=gWatermarkImages.begin(); iter != gWatermarkImages.end(); iter++)
        {
            if (iter->id == imageId)
            {
                if (iter->image == nullptr)
                {
                    iter->image = std::make_shared<RdkShell::Image>();
                }
                iter->image->loadImageData(imageData, imageSize);
                break;
            }
        }
        if (iter == gWatermarkImages.end())
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Error, "watermark with image id %d not created already", imageId);
            ret = false;
        }
        if (shmdt(imageData) == -1)
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Error, "error detaching image data segment");
            return false;
        }
        return ret;
    }

    static bool insertWatermarkImage(WatermarkImage& image)
    {
        if (gWatermarkImages.size() == 0)
	{
            gWatermarkImages.insert(gWatermarkImages.begin(), image); 
            return true;
	}	
        std::vector<WatermarkImage>::iterator iter = gWatermarkImages.end();
        for (iter=gWatermarkImages.begin(); iter != gWatermarkImages.end(); iter++)
        {
            if (iter->zorder > image.zorder)
            {
                break;
            }
        }
        if (iter == gWatermarkImages.end()) 
        {
            gWatermarkImages.push_back(image);
        }
        else
	{	
            gWatermarkImages.insert(iter, image);
        }
        return true;
    }

    bool CompositorController::createWatermarkImage(uint32_t imageId, uint32_t zorder)
    {
        std::vector<WatermarkImage>::iterator iter = gWatermarkImages.end();
        for (iter=gWatermarkImages.begin(); iter != gWatermarkImages.end(); iter++)
        {
            if (iter->id == imageId)
            {
                RdkShell::Logger::log(RdkShell::LogLevel::Error, "watermark with image id %d created already", imageId);
                return false;
            }
        }
        WatermarkImage image(imageId, zorder);
        bool ret = insertWatermarkImage(image);
        RdkShell::Logger::log(RdkShell::LogLevel::Debug, "watermark with image id %d created", imageId);
        return ret;
    }

    bool CompositorController::deleteWatermarkImage(uint32_t imageId)
    {
        bool ret =false;
        std::vector<WatermarkImage>::iterator iter = gWatermarkImages.end();
        for (iter=gWatermarkImages.begin(); iter != gWatermarkImages.end(); iter++)
        {
            if (iter->id == imageId)
            {
                break;
            }
        }
        if (iter != gWatermarkImages.end())
        {
            iter->image == nullptr;
            gWatermarkImages.erase(iter);
            ret = true;
        }
        else
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Error, "watermark with image id %d is not present", imageId);
        }
        return ret;
    }

    bool CompositorController::adjustWatermarkImage(uint32_t imageId, uint32_t zorder)
    {
        std::vector<WatermarkImage>::iterator iter = gWatermarkImages.end();
        for (iter=gWatermarkImages.begin(); iter != gWatermarkImages.end(); iter++)
        {
            if (iter->id == imageId)
            {
                break;
            }
        }
        if (iter == gWatermarkImages.end())
        {
            return false;
        }
        WatermarkImage image(imageId, zorder);
        image.image = iter->image;
        gWatermarkImages.erase(iter);
        insertWatermarkImage(image);
        return true;
    }

    bool CompositorController::alwaysShowWatermarkImageOnTop(bool show)
    {
        gAlwaysShowWatermarkImageOnTop = show;
        return true;
    }

    bool CompositorController::screenShot(uint8_t* &data, uint32_t &size)
    {
        uint32_t width, height;
        RdkShell::EssosInstance::instance()->resolution(width, height);
        size = 4 * width * height;
        data = (uint8_t *)malloc(size);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
        return true;
    }

    bool CompositorController::enableInputEvents(const std::string& client, bool enable)
    {
        auto node = gScene.find(client);
        if (!node)
        {
            return false; // TODO: logging
        }

        auto compositor = node->compositor();
        if (!compositor)
        {
            return false; // TODO: logging
        }

        compositor->enableInputEvents(enable);
        return true;
    }

    bool CompositorController::showCursor()
    {
        if (gCursor)
        {
            gCursor->show();
            return true;
        }
        else
        {
            return false;
        }
    }

    bool CompositorController::hideCursor()
    {
        if (gCursor)
        {
            gCursor->hide();
            return true;
        }
        else
        {
            return false;
        }
    }

    bool CompositorController::setCursorSize(uint32_t width, uint32_t height)
    {
        if (gCursor)
        {
            gCursor->setSize(width, height);
            return true;
        }
        else
        {
            return false;
        }
    }

    bool CompositorController::getCursorSize(uint32_t& width, uint32_t& height)
    {
        if (gCursor)
        {
            gCursor->getSize(width, height);
            return true;
        }
        else
        {
            return false;
        }
    }

    void CompositorController::setKeyRepeatConfig(bool enabled, int32_t initialDelay, int32_t repeatInterval)
    {
        gKeyRepeatConfig.enabled = enabled;
        gKeyRepeatConfig.initialDelay = initialDelay;
        gKeyRepeatConfig.repeatInterval = repeatInterval;

        Logger::log(LogLevel::Information, "setKeyRepeatConfig enabled: %d, initialDelay: %d, repeatInterval: %d",
            enabled, initialDelay, repeatInterval);
    }

    static void addWatermarks(bool needsHolePunch, RdkShellRect rect)
    {
        if (needsHolePunch && !gAlwaysShowWatermarkImageOnTop)
        {
            drawWatermarkImages(rect, true);
        }
    }

    bool CompositorController::createGroup(const std::string& groupName, const std::vector<std::string>& clients)
    {
        if (gScene.find(groupName))
        {
            return false; // TODO: logging
        }

        std::vector<std::shared_ptr<Node>> clientNodes;
        for (auto& client : clients)
        {
            auto clientNode = gScene.find(client);
            if (!clientNode)
            {
                return false; // TODO: logging
            }

            clientNodes.push_back(clientNode);
        }

        auto group = std::make_shared<Group>(standardizeName(groupName), &addWatermarks);
        gScene.moveInto(gScene.layerDefault, group);
        for (auto& node : clientNodes)
        {
            gScene.moveInto(group, node);
        }

        return true;
    }

    bool CompositorController::getGroups(std::map<std::string, std::vector<std::string>>& groups)
    {
        groups.clear();

        auto nodeList = gScene.groups();
        for (auto nodeIt = nodeList.rbegin(); nodeIt != nodeList.rend(); ++nodeIt)
        {
            auto group = (*nodeIt)->group();
            auto& children = group->children();

            auto& clients = groups[group->name()];
            for (auto childIt = children.rbegin(); childIt != children.rend(); ++childIt)
            {
                auto& child = *childIt;
                clients.push_back(child->name());
            }
        }

        return true;
    }

    bool CompositorController::deleteGroup(const std::string& groupName)
    {
        auto node = gScene.find(groupName);
        if (!node)
        {
            return false; // TODO: logging
        }

        auto group = node->group();
        if (!group)
        {
            return false; // TODO: logging
        }

        gScene.remove(group);
        return true;
    }

    bool CompositorController::setGroup(const std::string& groupName, const std::vector<std::string>& clients)
    {
        auto node = gScene.find(groupName);
        if (!node)
        {
            return false; // TODO: logging
        }

        auto group = node->group();
        if (!group)
        {
            return false; // TODO: logging
        }

        std::vector<std::shared_ptr<Node>> clientNodes;
        for (auto& client : clients)
        {
            auto clientNode = gScene.find(client);
            if (!clientNode)
            {
                return false; // TODO: logging
            }

            clientNodes.push_back(clientNode);
        }

        for (auto& client : clientNodes)
        {
            gScene.moveInto(group, client);
        }
        return true;
    }

    bool CompositorController::removeFromGroup(const std::string& groupName, const std::vector<std::string>& clients)
    {
        auto node = gScene.find(groupName);
        if (!node)
        {
            return false; // TODO: logging
        }

        auto group = node->group();
        if (!group)
        {
            return false; // TODO: logging
        }

        std::vector<std::shared_ptr<Node>> clientNodes;
        for (auto& client : clients)
        {
            auto clientNode = gScene.find(client);
            if (!clientNode)
            {
                return false; // TODO: logging
            }

            clientNodes.push_back(clientNode);
        }

        auto root = group->root();
        for (auto& client : clientNodes)
        {
            gScene.moveInto(root, client);
        }

        return true;
    }
}
