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

#include "servermessagehandler.h"
#include "compositorcontroller.h"
#include "communicationfactory.h"
#include <sstream> 

namespace RdkShell
{
    /* message handlers */
    static bool createDisplayHandler(int id, const rapidjson::Value& params, void* context);
    static bool launchApplicationHandler(int id, const rapidjson::Value& params, void* context);
    static bool killHandler(int id, const rapidjson::Value& params, void* context);
    static bool setBoundsHandler(int id, const rapidjson::Value& params, void* context);
    static bool setScaleHandler(int id, const rapidjson::Value& params, void* context);
    static bool getBoundsHandler(int id, const rapidjson::Value& params, void* context);
    static bool getScaleHandler(int id, const rapidjson::Value& params, void* context);
    static bool addAnimationHandler(int id, const rapidjson::Value& params, void* context);
  
    ServerMessageHandler::ServerMessageHandler(): mHandlerMap(), mCommunicationHandler(NULL)
    {
        mCommunicationHandler = createCommunicationHandler(true);
        mCommunicationHandler->setListener(this);
        initializeMessageHandlers();
    }
  
    void ServerMessageHandler::initializeMessageHandlers()
    {
        mHandlerMap["kill"] = &killHandler;
        mHandlerMap["createDisplay"] = &createDisplayHandler;
        mHandlerMap["launchApplication"] = &launchApplicationHandler;
        mHandlerMap["getBounds"] = getBoundsHandler;
        mHandlerMap["setBounds"] = setBoundsHandler;
        mHandlerMap["getScale"] = getScaleHandler;
        mHandlerMap["setScale"] = setScaleHandler;
        mHandlerMap["addAnimation"] = addAnimationHandler;
    }
  
    void ServerMessageHandler::start()
    {
        CompositorController::setEventListener(shared_from_this());
        mCommunicationHandler->initialize();
    }
  
    void ServerMessageHandler::process()
    {
        mCommunicationHandler->process();
    }
  
    void ServerMessageHandler::stop()
    {
        CompositorController::setEventListener(nullptr);
        mCommunicationHandler->setListener(nullptr);
        mCommunicationHandler->terminate();
    }
  
    void ServerMessageHandler::sendErrorResponse(int id, std::string& method)
    {
        std::stringstream response;
        response << "{\"type\":\"response\", \"method\":\"" << method.c_str() << "\",\"params\":{ \"success\":" << std::boolalpha << false << "}}";
        std::string message(response.str());
        if (mCommunicationHandler)
        {
            mCommunicationHandler->sendMessage(id, message);
        }
    }
  
    void ServerMessageHandler::onMessageReceived(int id, std::string& request)
    {
        Document d;
        d.Parse(request.c_str());
  
        if (d.HasMember("method") && d.HasMember("params"))
        {
            std::string method =  d["method"].GetString();
            const rapidjson::Value& params = d["params"];
            if ((mHandlerMap.find(method) != mHandlerMap.end()))
            {
                bool ret = mHandlerMap[method](id, params, this);
                if (false == ret)
                {
                    sendErrorResponse(id, method);
                }
            }
        }
    }
  
    bool createDisplayHandler(int id, const rapidjson::Value& params, void* context)
    {
        std::string client = params["0"].GetString();
        std::string display = params.HasMember("1")?params["1"].GetString():"";
        return CompositorController::createDisplay(client, display);
    }
    
    bool launchApplicationHandler(int id, const rapidjson::Value& params, void* context)
    {
        std::string client = params["0"].GetString();
        std::string uri = params["1"].GetString();
        std::string mime = params["2"].GetString();
        return CompositorController::launchApplication(client, uri, mime);
    }
  
    bool killHandler(int id, const rapidjson::Value& params, void* context)
    {
        std::string client = params["0"].GetString();
        return CompositorController::kill(client);
    }
  
    bool setBoundsHandler(int id, const rapidjson::Value& params, void* context)
    {
        std::string client = params["0"].GetString();
        unsigned int x=0, y=0, w=0, h=0;
        if (false == CompositorController::getBounds(client, x, y, w, h))
        {
            return false;
        }
        x = params.HasMember("1")?params["1"].GetUint():x;
        y = params.HasMember("2")?params["2"].GetUint():y;
        w = params.HasMember("3")?params["3"].GetUint():w;
        h = params.HasMember("4")?params["4"].GetUint():h;
        return CompositorController::setBounds(client, x, y, w, h);
    }
  
    bool setScaleHandler(int id, const rapidjson::Value& params, void* context)
    {
        std::string client = params["0"].GetString();
        double sx=1.0, sy=1.0;
        if (false == CompositorController::getScale(client, sx, sy))
        {
            return false;
        }
        sx = params.HasMember("1")?params["1"].GetDouble():sx;
        sy = params.HasMember("2")?params["2"].GetDouble():sy;
        return CompositorController::setScale(client, sx, sy);
    }
  
    bool addAnimationHandler(int id, const rapidjson::Value& params, void* context)
    {
        std::string client = params["0"].GetString();
        double duration = params["1"].GetDouble();
        std::map<std::string, RdkShellData> animationProperties;
        if (params.HasMember("2"))
        {
            animationProperties["x"] = params["2"].GetUint();
        }
        if (params.HasMember("3"))
        {
            animationProperties["y"] = params["3"].GetUint();
        }
        if (params.HasMember("4"))
        {
            animationProperties["w"] = params["4"].GetUint();
        }
        if (params.HasMember("5"))
        {
            animationProperties["h"] = params["5"].GetUint();
        }
        if (params.HasMember("6"))
        {
            animationProperties["sx"] = params["6"].GetUint();
        }
        if (params.HasMember("7"))
        {
            animationProperties["sy"] = params["7"].GetDouble();
        }
        return CompositorController::addAnimation(client, duration, animationProperties);
    }
  
    bool getBoundsHandler(int id, const rapidjson::Value& params, void* context)
    {
        std::stringstream response;
        std::string client = params["0"].GetString();
        unsigned int x=0, y=0, w=0, h=0;
        bool ret = CompositorController::getBounds(client, x, y, w, h);
        response << "{\"type\":\"response\", \"method\":\"getBounds\", \"params\":{";
        if (true == ret)
        {
            response << "\"success\":" << std::boolalpha << true << ",\"x\":" << x << ",\"y\":" << y << ",\"w\":" << w << ",\"h\":" << h;
        }
        else
        {
            response << "\"success\":" << std::boolalpha << false;
        }
        response << "}}";
        std::string message(response.str());
        if (NULL != context)
        {
            ((ServerMessageHandler*)context)->communicationHandler()->sendMessage(id,message);
        }
    }
  
    bool getScaleHandler(int id, const rapidjson::Value& params, void* context)
    {
        std::stringstream response;
        std::string client = params["0"].GetString();
        double sx=0, sy=0;
        bool ret = CompositorController::getScale(client, sx, sy);
        response << "{\"type\":\"response\", \"method\":\"getScale\", \"params\":{";
        if (true == ret)
        {
            response << "\"success\":" << std::boolalpha << true << ",\"sx\":" << sx << ",\"sy\":" << sy;
        }
        else
        {
            response << "\"success\":" << std::boolalpha << false;
        }
        response << "}}";
        std::string message(response.str());
        if (NULL != context)
        {
            ((ServerMessageHandler*)context)->communicationHandler()->sendMessage(id,message);
        }
        return true;
    }
    
    void ServerMessageHandler::onAnimation(std::vector<std::map<std::string, RdkShellData>>& animationData)
    {
        std::stringstream response;
        bool isFirstClient= true;
        response << "{\"type\":\"event\", \"name\": \"onAnimation\", \"params\":[";
        for (int i=0; i<animationData.size(); i++)
        {
            std::map<std::string, RdkShellData>& data = animationData[i];
            if (!isFirstClient)
            {
                response << ",";
            }
            isFirstClient= false;
            response << "{";
            bool isFirstProperty = true;
            for ( const auto &property : data )
            {
                if (!isFirstProperty)
                {
                    response << ",";
                }
                isFirstProperty = false; 
                response << "\"" << property.first << "\":";
                if (property.first == "x" || property.first == "y")
                {
                    response << property.second.toInteger32();
                }
                else if (property.first == "w" || property.first == "h")
                {
                    response << property.second.toUnsignedInteger32();
                }
                else if (property.first == "sx" || property.first == "sy")
                {
                    response << property.second.toDouble();
                }
                else if (property.first == "client")
                {
                    response << "\"" << property.second.toString() << "\"";
                }
            }
            response << "}";
        }
        response << "]}";
        std::string message(response.str());
        if (mCommunicationHandler)
        {
            mCommunicationHandler->sendEvent(message);
        }
    }
  
    CommunicationHandler* ServerMessageHandler::communicationHandler()
    {
        return mCommunicationHandler;
    }
}
