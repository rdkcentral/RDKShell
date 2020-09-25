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
    char const *addressenv = getenv("RDKSHELL_SERVER_ADDRESS");
    if (addressenv)
    {
      address = addressenv;
    }
    char const *portenv = getenv("RDKSHELL_SERVER_PORT");
    if (portenv)
    {
        port = atoi(portenv);
    }
    return new SocketHandler(address, port, isServer);
  }
}
