#ifndef RDKSHELL_COMMUNICATION_FACTORY_H
#define RDKSHELL_COMMUNICATION_FACTORY_H

#include "communicationhandler.h"

namespace RdkShell
{
    CommunicationHandler* createCommunicationHandler(bool isServer=false);
}

#endif //RDKSHELL_COMMUNICATION_FACTORY_H
