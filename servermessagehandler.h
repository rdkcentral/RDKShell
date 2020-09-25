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

#ifndef RDKSHELL_SERVER_MESSAGE_HANDLER_H
#define RDKSHELL_SERVER_MESSAGE_HANDLER_H

#include "communicationhandler.h"
#include <iostream>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include "rdkshelldata.h"
#include "rdkshellevents.h"
#include "rapidjson/document.h"

using namespace rapidjson;

namespace RdkShell
{
  class ServerMessageHandler: public RdkShellClientListener, public RdkShellEventListener, public std::enable_shared_from_this<ServerMessageHandler>
  {
    public:
      ServerMessageHandler();
      void start();
      void poll();
      void stop();
      /* RdkShellClientListener methods */
      virtual void messageReceived(int id, std::string& message);

      /* RdkShellEventListener methods */
      virtual void onAnimation(std::vector<std::map<std::string, RdkShellData>>& animationData);
      CommunicationHandler* getCommunicationHandler();

    private:
      void initMsgHandlers();
      void sendErrorResponse(int id, std::string& method);
      std::shared_ptr<ServerMessageHandler> getptr();
      std::map<std::string, bool(*)(int, const rapidjson::Value&, void*)> mHandlerMap;
      CommunicationHandler* mCommunicationHandler;
  };
}
#endif  //RDKSHELL_SERVER_MESSAGE_HANDLER_H
