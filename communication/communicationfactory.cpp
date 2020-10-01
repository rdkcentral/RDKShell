#include "communicationfactory.h"
#include "sockethandler.h"

namespace RdkShell
{
    #define RDKSHELL_SERVER_ADDRESS "localhost"
    #define RDKSHELL_SERVER_PORT 9996
  
    CommunicationHandler* createCommunicationHandler(bool isServer)
    {
        std::string address = RDKSHELL_SERVER_ADDRESS;
        int port = RDKSHELL_SERVER_PORT;
        char const *addressEnvironmentValue = getenv("RDKSHELL_SERVER_ADDRESS");
        if (addressEnvironmentValue)
        {
            address = addressEnvironmentValue;
        }
        char const *portEnvironmentValue = getenv("RDKSHELL_SERVER_PORT");
        if (portEnvironmentValue)
        {
            port = atoi(portEnvironmentValue);
        }
        return new SocketHandler(address, port, isServer);
    }
}
