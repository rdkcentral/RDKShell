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

#include "logger.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h> 

namespace RdkShell
{
    static const char* logLevelStrings[] =
    {
      "DEBUG",
      "INFO",
      "WARN",
      "ERROR",
      "FATAL"
    };

    static const int numLogLevels = sizeof(logLevelStrings)/sizeof(logLevelStrings[0]);
    
    const char* logLevelToString(LogLevel l)
    {
      const char* s = "LOG";
      int level = (int)l;
      if (level < numLogLevels)
        s = logLevelStrings[level];
      return s;
    }

    LogLevel Logger::sLogLevel = Information;

    void Logger::setLogLevel(const char* loglevel)
    {
      LogLevel level = Information;
      if (loglevel)
      {
        if (strcasecmp(loglevel, "debug") == 0)
          level = Debug;
        else if (strcasecmp(loglevel, "info") == 0)
          level = Information;
        else if (strcasecmp(loglevel, "warn") == 0)
          level = Warn;
        else if (strcasecmp(loglevel, "error") == 0)
          level = Error;
        else if (strcasecmp(loglevel, "fatal") == 0)
          level = Fatal;
      }
      sLogLevel = level;
    }

    void Logger::logLevel(std::string& level)
    {
      level = logLevelToString(sLogLevel);
    }

    void Logger::log(LogLevel level, const char* format, ...)
    {
      if (level < sLogLevel)
      {
        return;
      }

      int threadId = syscall(__NR_gettid);
      const char* logLevel = logLevelToString(level);

      printf("[%s] Thread-%d: ", logLevel, threadId);
      va_list ptr;
      va_start(ptr, format);
      vprintf(format, ptr);
      va_end(ptr);
      printf("\n");
    }
}
