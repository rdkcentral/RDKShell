#!/bin/sh

# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

validate()
{
  api=$1  
  expectedResult=$2
  curlOutput=$3    
  resultParam=`echo $curlOutput|awk -F'"result\":' '{printf $2}'|sed "s/[{}]//g"|awk -F',' '{print $0}'`
                                                                                                        
  if [ "$resultParam" = "$expectedResult" ]; then     
    echo "$api : Success"                                                                               
    return 0                                      
  else                                            
    echo "$api : Failed - [$resultParam]"
    return 1                             
  fi                                     
}

#executes command and validates the output
testAPI()
{
  apiname=$1
  inputparams=$2
  expectedResult=$3
                   
  params=`echo $inputparams`
  curl_command="curl --silent -d '$params' http://127.0.0.1:9998/jsonrpc"
  curlOutput=`eval $curl_command`
  validate $apiname $expectedResult "$curlOutput"
}

#######################api before client creation 
inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setScreenResolution","params":{"w":600,"h":400}}'
expectedResult='"success":true'                                          
testAPI "setScreenResolution" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getScreenResolution"}'
expectedResult='"w":600,"h":720,"success":true'
testAPI "getScreenResolution" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getClients"}'
expectedResult='"clients":[],"success":true'
testAPI "getClients" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getZOrder"}'
expectedResult='"clients":[],"success":true'
testAPI "getZOrder" "$inputParams" "$expectedResult"

#######################api after client creation 

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.createDisplay","params":{"client":"testapp"}}'
expectedResult='"success":true'
testAPI "createDisplayApp1" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.createDisplay","params":{"client":"testapp1"}}'
expectedResult='"success":true'
testAPI "createDisplayApp2" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.moveToFront","params":{"client":"testapp"}}'
expectedResult='"success":true'
testAPI "moveToFrontSuccess" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.moveToBack","params":{"client":"testapp"}}'
expectedResult='"success":true'
testAPI "moveToBack" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.moveBehind","params":{"client":"testapp","target":"testapp1"}}'
expectedResult='"success":true'
testAPI "moveBehind" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getClients"}'
expectedResult='"clients":["testapp1","testapp"],"success":true'
testAPI "getClients" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getZOrder"}'
expectedResult='"clients":["testapp1","testapp"],"success":true'
testAPI "getZOrder" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.addKeyIntercept","params":{"client":"testapp","keyCode":"48","modifiers":["ctrl", "shift"]}}'
expectedResult='"success":true'
testAPI "addKeyIntercept" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.removeKeyIntercept","params":{"client":"testapp","keyCode":"48","modifiers":["ctrl", "shift"]}}'
expectedResult='"success":true'
testAPI "removeKeyIntercept" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setFocus","params":{"client":"testapp"}}'
expectedResult='"success":true'
testAPI "setFocus" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setVisibility","params":{"client":"testapp","visible":true}}'
expectedResult='"success":true'
testAPI "setVisibility" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getVisibility","params":{"client":"testapp"}}'
expectedResult='"visible":true,"success":true'
testAPI "getVisibility" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setBounds","params":{"client":"testapp","x":0,"y":0,"w":600,"h":600}}'
expectedResult='"success":true'
testAPI "setBounds" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getBounds","params":{"client":"testapp"}}'
expectedResult='"bounds":"x":0,"y":0,"w":600,"h":600,"success":true'
testAPI "getBounds" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setOpacity","params":{"client":"testapp","opacity":100}}'
expectedResult='"success":true'
testAPI "setOpacity" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getOpacity","params":{"client":"testapp"}}'
expectedResult='"opacity":100,"success":true'
testAPI "getOpacity" "$inputParams" "$expectedResult"

