#include "sockethandler.h"
#include "communicationutils.h"
#include "logger.h"
#include <rapidjson/document.h>
#include <stdio.h>
#include <string.h>
#include <sstream> 
#include <iostream>
#include <vector>
#include <map>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>

using namespace rapidjson;

namespace RdkShell
{
    #define READ_BUFFER_SIZE 2096
  
    struct ClientRequestInformation
    {
        int fd;
        int id;
    };
  
    static std::map<unsigned int, struct ClientRequestInformation> sActiveRequestMap;
    static std::vector<int> sClientsToRemove;
    static unsigned int sMessageId = 0;
  
    SocketHandler::SocketHandler(std::string& server, int port, bool isServer): mFd(-1), mServer(server), mPort(port), mIsServer(isServer), mListener(NULL), mActive(false)
    {
        memset(&mLocalEndpoint, 0, sizeof(struct sockaddr_storage));
        memset(&mRemoteEndpoint, 0, sizeof(struct sockaddr_storage));
        mBuffer = new char[READ_BUFFER_SIZE];
        memset(mBuffer, 0, READ_BUFFER_SIZE);
    }
    
    SocketHandler::~SocketHandler()
    {
        delete mBuffer;
        mBuffer = NULL;
    }
  
    bool SocketHandler::initialize()
    {
        struct hostent *hostEntry = NULL;
        hostEntry = gethostbyname(mServer.c_str());
        if (NULL == hostEntry)
        {
            Logger::log(Fatal, "unable to find address!!!");
            return false;
        }
        mServer = inet_ntoa(*(struct in_addr*)hostEntry->h_addr);
  
        if (mFd != -1)
        {
            close(mFd);
        }
  
        struct sockaddr_in* ipv4Address = (struct sockaddr_in *) (mIsServer?&mLocalEndpoint:&mRemoteEndpoint);
        int ret = inet_pton(AF_INET, mServer.c_str(), &ipv4Address->sin_addr);
        if (1 == ret)
        {
            ipv4Address->sin_family = AF_INET;
            ipv4Address->sin_port = htons(mPort);
        }
        else
        {
            Logger::log(Fatal, "socket initialization failed !!!");
            return false;
        }
        if (mIsServer)
        {
            mLocalEndpoint.ss_family = AF_INET;
            mFd = socket(mLocalEndpoint.ss_family, SOCK_STREAM, 0);
        }
        else
        {
            mRemoteEndpoint.ss_family = AF_INET;
            mFd = socket(mRemoteEndpoint.ss_family, SOCK_STREAM, 0);
        }
  
        if (mFd == -1)
            return false;
  
        uint32_t one = 1;
        ret = fcntl(mFd, F_SETFD, fcntl(mFd, F_GETFD) | FD_CLOEXEC);

        if (ret == -1)
            return false;

        ret = setsockopt(mFd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
        bool isSocketReady = false;
        if (mIsServer)
        {
            ret = setsockopt(mFd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
            ret = bind(mFd, (struct sockaddr *)&mLocalEndpoint, sizeof(struct sockaddr_in));
            if (ret == -1)
            {
                Logger::log(Fatal, "Failed to bind - [%s]", strerror(errno));
                return false;
            }
            ret = listen(mFd, 4);
            if (ret == -1)
            {
                Logger::log(Fatal, "Failed to listen - [%s]", strerror(errno));
                return false;
            }
            isSocketReady = true;
        }
        else
        {
            int retry = 0;
            bool connected=false;
            while (retry <= 2)
            {
                ret = ::connect(mFd, (struct sockaddr *)&mRemoteEndpoint, sizeof(struct sockaddr_in));
                                                                                                 
                if (ret == -1)
                {
                    int err = errno;
                    Logger::log(Error, "Error while connecting to [%s:%d] - [%s]", mServer.c_str(), mPort, strerror(err));
                    sleep(1);
                    retry++;
                }
                else
                {
                    connected = true;
                    break;
                }
            }
            isSocketReady = connected;
        }
        mActive = true;
        return isSocketReady;
    }
  
    void SocketHandler::terminate()
    {
        for (auto client : mClients)
        {
            close(client.fd);
        }
        mClients.clear();
  
        if (mFd != -1)
        {
            shutdown(mFd, SHUT_RDWR);
            close(mFd);
        }
        mFd = -1;
        mActive = false;
    }
    
    bool SocketHandler::sendMessage(int id, std::string& message)
    {
        int fd = mFd, messageId = id;
        if (mIsServer)
        {
            if (sActiveRequestMap.find(id) == sActiveRequestMap.end())
            {
                return false;
            }
            struct ClientRequestInformation& information = sActiveRequestMap[id];
            fd = information.fd;
            messageId = information.id;
            sActiveRequestMap.erase(id);
        }
        return sendToNetwork(messageId, fd, message);
    }
  
    bool SocketHandler::sendToNetwork(int messageId, int fd, std::string& message)
    {
        std::string header;
        prepareHeader(messageId, message, header);
        std::stringstream headerLengthString;
        headerLengthString<<header.length();
        int sent = send(fd, headerLengthString.str().c_str() , 4, MSG_NOSIGNAL);
        sent = send(fd, header.c_str(), header.length(), MSG_NOSIGNAL);
        if (sent != header.length())
        {
            int err = errno;
            Logger::log(Error, "error while sending header - [%s]", strerror(err));
            return false;
        }
  
        sent = send(fd, message.c_str(), message.length(), MSG_NOSIGNAL);
        if (sent != message.length())
        {
            int err = errno;
            Logger::log(Error, "error while sending data - [%s]", strerror(err));
            return false;
        }
        return true;
    }
  
    bool SocketHandler::readData(int fd, int& id, std::string* message)
    {
        ssize_t bytesRead = 0;
        int bytesToRead = 4;
        int byteSent = 0;
        memset(mBuffer, 0, READ_BUFFER_SIZE);
        bytesRead = recv(fd, mBuffer, bytesToRead, MSG_NOSIGNAL);
        if (bytesRead == -1)
        {
            Logger::log(Error, "read error:  [%s]", strerror(errno));
            return false;
        }
        if ((bytesRead == 0) && mIsServer)
        {
            sClientsToRemove.push_back(fd);
            return false;
        }
        int headerLength = 0;
        if (bytesRead == 4)
        {
            headerLength = atoi(mBuffer);
        }
        else
        {
            return false;
        }
        if (headerLength > (READ_BUFFER_SIZE-4))
        {
            Logger::log(Error, "received long or corrupted header and removing client");
            sClientsToRemove.push_back(fd);
            return false;
        }
        bytesRead = recv(fd, mBuffer+4, headerLength, MSG_NOSIGNAL);
        if (bytesRead == 0)
        {
            sClientsToRemove.push_back(fd);
            return false;
        }
        if (bytesRead == headerLength)
        {
            std::string headerData(mBuffer+4, headerLength);
            Document d;
            d.Parse(headerData.c_str());
            unsigned int payloadLength = 0;
            if (d.HasMember("length"))
            {
                payloadLength = d["length"].GetUint();
            }
            if (d.HasMember("id"))
            {
                id = d["id"].GetInt();
            }
            bytesToRead = (payloadLength>READ_BUFFER_SIZE)?READ_BUFFER_SIZE:payloadLength;
            while(bytesToRead > 0)
            {
                memset(mBuffer, 0, READ_BUFFER_SIZE);
                bytesRead = recv(fd, mBuffer, bytesToRead, MSG_NOSIGNAL);
                bytesToRead = bytesToRead-bytesRead;
                message->append(mBuffer, bytesRead);
            }
        }
        return message->length()>0;
    }
  
    bool SocketHandler::process(int wait, std::string* message)
    {
        if (!mActive)
            return false;
  
        fd_set readfds, errfds;
        FD_ZERO(&readfds);
        FD_ZERO(&errfds);
  
        FD_SET(mFd, &readfds);
        FD_SET(mFd, &errfds);
  
        struct timeval polltimeout;
        polltimeout.tv_sec = wait;
        polltimeout.tv_usec = 0;
  
        int maxfd = mFd;
        if (mIsServer)
        {
            for (auto& client: mClients)
            {
                maxfd = (client.fd>maxfd)?client.fd:maxfd;
                FD_SET(client.fd, &readfds);
                FD_SET(client.fd, &errfds);
            }
        }
        int ret = select(maxfd + 1, &readfds, NULL, &errfds, &polltimeout);
  
        if (ret == -1)
        {
            Logger::log(Error, "error while reading  [%s]", strerror(errno));
            return false;
        }
  
        if (ret <= 0)
        {
            return false;
        }
  
        if (FD_ISSET(mFd, &readfds))
        {
            if (mIsServer)
            {
                acceptClient();
                return true;
            }
            else
            {
                int messageId;
                return readData(mFd, messageId, message);
            }
        }
        
        if (mIsServer)
        {
            for (auto& client: mClients)
            {
                if (FD_ISSET(client.fd, &readfds))
                {
                    std::string data;
                    int messageId;
                    if (readData(client.fd, messageId, &data))
                    {
                        struct ClientRequestInformation information;
                        information.id = messageId;
                        information.fd = client.fd;
                        sActiveRequestMap[sMessageId] = information;
                        mListener->onMessageReceived(sMessageId, data);
                        sMessageId++;
                    }
                }
                if (FD_ISSET(client.fd, &errfds))
                {
                    Logger::log(Warn, "received error while reading from a client");
                }
            }
            removeInactiveClients();
        }
        return false;
    }
  
    void SocketHandler::acceptClient()
    {
        Socket client;
        socklen_t socketLength;
  
        socketLength = sizeof(struct sockaddr_storage);
        memset(&client.endpoint, 0, sizeof(struct sockaddr_storage));
  
        client.fd = accept(mFd, (struct sockaddr *)&(client.endpoint), &socketLength);
        if (client.fd == -1)
        {
            Logger::log(Error, "accept: error - [%s]", strerror(errno));
            return;
        }
        mClients.push_back(client); 
    }
  
    void SocketHandler::sendEvent(std::string& event)
    {
        for (auto& client: mClients)
        {
            bool ret = sendToNetwork(-1, client.fd, event);
            if (false == ret)
            {
                Logger::log(Warn, "failed to send data to client [%d]", client.fd);
            }
        }
    }
  
    void SocketHandler::removeInactiveClients()
    {
        for (std::vector<int>::iterator iter = sClientsToRemove.begin(); iter != sClientsToRemove.end(); iter++)
        {
            std::vector<Socket>::iterator clientIterator = mClients.begin();
            while (clientIterator != mClients.end())
            {
                if ((*clientIterator).fd == *iter)
                {
                    clientIterator = mClients.erase(clientIterator); 
                }
                else
                {
                    clientIterator++;
                }
            }
        }
        sClientsToRemove.clear();
    }
  
    void SocketHandler::setListener(RdkShellClientListener* listener)
    {
        mListener = listener;
    }
}
