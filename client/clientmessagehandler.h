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

#ifndef RDKSHELL_CLIENT_MESSAGE_HANDLER_H
#define RDKSHELL_CLIENT_MESSAGE_HANDLER_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <rapidjson/document.h>
#include <rdkshelldata.h>

using namespace rapidjson;

namespace RdkShell
{
  enum MessageType
  {
    NONE,
    RESPONSE,
    EVENT,
    APIERROR
  };

  class MessageHandler {
    public:
      static void initialize();
      static bool prepareRequestParameters(std::string& method, std::map<std::string, RdkShellData>& params, std::string& paramstring);
      static bool prepareRequest(std::string& name, std::map<std::string, RdkShellData>& params, std::stringstream& output);
      static MessageType handleMessage(std::string& response, std::string& name, std::map<std::string, RdkShellData>& responseData, std::vector<std::map<std::string, RdkShellData>>& eventData);
  
    private:
      static void handleResponse(Document& d, std::string& name, bool& isApiError, std::map<std::string, RdkShellData>& responseData);
      static void handleEvent(Document &d, std::string& name, std::vector<std::map<std::string, RdkShellData>>& eventData);
      static std::map<std::string, void(*)(const rapidjson::Value& , std::map<std::string, RdkShellData>&)> mResponseHandler;
      static std::map<std::string, void(*)(const rapidjson::Value&, std::vector<std::map<std::string, RdkShellData>>&)> mEventHandler;
  };
}
#endif  //RDKSHELL_CLIENT_MESSAGE_HANDLER_H
