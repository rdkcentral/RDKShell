/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2021 RDK Management
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

#include "framebuffer.h"
#include "logger.h"

namespace RdkShell
{
    FrameBuffer::FrameBuffer(int width, int height)
        : mWidth(width)
        , mHeight(height)
        , mFboId(0)
        , mTextureId(0)
    {
        Logger::log(LogLevel::Information, "RdkShell creating FrameBuffer resolution: %d x %d", width, height);

        glGenFramebuffers(1, &mFboId);

        GLint prevFbo;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mFboId);

        glGenTextures(1, &mTextureId);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextureId, 0);

        GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            Logger::log(LogLevel::Error, "%s: glGetError() = %X\n", __func__, error);
        }

        GLenum valid = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (valid != GL_FRAMEBUFFER_COMPLETE)
        {
            Logger::log(LogLevel::Error, "%s: glCheckFramebufferStatus() = %X\n", __func__, valid);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    }

    FrameBuffer::~FrameBuffer()
    {
        glDeleteFramebuffers(1, &mFboId);
        glDeleteTextures(1, &mTextureId);
    }

    void FrameBuffer::bind()
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mPrevFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mFboId);
    }

    void FrameBuffer::unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mPrevFbo);
    }
}
