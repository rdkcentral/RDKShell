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

#include "clientmessagehandler.h"
#include "communicationhandler.h"
#include <sstream> 

namespace RdkShell
{
  std::map<std::string, void(*)(const rapidjson::Value& , std::map<std::string, RdkShellData>&)> MessageHandler::mResponseHandler;
  std::map<std::string, void(*)(const rapidjson::Value&, std::vector<std::map<std::string, RdkShellData>>&)> MessageHandler::mEventHandler;

  // message handlers
  static void getScaleHandler(const rapidjson::Value& params, std::map<std::string, RdkShellData>& responseData);
  static void getBoundsHandler(const rapidjson::Value& params, std::map<std::string, RdkShellData>& responseData);

  // event handlers
  static void onAnimationHandler(const rapidjson::Value& params, std::vector<std::map<std::string, RdkShellData>>& eventData);

  struct ParameterInfo
  {
    int index;
    bool mandatory;
    char type;
  };

  static std::map<std::string, std::map<std::string, struct ParameterInfo>> sParameterInfo;
  static std::map<std::string, int> sMandatoryParameterInfo;

  static void populateMandatoryParameterInfo()
  {
    for (std::map<std::string, std::map<std::string, struct ParameterInfo>>::iterator iter = sParameterInfo.begin(); iter != sParameterInfo.end(); iter++)
    {
       int numManadatoryParams = 0;
       for (std::map<std::string, struct ParameterInfo>::iterator paramIter = iter->second.begin(); paramIter != iter->second.end(); paramIter++)
       {
         struct ParameterInfo& info = paramIter->second; 
         if (info.mandatory == true)
           numManadatoryParams++;
       }
       sMandatoryParameterInfo[iter->first] = numManadatoryParams;
    }
  }

  static bool populateParameterValue(char type, RdkShellData& param, std::stringstream& outputstring)
  {
      bool ret = true;
      switch(type)
      {
        case 'b':
          outputstring << std::boolalpha << param.toBoolean();
          break;
        case 'i':
          outputstring << param.toInteger32();
          break;
        case 'u':
          outputstring << param.toUnsignedInteger32();
          break;
        case 's':
          outputstring << "\"" << param.toString() << "\"";
          break;
        case 'f':
          outputstring << param.toDouble();
          break;
        default:
          std::cout << "unable to pass a specific parameter ...." << std::endl;
          ret = false;
          break;
      }
      return ret;
  }

  void MessageHandler::initialize()
  {
    sParameterInfo["createDisplay"] = std::map<std::string, ParameterInfo>();
    sParameterInfo["createDisplay"]["client"] = {0, true, 's'};
    sParameterInfo["createDisplay"]["display"] = {1, false, 's'};

    sParameterInfo["launchApplication"] = std::map<std::string, ParameterInfo>();
    sParameterInfo["launchApplication"]["client"] = {0, true, 's'};
    sParameterInfo["launchApplication"]["uri"] = {1, true, 's'};
    sParameterInfo["launchApplication"]["mime"] = {2, true, 's'};

    sParameterInfo["kill"] = std::map<std::string, ParameterInfo>();
    sParameterInfo["kill"]["client"] = {0, true, 's'};

    sParameterInfo["setScale"] = std::map<std::string, ParameterInfo>();
    sParameterInfo["setScale"]["client"] = {0, true, 's'};
    sParameterInfo["setScale"]["sx"] = {1, false, 'f'};
    sParameterInfo["setScale"]["sy"] = {2, false, 'f'};

    sParameterInfo["getScale"] = std::map<std::string, ParameterInfo>();
    sParameterInfo["getScale"]["client"] = {0, true, 's'};

    sParameterInfo["getBounds"] = std::map<std::string, ParameterInfo>();
    sParameterInfo["getBounds"]["client"] = {0, true, 's'};

    sParameterInfo["setBounds"] = std::map<std::string, ParameterInfo>();
    sParameterInfo["setBounds"]["client"] = {0, true, 's'};
    sParameterInfo["setBounds"]["x"] = {1, false, 'u'};
    sParameterInfo["setBounds"]["y"] = {2, false, 'u'};
    sParameterInfo["setBounds"]["w"] = {3, false, 'u'};
    sParameterInfo["setBounds"]["h"] = {4, false, 'u'};

    sParameterInfo["addAnimation"] = std::map<std::string, ParameterInfo>();
    sParameterInfo["addAnimation"]["client"] = {0, true, 's'};
    sParameterInfo["addAnimation"]["duration"] = {1, true, 'u'};
    sParameterInfo["addAnimation"]["x"] = {2, false, 'u'};
    sParameterInfo["addAnimation"]["y"] = {3, false, 'u'};
    sParameterInfo["addAnimation"]["w"] = {4, false, 'u'};
    sParameterInfo["addAnimation"]["w"] = {5, false, 'u'};
    sParameterInfo["addAnimation"]["sx"] = {6, false, 'f'};
    sParameterInfo["addAnimation"]["sy"] = {7, false, 'f'};
    populateMandatoryParameterInfo();

    mResponseHandler["getBounds"] = getBoundsHandler;
    mResponseHandler["getScale"] = getScaleHandler;
    mEventHandler["onAnimation"] = onAnimationHandler;
  }

  bool MessageHandler::prepareRequestParameters(std::string& method, std::map<std::string, RdkShellData>& params, std::string& paramstring)
  {
    std::stringstream outputstring;
    std::map<std::string , std::map<std::string, ParameterInfo>>::iterator parameteriter = sParameterInfo.find(method);
    if (parameteriter == sParameterInfo.end())
    {
      std::cout << "method " << method << " is not supported " << std::endl;
      return false;
    }
    if (sMandatoryParameterInfo[method] > params.size())
    {
      std::cout << "sufficient number of mandatory parameters not passed for method " << method << std::endl;
      return false;
    }
    int numMandatoryParameters = 0;
    std::map<std::string, ParameterInfo>& info = parameteriter->second;

    bool firstparameter = true;
    outputstring << "{";
    for (std::map<std::string, RdkShellData>::iterator iter = params.begin(); iter != params.end(); iter++)
    {
      std::map<std::string, ParameterInfo>::iterator argsiter = info.find(iter->first);
      if (argsiter != info.end())
      {
        if (!firstparameter)
        {
          outputstring << ",";
        }
        firstparameter = false;
        ParameterInfo& pinfo = argsiter->second;
        outputstring << "\"" << pinfo.index << "\":";
        bool ret = populateParameterValue(pinfo.type, iter->second, outputstring);
        if (false == ret)
        {
          std::cout << "unable to call method " << method << std::endl;
          return false;
        }
        if (pinfo.mandatory == true)
          numMandatoryParameters++;
      }
      else
      {
        std::cout << "parameter " << iter->first << " not applicable for method " << method << std::endl;
      }
    }
    if (numMandatoryParameters < sMandatoryParameterInfo[method])
    {
      std::cout << "Sufficient number of mandatory parameters not passed for method " << method << std::endl;
      return false;
    }
    outputstring << "}";
    paramstring = outputstring.str();
    return true;
  }

  bool MessageHandler::prepareRequest(std::string& name, std::map<std::string, RdkShellData>& params, std::stringstream& output)
  {
    std::string paramstring("");
    bool ret = prepareRequestParameters(name, params, paramstring);
    if (true == ret)
    {
      output << "{\"method\" : \""<<name.c_str()<<"\", \"params\":" << paramstring << "}";
    }
    return ret;
  }

  void MessageHandler::handleResponse(Document& d, std::string& name, bool& isApiError, std::map<std::string, RdkShellData>& responseData)
  {
    if (!d.HasMember("method"))
    {
      return;
    }
    std::string method(d["method"].GetString());
    name = method;
    if (!d.HasMember("params"))
    {
      return;
    }
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("success"))
    {
      isApiError = !(params["success"].GetBool());
    }
    if (isApiError)
    {
      return;  
    }
    if (mResponseHandler.find(method) != mResponseHandler.end())
    {
      mResponseHandler[method](params, responseData);
    }
    else
    {
      std::cout << "received unhandled response message " << method << std::endl;
    }
  }

  void MessageHandler::handleEvent(Document& d, std::string& name, std::vector<std::map<std::string, RdkShellData>>& eventData)
  {
    if (!d.HasMember("name"))
    {
      return;
    }
    std::string event(d["name"].GetString());
    name = event;
    if (mEventHandler.find(event) != mEventHandler.end())
    {
      if (d.HasMember("params"))
      {
        const rapidjson::Value& params = d["params"];
        mEventHandler[event](params, eventData);
      }
    }
    else
    {
      std::cout << "received unhandled event " << name << std::endl;
    }
  }

  MessageType MessageHandler::handleMessage(std::string& response, std::string& name, std::map<std::string, RdkShellData>& responseData, std::vector<std::map<std::string, RdkShellData>>& eventData)
  {
    MessageType ret = NONE;
    std::string type("");
    Document d;
    d.Parse(response.c_str());
    if (!d.HasMember("type"))
    {
      return ret;
    }
    type = d["type"].GetString();
    if (type.compare("response") == 0)
    {
      bool isApiError = false;
      handleResponse(d, name, isApiError, responseData);
      if (isApiError)
      {
        ret = APIERROR;
      }
      else
      {
        ret = RESPONSE;
      }
    }
    else if (type.compare("event") == 0)
    {
      handleEvent(d, name, eventData);
      ret = EVENT;
    }
    return ret;
  }

  static void getBoundsHandler(const rapidjson::Value& params, std::map<std::string, RdkShellData>& responseData)
  {
    responseData["x"] = params["x"].GetUint();
    responseData["y"] = params["y"].GetUint();
    responseData["w"] = params["w"].GetUint();
    responseData["h"] = params["h"].GetUint();
  }
  
  static void getScaleHandler(const rapidjson::Value& params, std::map<std::string, RdkShellData>& responseData)
  {
    responseData["sx"] = params["sx"].GetDouble();
    responseData["sy"] = params["sy"].GetDouble();
  }
  
  static void onAnimationHandler(const rapidjson::Value& params, std::vector<std::map<std::string, RdkShellData>>& eventData)
  {
    if (params.IsArray())
    {
      for (rapidjson::SizeType i = 0; i < params.Size(); i++)
      {
        eventData.push_back(std::map<std::string, RdkShellData>());
        const rapidjson::Value& animationObject = params[i];
        std::map<std::string, RdkShellData>& targetObject = eventData.back();
        targetObject["client"] = animationObject["client"].GetString();
        if (animationObject.HasMember("x"))
        {
          targetObject["x"] = animationObject["x"].GetUint();
        }
        if (animationObject.HasMember("y"))
        {
          targetObject["y"] = animationObject["y"].GetUint();
        }
        if (animationObject.HasMember("w"))
        {
          targetObject["w"] = animationObject["w"].GetUint();
        }
        if (animationObject.HasMember("h"))
        {
          targetObject["h"] = animationObject["h"].GetUint();
        }
        if (animationObject.HasMember("sx"))
        {
          targetObject["sx"] = animationObject["sx"].GetDouble();
        }
        if (animationObject.HasMember("sy"))
        {
          targetObject["sy"] = animationObject["sy"].GetDouble();
        }
      }
    }
  }
}
