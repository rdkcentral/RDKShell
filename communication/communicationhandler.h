#ifndef RDKSHELL_COMMUNICATION_HANDLER_H
#define RDKSHELL_COMMUNICATION_HANDLER_H

#include <string>

namespace RdkShell
{
    class RdkShellClientListener
    {
        public:
            virtual void onMessageReceived(int id, std::string& message) = 0;
    };
  
    class CommunicationHandler
    {
        public:
            virtual bool initialize() = 0;
            virtual void terminate() = 0;
            virtual bool sendMessage(int id, std::string& message) = 0;
            virtual bool process(int wait=0, std::string* message=nullptr) = 0;
            virtual void sendEvent(std::string& event) = 0;
            virtual void setListener(RdkShellClientListener* listener) = 0;
    };
}
#endif //RDKSHELL_COMMUNICATION_HANDLER_H
