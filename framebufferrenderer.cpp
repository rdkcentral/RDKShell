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

#include "framebufferrenderer.h"
#include "framebuffer.h"
#include "logger.h"

#include <GLES2/gl2.h>

namespace RdkShell
{
    FrameBufferRenderer::FrameBufferRenderer()
    {
        createShaderProgram();
    }

    FrameBufferRenderer::~FrameBufferRenderer()
    {
        glDeleteProgram(mShaderProgram);
    }

    FrameBufferRenderer *FrameBufferRenderer::instance()
    {
        static FrameBufferRenderer renderer;

        return &renderer;
    }

    void FrameBufferRenderer::draw(std::shared_ptr<FrameBuffer> fbo, uint32_t screenWidth, uint32_t screenHeight, 
        float *matrix, int32_t boundsX, int32_t boundsY, uint32_t boundsWidth, uint32_t boundsHeight, float alpha)
    {
        // Logger::log(LogLevel::Error, "FrameBufferRenderer::blit 1 fbo: %x, screen res: %d x %d, dst rect: %d, %d, %d, %d, fbo res: %d x %d",
        //     fbo, screenWidth, screenHeight, boundsX, boundsY, boundsWidth, boundsHeight, fbo->width(), fbo->height());

        glUseProgram(mShaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo->texture());
        glUniform1i(mTextureLocation, 0);
        glUniform2f(mResolutionLocation, screenWidth, screenHeight);
        glUniformMatrix4fv(mMatrixLocation, 1, GL_FALSE, matrix);
        glUniform1f(mAlphaLocation, alpha);

        // vertices in screen coordinates, origin of the blit is translated using the matrix
        const float vertices[4][2] =
        {
            {0, 0},
            {boundsWidth, 0},
            {0, boundsHeight},
            {boundsWidth, boundsHeight}
        };

        const float uvCoordinates[4][2] =
        {
            {0.f, 1.f},
            {1.f, 1.f},
            {0.f, 0.f},
            {1.f, 0.f}
        };

        glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glVertexAttribPointer(mUvLocation, 2, GL_FLOAT, GL_FALSE, 0, uvCoordinates);
        glEnableVertexAttribArray(mPositionLocation);
        glEnableVertexAttribArray(mUvLocation);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(mPositionLocation);
        glDisableVertexAttribArray(mUvLocation);
        glUseProgram(0);
    }

    void FrameBufferRenderer::createShaderProgram()
    {
        const char* vertexShaderSource = 
            "attribute vec2 a_position; \n"
            "attribute vec2 a_uv; \n"
            "uniform vec2 u_resolution;\n"
            "uniform mat4 u_matrix;\n"
            "varying vec2 v_uv; \n"
            "void main() \n"
            "{ \n"
            "  vec4 pos = u_matrix * vec4(a_position, 0, 1);\n"
            "  vec4 zeroToOne = pos / vec4(u_resolution, u_resolution.x, 1);\n"
            "  vec4 zeroToTwo = zeroToOne * vec4(2.0, 2.0, 1.0, 1.0);\n"
            "  vec4 clipSpace = zeroToTwo - vec4(1.0, 1.0, 0.0, 0.0);\n"
            "  gl_Position = clipSpace * vec4(1, -1, 1, 1);\n"
            "  v_uv = a_uv; \n"
            "} \n";

        const char* fragmentShaderSource =
            "precision lowp float; \n"
            "varying vec2 v_uv; \n"
            "uniform sampler2D s_texture; \n"
            "uniform float u_alpha; \n"
            "void main() \n"
            "{ \n"
            "  gl_FragColor = texture2D(s_texture, v_uv) * u_alpha; \n"
            "}\n";
        
        GLint status;

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);

        if (!status)
        {
            char errorLog[1000];
            GLsizei errorLength;
            glGetShaderInfoLog(fragmentShader, 1000, &errorLength, errorLog);

            Logger::log(LogLevel::Error, "error compiling fragment shader: %s", errorLog);

            glDeleteShader(fragmentShader);
            return;
        }

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);

        if (!status)
        {
            char errorLog[1000];
            GLsizei errorLength;
            glGetShaderInfoLog(vertexShader, 1000, &errorLength, errorLog);

            Logger::log(LogLevel::Error, "error compiling vertex shader: %s", errorLog);

            glDeleteShader(fragmentShader);
            glDeleteShader(vertexShader);
            return;
        }

        mShaderProgram = glCreateProgram();
        glAttachShader(mShaderProgram, fragmentShader);
        glAttachShader(mShaderProgram, vertexShader);


        mPositionLocation = 0;
        mUvLocation = 1;
        glBindAttribLocation(mShaderProgram, mPositionLocation, "a_position");
        glBindAttribLocation(mShaderProgram, mUvLocation,  "a_uv");

        glLinkProgram(mShaderProgram);
        glGetProgramiv(mShaderProgram, GL_LINK_STATUS, &status);

        if (!status)
        {
            char errorLog[1000];
            GLsizei errorLength;
            glGetProgramInfoLog(mShaderProgram, 1000, &errorLength, errorLog);
            Logger::log(LogLevel::Error, "error linking the program %s", errorLog);
        }

        // after link operation we can detach and delete the shader objects
        glDetachShader(mShaderProgram, fragmentShader);
        glDetachShader(mShaderProgram, vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteShader(vertexShader);

        mTextureLocation = glGetUniformLocation(mShaderProgram, "s_texture");
        mResolutionLocation = glGetUniformLocation(mShaderProgram, "u_resolution");
        mMatrixLocation = glGetUniformLocation(mShaderProgram, "u_matrix");
        mAlphaLocation = glGetUniformLocation(mShaderProgram, "u_alpha");
    }
}