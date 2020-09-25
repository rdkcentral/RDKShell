#include "sockethandler.h"
#include "communicationutils.h"
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
  }
  
  SocketHandler::~SocketHandler()
  {
    delete mBuffer;
    mBuffer = NULL;
  }

  bool SocketHandler::initialize()
  {
    struct hostent *hostentry = NULL;
    hostentry = gethostbyname(mServer.c_str());
    if (NULL == hostentry)
    {
      std::cout << "unable to find address!!!" <<std::endl;
      return false;
    }
    mServer = inet_ntoa(*(struct in_addr*)hostentry->h_addr);

    if (mFd != -1)
    {
      close(mFd);
    }

    struct sockaddr_in* v4addr = (struct sockaddr_in *) (mIsServer?&mLocalEndpoint:&mRemoteEndpoint);
    int ret = inet_pton(AF_INET, mServer.c_str(), &v4addr->sin_addr);
    if (1 == ret)
    {
      v4addr->sin_family = AF_INET;
      v4addr->sin_port = htons(mPort);
    }
    else
    {
      std::cout << "socket initialization failed !!!" <<std::endl;
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
    fcntl(mFd, F_SETFD, fcntl(mFd, F_GETFD) | FD_CLOEXEC);
    ret = setsockopt(mFd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
    bool issocketready = false;
    if (mIsServer)
    {
      ret = setsockopt(mFd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
      ret = bind(mFd, (struct sockaddr *)&mLocalEndpoint, sizeof(struct sockaddr_in));
      if (ret == -1)
      {
        std::cout << "Failed to bind " << strerror(errno) << std::endl;
        return false;
      }
      ret = listen(mFd, 4);
      if (ret == -1)
      {
        std::cout << "Failed to listen " << strerror(errno) << std::endl;
        return false;
      }
      issocketready = true;
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
          std::cout << "error while connecting to " << mServer << ":" << mPort << " - " <<  strerror(err) << std::endl;
          sleep(1);
          retry++;
        }
        else
        {
          connected = true;
          break;
        }
      }
      issocketready = connected;
    }
    mActive = true;
    return issocketready;
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
    int fd = mFd, messageid = id;
    if (mIsServer)
    {
      if (sActiveRequestMap.find(id) == sActiveRequestMap.end())
      {
        return false;
      }
      struct ClientRequestInformation& information = sActiveRequestMap[id];
      fd = information.fd;
      messageid = information.id;
      sActiveRequestMap.erase(id);
    }
    return sendToNetwork(messageid, fd, message);
  }

  bool SocketHandler::sendToNetwork(int messageid, int fd, std::string& message)
  {
    std::string header;
    prepareHeader(messageid, message, header);
    std::stringstream lenstr;
    lenstr<<header.length();
    int sent = send(fd, lenstr.str().c_str() , 4, MSG_NOSIGNAL);
    sent = send(fd, header.c_str(), header.length(), MSG_NOSIGNAL);
    if (sent != header.length())
    {
      int err = errno;
      std::cout << "error while sending header - " << strerror(err) << std::endl;
      return false;
    }

    sent = send(fd, message.c_str(), message.length(), MSG_NOSIGNAL);
    if (sent != message.length())
    {
      int err = errno;
      std::cout << "error while sending data - " << strerror(err) << std::endl;
      return false;
    }
    return true;
  }

  bool SocketHandler::readData(int fd, int& id, std::string* message)
  {
    ssize_t bytes_read;
    int bytes_to_read = 4;
    int bytesent;
    memset(mBuffer, 0, READ_BUFFER_SIZE);
    bytes_read = recv(fd, mBuffer, bytes_to_read, MSG_NOSIGNAL);
    if (bytes_read == -1)
    {
      std::cout << "read error: "<< strerror(errno) << std::endl;
      return false;
    }
    if ((bytes_read == 0) && mIsServer)
    {
      sClientsToRemove.push_back(fd);
      return false;
    }
    int headerLength = 0;
    if (bytes_read == 4)
    {
      headerLength = atoi(mBuffer);
    }
    else
    {
      return false;
    }
    bytes_read = recv(fd, mBuffer+4, headerLength, MSG_NOSIGNAL);
    if (bytes_read == 0)
    {
      sClientsToRemove.push_back(fd);
      return false;
    }
    if (bytes_read == headerLength)
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
      bytes_to_read = (payloadLength>READ_BUFFER_SIZE)?READ_BUFFER_SIZE:payloadLength;
      if (bytes_read == 0)
      {
        sClientsToRemove.push_back(fd);
        return false;
      }
      while(bytes_to_read > 0)
      {
        memset(mBuffer, 0, READ_BUFFER_SIZE);
        bytes_read = recv(fd, mBuffer, bytes_to_read, MSG_NOSIGNAL);
        bytes_to_read = bytes_to_read-bytes_read;
        message->append(mBuffer, bytes_read);
      }
    }
    return message->length()>0;
  }

  bool SocketHandler::poll(int wait, std::string* message)
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
      std::cout << "error while reading " << strerror(errno) << std::endl;
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
            mListener->messageReceived(sMessageId, data);
            sMessageId++;
          }
        }
        if (FD_ISSET(client.fd, &errfds))
        {
          std::cout << "received error while reading from a client " << std::endl;
        }
      }
      removeInactiveClients();
    }
    return false;
  }

  void SocketHandler::acceptClient()
  {
    Socket client;
    socklen_t                 socket_length;

    socket_length = sizeof(struct sockaddr_storage);
    memset(&client.endpoint, 0, sizeof(struct sockaddr_storage));

    client.fd = accept(mFd, (struct sockaddr *)&(client.endpoint), &socket_length);
    if (client.fd == -1)
    {
      std::cout << "accept: error << " << strerror(errno) << std::endl;
      return;
    }
    mClients.push_back(client); 
  }

  void SocketHandler::broadcast(std::string& message)
  {
    for (auto& client: mClients)
    {
      bool ret = sendToNetwork(-1, client.fd, message);
      if (false == ret)
      {
        std::cout << "error sending data to client " <<client.fd << std::endl;
      }
    }
  }

  void SocketHandler::removeInactiveClients()
  {
    for (std::vector<int>::iterator iter = sClientsToRemove.begin(); iter != sClientsToRemove.end(); iter++)
    {
      std::vector<Socket>::iterator clientiter = mClients.begin();
      while (clientiter != mClients.end())
      {
        if ((*clientiter).fd == *iter)
        {
          clientiter = mClients.erase(clientiter); 
        }
        else
        {
          clientiter++;
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
