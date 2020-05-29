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

#include "messageHandler.h"
#include "compositorcontroller.h"
#include "linuxkeys.h"
#include <sstream> 
#include <iostream>
#include <vector>
#include <string>

namespace RdkShell
{

uint32_t getKeyFlag(std::string modifier)
{
  uint32_t flag = 0;
  if (0 == modifier.compare("ctrl"))
  {
    flag = RDKSHELL_FLAGS_CONTROL;
  }
  else if (0 == modifier.compare("shift"))
  {
    flag = RDKSHELL_FLAGS_SHIFT;
  }
  else if (0 == modifier.compare("alt"))
  {
    flag = RDKSHELL_FLAGS_ALT;
  }
  return flag;
}

static std::map<std::string, void(*)(Document&, uWS::WebSocket<uWS::SERVER> *)> mHandlerMap = {};

void notifyClient(uWS::WebSocket<uWS::SERVER> *ws, char* message, size_t length, uWS::OpCode opCode) {
  ws->send(message, length, opCode);
}

void sendResponse(uWS::WebSocket<uWS::SERVER> *ws, bool ret) {
  std::stringstream str("");
  str<<"{\"params\":{";
  str<<"\"success\":"<<std::boolalpha<<ret;
  str<<"}";
  str<<"}";
  notifyClient(ws, (char*)str.str().c_str(), str.str().length(), uWS::OpCode::TEXT);
}

void moveToFrontHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
  }
  bool ret = CompositorController::moveToFront(client);
  sendResponse(ws, ret);
}

void moveToBackHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
  }
  bool ret = CompositorController::moveToBack(client);
  sendResponse(ws, ret);
}

void moveBehindHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");
  std::string target("");

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
    if (params.HasMember("target")) {
      target = params["target"].GetString();
    }
  }
  bool ret = CompositorController::moveBehind(client, target);
  sendResponse(ws, ret);
}

void setFocusHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
  }
  bool ret = CompositorController::setFocus(client);
  sendResponse(ws, ret);
}

void killHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
  }
  bool ret = CompositorController::kill(client);
  sendResponse(ws, ret);
}

void addKeyInterceptHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");
  uint32_t keyCode = 0;
  uint32_t flags = 0;

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
    if (params.HasMember("keyCode")) {
      keyCode = params["keyCode"].GetUint();
    }
    if (params.HasMember("modifiers")) {
      const rapidjson::Value& modifiersParam = params["modifiers"];
      if (modifiersParam.IsArray()) {
        for (rapidjson::SizeType i = 0; i < modifiersParam.Size(); i++)
        {
          if (modifiersParam[i].IsString()) {
            flags |= getKeyFlag(modifiersParam[i].GetString());
          }
        }
      }
    }
  }

  bool ret = CompositorController::addKeyIntercept(client, keyCode, flags);
  sendResponse(ws, ret);
}

void removeKeyInterceptHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");
  uint32_t keyCode = 0;
  uint32_t flags = 0;

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
    if (params.HasMember("keyCode")) {
      keyCode = params["keyCode"].GetUint();
    }
    if (params.HasMember("modifiers")) {
      const rapidjson::Value& modifiersParam = params["modifiers"];
      if (modifiersParam.IsArray()) {
        for (rapidjson::SizeType i = 0; i < modifiersParam.Size(); i++)
        {
          if (modifiersParam[i].IsString()) {
            flags |= getKeyFlag(modifiersParam[i].GetString());
          }
        }
      }
    }
  }

  bool ret = CompositorController::removeKeyIntercept(client, keyCode, flags);
  sendResponse(ws, ret);
}

void getScreenResolutionHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  unsigned int width = 0;
  unsigned int height = 0;

  CompositorController::getScreenResolution(width, height);
  std::stringstream str("");
  str<<"{\"params\":{";
  str<<"\"width\":"<<width;
  str<<",\"height\":"<<height;
  str<<"}";
  str<<"}";
  notifyClient(ws, (char*)str.str().c_str(), str.str().length(), uWS::OpCode::TEXT);
}

void setScreenResolutionHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");
  unsigned int w=0, h=0;

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("w")) {
      w = params["w"].GetUint();
    }
    if (params.HasMember("h")) {
      h = params["h"].GetUint();
    }
  }
  bool ret = CompositorController::setScreenResolution(w, h);
  sendResponse(ws, ret);
}

void createDisplayHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");
  std::string displayName("");

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
    if (params.HasMember("displayName"))
    {
      displayName = params["displayName"].GetString();
    }
  }
  bool ret = CompositorController::createDisplay(client, displayName);
  sendResponse(ws, ret);
}

void getClientsHandler(Document& d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::vector<std::string> clients;
  CompositorController::getClients(clients);

  std::stringstream str("");
  str<<"{\"params\":[";
  for (size_t i=0; i<clients.size(); i++) {
    str<<"\""<<clients[i]<<"\"";
    if (i != clients.size()-1)
      str<<",";
  }
  str<<"]";
  str<<"}";
  notifyClient(ws, (char*)str.str().c_str(), str.str().length(), uWS::OpCode::TEXT);
}

void getZOrderHandler(Document &d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::vector<std::string> clients;
  CompositorController::getZOrder(clients);

  std::stringstream str("");
  str<<"{\"params\":[";
  for (size_t i=0; i<clients.size(); i++) {
    str<<"\""<<clients[i]<<"\"";
    if (i != clients.size()-1)
      str<<",";
  }
  str<<"]";
  str<<"}";
  notifyClient(ws, (char*)str.str().c_str(), str.str().length(), uWS::OpCode::TEXT);
}

void getBoundsHandler(Document &d, uWS::WebSocket<uWS::SERVER> *ws)
{
  unsigned int x = 0;
  unsigned int y = 0;
  unsigned int width = 0;
  unsigned int height = 0;

  std::string client("");

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
  }

  std::stringstream str("");
  str<<"{\"params\":{";
  if (true == CompositorController::getBounds(client, x, y, width, height)) {
    str<<"\"x\":"<<x;
    str<<",\"y\":"<<y;
    str<<",\"w\":"<<width;
    str<<",\"h\":"<<height;
  }
  str<<"}";
  str<<"}";
  notifyClient(ws, (char*)str.str().c_str(), str.str().length(), uWS::OpCode::TEXT);
}

void setBoundsHandler(Document &d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");
  unsigned int x=0, y=0, w=0, h=0;

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
    if (params.HasMember("x")) {
      x = params["x"].GetUint();
    }
    if (params.HasMember("y")) {
      y = params["y"].GetUint();
    }
    if (params.HasMember("w")) {
      w = params["w"].GetUint();
    }
    if (params.HasMember("h")) {
      h = params["h"].GetUint();
    }
  }
  bool ret = CompositorController::setBounds(client, x, y, w, h);
  sendResponse(ws, ret);
}

void getVisibilityHandler(Document &d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");
  bool visible = true;

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
  }
  std::stringstream str("");
  str<<"{\"params\":{";
  if (true == CompositorController::getVisibility(client, visible)) {
    str<<"\"visible\":"<<std::boolalpha<<visible;
  }
  str<<"}";
  str<<"}";
  notifyClient(ws, (char*)str.str().c_str(), str.str().length(), uWS::OpCode::TEXT);
}

void setVisibilityHandler(Document &d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");
  bool visible = false;

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
    if (params.HasMember("visible")) {
      visible = params["visible"].GetBool();
    }
  }
  bool ret = CompositorController::setVisibility(client, visible);
  sendResponse(ws, ret);
}

void getOpacityHandler(Document &d, uWS::WebSocket<uWS::SERVER> *ws)
{
  std::string client("");
  unsigned int opacity = 0;

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
  }
  std::stringstream str("");
  str<<"{\"params\":{";
  if (true == CompositorController::getOpacity(client, opacity)) {
    str<<"\"opacity\":"<<opacity;
  }
  str<<"}";
  str<<"}";
  notifyClient(ws, (char*)str.str().c_str(), str.str().length(), uWS::OpCode::TEXT);
}

void setOpacityHandler(Document &d, uWS::WebSocket<uWS::SERVER> *ws) {
  std::string client("");
  unsigned int opacity = 0;

  if (d.HasMember("params")) {
    const rapidjson::Value& params = d["params"];
    if (params.HasMember("client")) {
      client = params["client"].GetString();
    }
    if (params.HasMember("opacity")) {
      opacity = params["opacity"].GetUint();
    }
  }
  bool ret = CompositorController::setOpacity(client, opacity);
  sendResponse(ws, ret);
}

bool handleMessage(Document& d, uWS::WebSocket<uWS::SERVER> *ws) {
  if (d.HasMember("msg")) {
    if (mHandlerMap.find(d["msg"].GetString()) != mHandlerMap.end()) {
      mHandlerMap[d["msg"].GetString()](d, ws);
    }
  }
  return true;
}

MessageHandler::MessageHandler(unsigned int port):mHub(), mPort(port) {
  initMsgHandlers();
}

void MessageHandler::start() {
    mHub.onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
        std::string msg = std::string(message, length);
        Document d;
        d.Parse(msg.c_str());
        handleMessage(d, ws);
    });

    mHub.listen(3000);
}

void MessageHandler::poll() {
  mHub.poll();
}

void MessageHandler::stop() {
}

void MessageHandler::initMsgHandlers() {
  mHandlerMap["moveToFront"] = moveToFrontHandler;
  mHandlerMap["moveToBack"] = moveToBackHandler;
  mHandlerMap["moveBehind"] = moveBehindHandler;
  mHandlerMap["setFocus"] = setFocusHandler;
  mHandlerMap["kill"] = killHandler;
  mHandlerMap["addKeyIntercept"] = addKeyInterceptHandler;
  mHandlerMap["removeKeyIntercept"] = removeKeyInterceptHandler;
  mHandlerMap["getScreenResolution"] = getScreenResolutionHandler;
  mHandlerMap["setScreenResolution"] = setScreenResolutionHandler;
  mHandlerMap["createDisplay"] = createDisplayHandler;
  mHandlerMap["getClients"] = getClientsHandler;
  mHandlerMap["getZOrder"] = getZOrderHandler;
  mHandlerMap["getBounds"] = getBoundsHandler;
  mHandlerMap["setBounds"] = setBoundsHandler;
  mHandlerMap["getVisibility"] = getVisibilityHandler;
  mHandlerMap["setVisibility"] = setVisibilityHandler;
  mHandlerMap["getOpacity"] = getOpacityHandler;
  mHandlerMap["setOpacity"] = setOpacityHandler;
}
}
