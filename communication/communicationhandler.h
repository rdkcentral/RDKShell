#ifndef RDKSHELL_COMMUNICATION_HANDLER_H
#define RDKSHELL_COMMUNICATION_HANDLER_H

#include <string>

namespace RdkShell
{
  class RdkShellClientListener
  {
    public:
      virtual void messageReceived(int id, std::string& message) = 0;
  };

  class CommunicationHandler
  {
    public:
      virtual bool initialize() = 0;
      virtual void terminate() = 0;
      virtual bool sendMessage(int id, std::string& message) = 0;
      virtual bool poll(int wait=0, std::string* message=nullptr) = 0;
      virtual void broadcast(std::string& message) = 0;
      virtual void setListener(RdkShellClientListener* listener) = 0;
  };
}
#endif //RDKSHELL_COMMUNICATION_HANDLER_H
