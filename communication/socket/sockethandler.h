#ifndef RDKSHELL_SOCKET_HANDLER_H
#define RDKSHELL_SOCKET_HANDLER_H

#include <communicationhandler.h>
#include <vector>
#include <string>
#include <sys/socket.h>

namespace RdkShell
{
    struct Socket
    {
        int fd;
        struct sockaddr_storage endpoint;
    };
  
    class SocketHandler:public CommunicationHandler
    {
        public:
            SocketHandler(std::string& server, int port, bool isServer=false);
            virtual ~SocketHandler();
            bool initialize();
            void terminate();
            bool sendMessage(int id, std::string& message);
            bool process(int wait, std::string* message = NULL);
            void sendEvent(std::string& event);
            void setListener(RdkShellClientListener* listener);
      
        private:
            void acceptClient();
            bool sendToNetwork(int messageId, int fd, std::string& message);
            bool readData(int fd, int& id, std::string* message);
            void removeInactiveClients();
            bool mActive;
            int mFd;
            std::string mServer;
            int mPort;
            char* mBuffer;
            struct sockaddr_storage mLocalEndpoint;
            struct sockaddr_storage mRemoteEndpoint;
            bool mIsServer;
            std::vector<Socket> mClients;
            RdkShellClientListener* mListener;
    };
}
#endif //RDKSHELL_SOCKET_HANDLER_H
