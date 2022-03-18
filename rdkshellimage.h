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

#pragma once

#include <string>
#include <GLES2/gl2.h>
#include "rdkshellrect.h"

namespace RdkShell
{
    class Image
    {
        public:
            Image();
            Image(const std::string& fileName, int32_t x, int32_t y, int32_t width, int32_t height);
            Image(const char* imageData, int32_t width, int32_t height);
            ~Image();
            void draw(bool useBounds = false);
            void draw(RdkShellRect rect);
            void fileName(std::string& fileName);
            bool loadLocalFile(const std::string& fileName, uint32_t* imageWidth = nullptr, uint32_t* imageHeight = nullptr);
            void bounds(int32_t& x, int32_t& y, int32_t& width, int32_t& height);
            void setBounds(int32_t x, int32_t y, int32_t width, int32_t height);
            bool loadImageData(const char* imageData, int32_t imageSize);
        private:
            bool createProgram(const GLchar* vertexShaderString, const GLchar* fragmentShaderString);
            void initialize();
            bool loadJpeg(std::string fileName, unsigned char *&image, int32_t &width, int32_t &height);
            bool loadPng(std::string fileName, unsigned char *&image, int32_t &width, int32_t &height);
            bool loadBmp(std::string fileName, unsigned char *&image, int32_t &width, int32_t &height);
            bool loadPngFromData(const char* imageData, int32_t imageSize, unsigned char *&image, int32_t &width, int32_t &height);
            std::string mFileName;
            int32_t mX;
            int32_t mY;
            int32_t mWidth;
            int32_t mHeight;
            GLint mProgram;
            GLint mVertexShader;
            GLint mFragmentShader;
            GLint mResolutionLocation;
            GLint mPositionLocation;
            GLint mUvLocation; 
            GLint mTextureLocation;
            GLuint mTexture;
    };
}
