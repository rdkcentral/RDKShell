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

#include "rdkshellchannel.h"
#include "communicationhandler.h"
#include "communicationfactory.h"
#include "clientmessagehandler.h"

#include <sstream>
#include <iostream>

#define MESSAGE_RESPONSE_TIMEOUT 3000
#define RDKSHELL_SERVER_ADDRESS "localhost"
#define RDKSHELL_SERVER_PORT 9996

namespace RdkShell
{
    RdkShellChannel* RdkShellChannel::mInstance = NULL;
}

namespace RdkShell
{
    RdkShellChannel::RdkShellChannel() : mCommunicationHandler(NULL), mIsConnectedToServer(false),
           mResponseTimeout(MESSAGE_RESPONSE_TIMEOUT), mEventHandlers(), mMessageId(0)
    {
    }
    
    RdkShellChannel::~RdkShellChannel()
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

    RdkShellChannel* RdkShellChannel::instance()
    {
        if (mInstance == NULL)
        {
            mInstance = new RdkShellChannel();
        }
        return mInstance;
    }

    void RdkShellChannel::initialize()
    {
        MessageHandler::initialize();
        mCommunicationHandler = createCommunicationHandler();
        bool ret = mCommunicationHandler->initialize();
        if (!ret)
        {
          std::cout << "client initialization failed !!" << std::endl;
        }
    }

    bool RdkShellChannel::callMethod(std::string& name, std::map<std::string, RdkShellData>& params)
    {
      std::stringstream message("");
      bool ret = MessageHandler::prepareRequest(name, params, message);
      if (true == ret)
      {
        std::string request = message.str();
        ret = mCommunicationHandler->sendMessage(mMessageId, request);
      }
      return ret;
    }

    bool RdkShellChannel::callMethodWithResult(std::string& name, std::map<std::string, RdkShellData>& params, std::map<std::string, RdkShellData>& result)
    {
      std::stringstream message("");
      bool ret = callMethod(name, params);
      if (true == ret)
      {
        processMessages(mResponseTimeout, result);
      }
      return ret;
    }

    void RdkShellChannel::registerForEvent(std::string& name, RdkShellEventListener* listener)
    {
      if (mEventHandlers.find(name) == mEventHandlers.end())
      {
        mEventHandlers[name] = std::vector<RdkShellEventListener*>();
      }
      bool found = false;
      std::map<std::string, std::vector<RdkShellEventListener*>>::iterator handlersiterator =  mEventHandlers.find(name);
      if (handlersiterator != mEventHandlers.end())
      {
        std::vector<RdkShellEventListener*>& listeners = handlersiterator->second;
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

    void RdkShellChannel::unregisterEvent(std::string& name, RdkShellEventListener* listener)
    {
      if (mEventHandlers.find(name) == mEventHandlers.end())
      {
        return;
      }
      std::map<std::string, std::vector<RdkShellEventListener*>>::iterator handlersiterator =  mEventHandlers.find(name);
      std::vector<RdkShellEventListener*>& listeners = handlersiterator->second;
      std::vector<RdkShellEventListener* >::iterator eraseentry = listeners.end();
      for (std::vector<RdkShellEventListener*>::iterator iter=listeners.begin(); iter != listeners.end(); iter++)
      {
        if (*iter == listener)
        {
          eraseentry = iter;
          break;
        }  
      }
      if (eraseentry != listeners.end())
      {
        listeners.erase(eraseentry);
      }
    }

    void RdkShellChannel::processMessages(int wait)
    {
      std::map<std::string, RdkShellData> responseData;
      processMessages(wait, responseData);
      if (responseData.size() > 0)
      {
        std::cout << "got a delayed response message " <<std::endl;
      }
    }

    void RdkShellChannel::processMessages(int wait, std::map<std::string, RdkShellData>& responseData)
    {
      std::string response("");
      if (mCommunicationHandler->poll(wait, &response))
      {
        std::string name;
        std::vector<std::map<std::string, RdkShellData>> eventData;
        MessageType ret = MessageHandler::handleMessage(response, name, responseData, eventData);
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
        else if (ret == APIERROR)
        {
          if (mEventHandlers.find("onApiError") != mEventHandlers.end())
          {
            std::vector<RdkShellEventListener*>& listeners = mEventHandlers["onApiError"];
            for (std::vector<RdkShellEventListener*>::iterator iter=listeners.begin(); iter != listeners.end(); iter++)
            {
              (*iter)->onApiError(name);
            }
          }
          std::cout << "error response received " << std::endl;
        }
        else if (ret == NONE)
        {
          std::cout << "error in processing message " << name << std::endl; 
        }
      }
    }
}
