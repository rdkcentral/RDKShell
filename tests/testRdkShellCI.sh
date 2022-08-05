#!/bin/sh

isThunderSecurityEnabled="0"
command -v tr181 2&>/dev/null
if [ $? -eq 0 ]
then
  echo "checking thunder security"
  THUNDER_SECURITY_ENABLED=`tr181 -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.ThunderSecurity.Enable 2>&1`
  if [ $THUNDER_SECURITY_ENABLED = "true" ]; then
    echo "Thunder security is enabled"
    isThunderSecurityEnabled="1"
  else
    echo "Thunder security is disabled"
  fi
else
  echo "command tr181 not found "
fi

securityToken=""
if [ $isThunderSecurityEnabled = "1" ]; then
  securityToken=`WPEFrameworkSecurityUtility |awk -F\" '{print $4}'`
fi

numberOfSuccess=0
numberOfFailures=0

validate()
{
  api=$1  
  expectedResult=$2
  curlOutput=$3    
  resultParam=`echo $curlOutput|awk -F'"result\":' '{printf $2}'|sed "s/[{}]//g"|awk -F',' '{print $0}'`
                                                                                                        
  if [ "$resultParam" = "$expectedResult" ]; then     
    numberOfSuccess=$((numberOfSuccess+1))
    #echo "$api : Success"                                                                               
    return 0                                      
  else                                            
    numberOfFailures=$((numberOfFailures+1))
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
  curl_command=""
  if [ $isThunderSecurityEnabled = "1" ]
  then
    token=`echo $securityToken`
    curl_command="curl -H \"Content-Type: application/json\"  -H \"Authorization: Bearer $token\" -X POST --silent -d '$params' http://127.0.0.1:9998/jsonrpc" 
  else
    curl_command="curl --silent -d '$params' http://127.0.0.1:9998/jsonrpc"
  fi
  curlOutput=`eval $curl_command`
  validate $apiname $expectedResult "$curlOutput"
}

clientsList=$(curl -sk -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST -d ' {"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKShell.1.getClients", "params": {}}' http://127.0.0.1:9998/jsonrpc)
resultsParam=`echo $clientsList|awk -F'"result\":' '{printf $2}'|sed "s/[{}]//g"|awk -F',' '{print $0}'`

#setLogLevel
inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setLogLevel","params":{"logLevel":"DEBUG"}}'
expectedResult='"logLevel":"DEBUG","success":true'
testAPI "setLogLevel" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getLogLevel","params":{}}'
expectedResult='"logLevel":"DEBUG","success":true'
testAPI "getLogLevel" "$inputParams" "$expectedResult"

#######################api before client creation 
inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setScreenResolution","params":{"w":600,"h":400}}'
expectedResult='"success":true'
testAPI "setScreenResolution" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getScreenResolution"}'
expectedResult='"w":600,"h":400,"success":true'
testAPI "getScreenResolution" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getClients"}'
expectedResult=$resultsParam
testAPI "getClients" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getZOrder"}'
expectedResult=$resultsParam
testAPI "getZOrder" "$inputParams" "$expectedResult"

clientStrip=`echo $clientsList|awk -F'"clients":' '{printf $2}'|sed "s/[{}]//g"|awk -F',"success":' '{print $1}'`
aParam=`echo $clientStrip|awk -F'"subtec_s1",' '{print $2}'|awk -F']' '{print $1}'`
bParam=`echo $clientStrip|awk -F'[' '{printf $2}'|awk -F']' '{print $1}'`
#######################api after client creation 

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.createDisplay","params":{"client":"testapp"}}'
expectedResult='"success":true'
testAPI "createDisplayApp1" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.createDisplay","params":{"client":"testapp1"}}'
expectedResult='"success":true'
testAPI "createDisplayApp2" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.createDisplay","params":{"client":"testapp2"}}'
expectedResult='"success":true'
testAPI "createDisplayApp3" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.moveToFront","params":{"client":"testapp1"}}'
expectedResult='"success":true'
testAPI "moveToFrontSuccess" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.moveToBack","params":{"client":"testapp1"}}'
expectedResult='"success":true'
testAPI "moveToBack" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.moveBehind","params":{"client":"testapp1","target":"testapp2"}}'
expectedResult='"success":true'
testAPI "moveBehind" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getClients"}'
if grep -q "subtec_s1" <<<$clientStrip
then
    expectedResult='"clients":["subtec_s1","testapp2","testapp1","testapp",'${aParam}'],"success":true'
else
    expectedResult='"clients":["testapp2","testapp1","testapp",'${bParam}'],"success":true'
fi
testAPI "getClients" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getZOrder"}'
if grep -q "subtec_s1" <<<$clientStrip
then
    expectedResult='"clients":["subtec_s1","testapp2","testapp1","testapp",'${aParam}'],"success":true'
else
    expectedResult='"clients":["testapp2","testapp1","testapp",'${bParam}'],"success":true'
fi
testAPI "getZOrder" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setFocus","params":{"client":"testapp1"}}'
expectedResult='"success":true'
testAPI "setFocus" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.addKeyIntercept","params":{"client":"testapp2","keyCode":50}}'
expectedResult='"success":true'
testAPI "addKeyIntercept" "$inputParams" "$expectedResult"

#generatekey
inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.generateKey","params":{"keys":[{"keyCode": 50, "delay":"0.0"}]}}'
expectedResult='"success":true'
testAPI "generateKeyAfterAddKeyIntercept" "$inputParams" "$expectedResult"

curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST -d '{"jsonrpc":"2.0", "id":3, "method":"org.rdk.RDKShell.1.getClients", "params":{}}' http://127.0.0.1:9998/jsonrpc
sleep 2
journalctl -a|grep "Key 50 intercepted by client testapp2"

if [ $? -eq 1 ]
then
  echo "add key intercept test failed"
  numberOfSuccess=$((numberOfSuccess-1))
  numberOfFailures=$((numberOfFailures+1))
fi

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.removeKeyIntercept","params":{"client":"testapp2","keyCode":50}}'
expectedResult='"success":true'
testAPI "removeKeyIntercept" "$inputParams" "$expectedResult"

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

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setScale","params":{"client":"testapp","sx":"1.5"}}'
expectedResult='"success":true'
testAPI "setScale" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getScale","params":{"client":"testapp"}}'
expectedResult='"sx":"1.500000","sy":"1.000000","success":true'
testAPI "getScale" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.setHolePunch","params":{"client":"testapp","holePunch":true}}'
expectedResult='"success":true'
testAPI "setHolePunch" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getHolePunch","params":{"client":"testapp"}}'
expectedResult='"holePunch":true,"success":true'
testAPI "getHolePunch" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.addKeyListener","params":{"client":"testapp", "keys":[{"keyCode":48,"propagate":false}]}}'
expectedResult='"success":true'
testAPI "addKeyListener" "$inputParams" "$expectedResult"

#generatekey
inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.generateKey","params":{"keys":[{"keyCode": 48, "delay":"0.0"}]}}'
expectedResult='"success":true'
testAPI "generateKeyAfterAddKeyListener" "$inputParams" "$expectedResult"

curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST -d '{"jsonrpc":"2.0", "id":3, "method":"org.rdk.RDKShell.1.getClients", "params":{}}' http://127.0.0.1:9998/jsonrpc

journalctl -a|grep -i startwpe|grep "Key 48 sent to listener testapp"

if [ $? -eq 1 ]
then
  echo "add key listener test failed"
  numberOfSuccess=$((numberOfSuccess-1))
  numberOfFailures=$((numberOfFailures+1))
fi

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.removeKeyListener","params":{"client":"testapp","keys":[{"keyCode":48}]}}'
expectedResult='"success":true'
testAPI "removeKeyListener" "$inputParams" "$expectedResult"

#add animation
inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.addAnimation","params":{"animations":[{"client": "testapp", "duration":"5.0","w":200, "h":200}]}}'
expectedResult='"success":true'
testAPI "addAnimation" "$inputParams" "$expectedResult"

sleep 6

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getBounds","params":{"client":"testapp"}}'
expectedResult='"bounds":"x":0,"y":0,"w":200,"h":200,"success":true'
testAPI "getBoundsAfterAnimation" "$inputParams" "$expectedResult"

#remove animation
inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.addAnimation","params":{"animations":[{"client": "testapp", "duration":"5.0","w":600, "h":600}]}}'
expectedResult='"success":true'
testAPI "addAnimationBeforeRemoveAnimation" "$inputParams" "$expectedResult"

sleep 1

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.removeAnimation","params":{"client": "testapp"}}'
expectedResult='"success":true'
testAPI "removeAnimation" "$inputParams" "$expectedResult"
inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getBounds","params":{"client":"testapp"}}'
expectedResult='"bounds":"x":0,"y":0,"w":600,"h":600,"success":true'
testAPI "getBoundsAfterRemoveAnimation" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.kill","params":{"client":"testapp"}}'
expectedResult='"success":true'
testAPI "killTestApp" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.kill","params":{"client":"testapp1"}}'
expectedResult='"success":true'
testAPI "killTestApp1" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.kill","params":{"client":"testapp2"}}'
expectedResult='"success":true'
testAPI "killTestApp2" "$inputParams" "$expectedResult"

inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKShell.1.getClients"}'
expectedResult=$resultsParam
testAPI "getClientsAfterKill" "$inputParams" "$expectedResult"

echo "Number of success test - $numberOfSuccess"
echo "Number of failed test - $numberOfFailures"

if [ $numberOfFailures -eq 0 ]
then
  exit 0
else
  exit 1
fi
