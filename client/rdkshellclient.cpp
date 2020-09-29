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

#include "rdkshellclient.h"
#include "communicationhandler.h"
#include "communicationfactory.h"
#include "clientmessagehandler.h"
#include "logger.h"
#include <sstream>
#include <iostream>

#define MESSAGE_RESPONSE_TIMEOUT 3000

namespace RdkShell
{
    RdkShellClient* RdkShellClient::mInstance = NULL;
}

namespace RdkShell
{
    RdkShellClient::RdkShellClient() : mCommunicationHandler(NULL), mIsConnectedToServer(false),
           mResponseTimeout(MESSAGE_RESPONSE_TIMEOUT), mEventHandlers(), mMessageId(0)
    {
    }
    
    RdkShellClient::~RdkShellClient()
    {
        if (NULL != mCommunicationHandler)
        {
            mCommunicationHandler->terminate();
            delete mCommunicationHandler;
            mCommunicationHandler = NULL;
        }
        mIsConnectedToServer = false;
        mEventHandlers.clear();
    }

    RdkShellClient* RdkShellClient::instance()
    {
        if (mInstance == NULL)
        {
            mInstance = new RdkShellClient();
        }
        return mInstance;
    }

    void RdkShellClient::initialize()
    {
        ClientMessageHandler::initialize();
        mCommunicationHandler = createCommunicationHandler();
        bool ret = mCommunicationHandler->initialize();
        if (!ret)
        {
            Logger::log(Fatal, "client initialization failed !!");
        }
    }

    bool RdkShellClient::callMethod(std::string& name, std::map<std::string, RdkShellData>& params)
    {
        std::stringstream message("");
        bool ret = ClientMessageHandler::prepareRequest(name, params, message);
        if (true == ret)
        {
            std::string request = message.str();
            ret = mCommunicationHandler->sendMessage(mMessageId, request);
        }
        return ret;
    }

    bool RdkShellClient::callMethodWithResult(std::string& name, std::map<std::string, RdkShellData>& params, std::map<std::string, RdkShellData>& result)
    {
        std::stringstream message("");
        bool ret = callMethod(name, params);
        if (true == ret)
        {
            processMessages(mResponseTimeout, result);
        }
        return ret;
    }

    void RdkShellClient::registerForEvent(std::string& name, RdkShellEventListener* listener)
    {
        if (mEventHandlers.find(name) == mEventHandlers.end())
        {
            mEventHandlers[name] = std::vector<RdkShellEventListener*>();
        }
        bool found = false;
        std::map<std::string, std::vector<RdkShellEventListener*>>::iterator handlersIterator =  mEventHandlers.find(name);
        if (handlersIterator != mEventHandlers.end())
        {
            std::vector<RdkShellEventListener*>& listeners = handlersIterator->second;
            for (std::vector<RdkShellEventListener*>::iterator iter=listeners.begin(); iter != listeners.end(); iter++)
            {
                if (*iter == listener)
                {
                    found = true;
                    break;
                }
            }
        }
        if (!found)
        {
            mEventHandlers[name].push_back(listener);
        }
    }

    void RdkShellClient::unregisterEvent(std::string& name, RdkShellEventListener* listener)
    {
        if (mEventHandlers.find(name) == mEventHandlers.end())
        {
            return;
        }
        std::map<std::string, std::vector<RdkShellEventListener*>>::iterator handlersIterator =  mEventHandlers.find(name);
        std::vector<RdkShellEventListener*>& listeners = handlersIterator->second;
        std::vector<RdkShellEventListener* >::iterator eraseEntry = listeners.end();
        for (std::vector<RdkShellEventListener*>::iterator iter=listeners.begin(); iter != listeners.end(); iter++)
        {
            if (*iter == listener)
            {
                eraseEntry = iter;
                break;
            }  
        }
        if (eraseEntry != listeners.end())
        {
            listeners.erase(eraseEntry);
        }
    }

    void RdkShellClient::processMessages(int wait)
    {
        std::map<std::string, RdkShellData> responseData;
        processMessages(wait, responseData);
        if (responseData.size() > 0)
        {
            Logger::log(Warn, "got a delayed response message");
        }
    }

    void RdkShellClient::processMessages(int wait, std::map<std::string, RdkShellData>& responseData)
    {
        std::string response("");
        if (mCommunicationHandler->process(wait, &response))
        {
            std::string name;
            std::vector<std::map<std::string, RdkShellData>> eventData;
            MessageType ret = ClientMessageHandler::handleMessage(response, name, responseData, eventData);
            if (ret == EVENT)
            {
                if (mEventHandlers.find(name) != mEventHandlers.end())
                {
                    std::vector<RdkShellEventListener*>& listeners = mEventHandlers[name];
                    for (std::vector<RdkShellEventListener*>::iterator iter=listeners.begin(); iter != listeners.end(); iter++)
                    {
                        (*iter)->onEvent(name, eventData);
                    }
                }
            }
            else if (ret == API_ERROR)
            {
                if (mEventHandlers.find("onApiError") != mEventHandlers.end())
                {
                    std::vector<RdkShellEventListener*>& listeners = mEventHandlers["onApiError"];
                    for (std::vector<RdkShellEventListener*>::iterator iter=listeners.begin(); iter != listeners.end(); iter++)
                    {
                        (*iter)->onApiError(name);
                    }
                }
                Logger::log(Information, "error response received ");
            }
            else if (ret == NONE)
            {
                Logger::log(Warn, "error in processing message [%s] ", name.c_str());
            }
        }
    }
}
