/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include "rdkshelljson.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

namespace RdkShell
{
bool RdkShellJson::readJsonFile(const char* path, rapidjson::Document& document)
{
  if (!path || *path == 0)
  {
    std::cout << "json file path is empty\n";
    return false;
  }

  FILE* fp = fopen(path, "rb");
  if (NULL == fp)
  {
    std::cout << "unable to open json file - " << path << "\n";
    return false;
  }

  //get file size
  int fd = fileno(fp);
  struct stat fileStatValue;
  int ret = fstat(fd, &fileStatValue);
  if (-1 == ret)
  {
    std::cout << "unable to read json file state - " << path << "\n";
    fclose(fp);
    return false;
  }
  int size = fileStatValue.st_size;

  if (size == 0)
  {
    std::cout << "json file is empty - " << path << "\n";
    fclose(fp);
    return false;
  }
  
  rapidjson::ParseResult result;
  char* readBuffer = new char[size];
  memset(readBuffer, 0, size);
  rapidjson::FileReadStream is(fp, readBuffer, size);
  result = document.ParseStream(is);

  fclose(fp);
  delete[] readBuffer;

  if (!result)
  {
    rapidjson::ParseErrorCode e = document.GetParseError();
    std::cout << "[JSON parse error : (" << rapidjson::GetParseError_En(e) << ") (" << result.Offset() << ")]\n";
    return false;
  }
  return true;
}
}
