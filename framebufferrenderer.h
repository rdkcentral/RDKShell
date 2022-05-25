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

#pragma once

#include <memory>
#include <GLES2/gl2.h>

namespace RdkShell
{
    class FrameBuffer;

    class FrameBufferRenderer
    {
    public:
        static FrameBufferRenderer *instance();
        
        void draw(std::shared_ptr<FrameBuffer> fbo, uint32_t screenWidth, uint32_t screenHeight,
            float *matrix, int32_t x, int32_t y, uint32_t width, uint32_t height, float alpha = 1.0f);
    
    private:
        FrameBufferRenderer();
        ~FrameBufferRenderer();

        void createShaderProgram();

        GLuint mShaderProgram;

        GLint mPositionLocation;
        GLint mUvLocation;
        GLint mTextureLocation;
        GLint mResolutionLocation;
        GLint mMatrixLocation;
        GLint mAlphaLocation;
    };
}