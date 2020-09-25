#include "communicationutils.h"
#include <sstream>

namespace RdkShell
{
  void prepareHeader(int id, std::string& message, std::string& header)
  {
    std::stringstream str("");
    str<<"{\"id\":" << id;
    str<<",\"length\":"<<(message.length());
    str<<"}";
    header = str.str();
  }
}
