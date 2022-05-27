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

#include "shm.h"

#include <sys/shm.h>
#include <string.h>
#include <errno.h>

namespace RdkShell 
{

namespace Shm
{
    char* error()
    {
        // errno is thread local, this function must be called from the same thread as the failing function
        return strerror(errno);
    }

    void* attach(int key, size_t size)
    {
        // shmget will fail if provided size is bigger than what has been previously allocated
        // however, allocation size is most likely rounded to the nearest page size multiple

        const int shmid = shmget(key, size, 0);
        if (shmid == -1) return nullptr;

        void* memory = shmat(shmid, 0, SHM_RDONLY);
        if (memory == (void*) -1) return nullptr;
        
        return memory;
    }

    void* create(int key, size_t size)
    {
        const int shmid = shmget(key, size, 0666 | IPC_CREAT | IPC_EXCL);
        if (shmid == -1) return nullptr;

        void* memory = shmat(shmid, 0, 0);
        shmctl(shmid, IPC_RMID, 0);
        if (memory == (void*) -1) return nullptr;

        return memory;
    }

    bool detach(void* memory)
    {
        return shmdt(memory) != -1;
    }
}

}