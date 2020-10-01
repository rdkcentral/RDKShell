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

#include <rdkshellclient.h>
#include <rdkshelldata.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sstream>
#include <logger.h>

using namespace RdkShell;

void displayResponse(std::map<std::string, RdkShellData>& response)
{
    bool firstparam = true;
    std::map<std::string, RdkShellData>::iterator iter = response.begin();
    std::stringstream responsestring; 
    responsestring << "{";
    while (iter != response.end())
    {
        std::string word = iter->first;
        if (!firstparam)
            responsestring << ",";
        firstparam = false;
        responsestring << word << ":";
        RdkShellData& param = iter->second;
        if (param.dataTypeIndex() == typeid(uint8_t))
        {
            responsestring << param.toUnsignedInteger8();
        }
        else if(param.dataTypeIndex() == typeid(uint32_t))
        {
            responsestring << param.toUnsignedInteger32();
        }
        else if(param.dataTypeIndex() == typeid(uint64_t))
        {
            responsestring << param.toUnsignedInteger64();
        }
        else if(param.dataTypeIndex() == typeid(int8_t))
        {
            responsestring << param.toInteger8();
        }
        else if(param.dataTypeIndex() == typeid(int32_t))
        {
            responsestring << param.toInteger32();
        }
        else if(param.dataTypeIndex() == typeid(int64_t))
        {
            responsestring << param.toInteger64();
        }
        else if(param.dataTypeIndex() == typeid(bool))
        {
            responsestring << param.toBoolean();
        }
        else if(param.dataTypeIndex() == typeid(std::string))
        {
            responsestring << param.toString();
        }
        else if(param.dataTypeIndex() == typeid(float))
        {
            responsestring << param.toFloat();
        }
        else if(param.dataTypeIndex() == typeid(double))
        {
            responsestring << param.toDouble();
        }
        iter++;
    }
    responsestring << "}";
    Logger::log(Information, "[%s]", responsestring.str().c_str());
}

void displayEvent(std::string& name, std::vector<std::map<std::string, RdkShellData>>& eventData)
{
    std::stringstream event;
    Logger::log(Information, "event received ");
    event << " { name : " << name << " - [ ";
    for (int i=0; i<eventData.size(); i++)
    {
        displayResponse(eventData[i]);
    }
    event << " ] ";
    Logger::log(Information, "[%s]", event.str().c_str());
}

class EventListener:public RdkShellEventListener
{
    public:
        void onEvent(std::string name, std::vector<std::map<std::string, RdkShellData>>& eventData)
        {
            displayEvent(name, eventData);
        }
  
        void onApiError(std::string name)
        {
            Logger::log(Information, "onApiError event received %s", name.c_str());
        }
};

int main()
{
    std::string method = "createDisplay";
    EventListener listener;
    RdkShellClient::instance()->initialize();
  
    method = "onAnimation";
    RdkShellClient::instance()->registerForEvent(method, (RdkShellEventListener*)&listener);
    method = "onApiError";
    RdkShellClient::instance()->registerForEvent(method, (RdkShellEventListener*)&listener);
  
    std::map<std::string, RdkShellData> requestParams;
    std::map<std::string, RdkShellData> response;

      // ##################################### TEST 1
    /*
      method = "createDisplay";
      requestParams["client"] = "test_display";
      response.clear();
      bool ret = RdkShellClient::instance()->callMethod(method, requestParams);
      Logger::log(Information, "callmethod for create display result - [%d]", ret);
    */

    // ##################################### TEST 2
    requestParams.clear();
    response.clear();
    method = "launchApplication";
    requestParams["client"] = "test_display";
    requestParams["uri"] = "westeros_test";
    requestParams["mime"] = "application/native";
    bool ret = RdkShellClient::instance()->callMethod(method, requestParams);
    Logger::log(Information, "callmethod for launchApplication result - [%d]", ret);
  
    // ##################################### TEST 3
    requestParams.clear();
    response.clear();
    method = "setBounds";
    requestParams["client"] = "test_display";
    requestParams["x"] = 0;
    requestParams["y"] = 0;
    ret = RdkShellClient::instance()->callMethod(method, requestParams);
    Logger::log(Information, "callmethod for setbounds result - [%d]", ret);
  
    // ##################################### TEST 4
    requestParams.clear();
    response.clear();
    method = "setScale";
    requestParams["client"] = "test_display";
    requestParams["sx"] = 1.5;
    ret = RdkShellClient::instance()->callMethod(method, requestParams);
    Logger::log(Information, "callmethod for setscale result - [%d]", ret);
  
    // ##################################### TEST 5
    requestParams.clear();
    response.clear();
    method = "getScale";
    requestParams["client"] = "test_display";
    ret = RdkShellClient::instance()->callMethodWithResult(method, requestParams, response);
    Logger::log(Information, "callmethod for getscale result - [%d] response size [%d]", ret, response.size());
    displayResponse(response);
  
    // ##################################### TEST 6
    requestParams.clear();
    response.clear();
    method = "getBounds";
    requestParams["client"] = "test_display";
    ret = RdkShellClient::instance()->callMethodWithResult(method, requestParams, response);
    Logger::log(Information, "callmethod for getbounds result - [%d] response size [%d]", ret, response.size());
    displayResponse(response);
     
    // sleeping here to start the app from command line
    //sleep(15);
  
    // ##################################### TEST 7
    method = "addAnimation";
    requestParams.clear();
    response.clear();
    requestParams["client"] = "test_display";
    requestParams["duration"] = 10;
    requestParams["x"] = 10;
    requestParams["y"] = 10;
    requestParams["sx"] = 1.0;
    requestParams["sy"] = 1.0;
    ret = RdkShellClient::instance()->callMethod(method, requestParams);
    Logger::log(Information, "callmethod for add animation result - [%d]", ret);
  
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double startTime = ts.tv_sec + ((double)ts.tv_nsec/1000000000);
    double totalElapsed = 0;
    
    // ################################### LOOK FOR EVENTS
    // perform some animation during this sleep ad check for events
    while (true)
    {
        RdkShellClient::instance()->processMessages();
        usleep(30000);
        clock_gettime(CLOCK_MONOTONIC, &ts);
        totalElapsed = (ts.tv_sec + ((double)ts.tv_nsec/1000000000)) - startTime;
        if (totalElapsed > 30)
            break;
    }
  
    // ##################################### TEST 8
    requestParams.clear();
    response.clear();
    method = "kill";
    requestParams["client"] = "test_display";
    ret = RdkShellClient::instance()->callMethod(method, requestParams);
    Logger::log(Information, "callmethod for kill result - [%d]", ret);
  
    method = "onAnimation";
    RdkShellClient::instance()->unregisterEvent(method, &listener);
    method = "onApiError";
    RdkShellClient::instance()->unregisterEvent(method, &listener);
    return 0;
}
