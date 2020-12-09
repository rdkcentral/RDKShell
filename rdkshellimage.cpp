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

#include "rdkshellimage.h"

#include "logger.h"
#include "compositorcontroller.h"
#include <jpeglib.h>
#include <png.h>
#include <string.h>
#include <setjmp.h>

namespace RdkShell
{
    struct jpegErrorManagerStruct
    {
        struct jpeg_error_mgr pub;
        jmp_buf setjmp_buffer;
    };

    char jpegLastErrorMsg[JMSG_LENGTH_MAX];
    void onJpegError(j_common_ptr cinfo)
    {
        jpegErrorManagerStruct* error = (jpegErrorManagerStruct*) cinfo->err;
        ( *(cinfo->err->format_message) ) (cinfo, jpegLastErrorMsg);
        longjmp(error->setjmp_buffer, 1);
    }

    GLchar imageVertexShaderString[] =
        "attribute vec2 a_position; \n"
        "attribute vec2 a_uv; \n"
        "varying vec2 v_uv; \n"
        "void main() \n"
        "{ \n"
        "  gl_Position = vec4(a_position, 0.0, 1.0); \n"
        "  v_uv = a_uv; \n"
        "} \n";

    GLchar imageFragmentShaderString[] =
        "precision lowp float; \n"
        "varying vec2 v_uv; \n"
        "uniform sampler2D s_texture; \n"
        "void main() \n"
        "{ \n"
        "  gl_FragColor = texture2D(s_texture, v_uv); \n"
        "}\n";
    
    Image::Image() : mFileName(), mProgram(0), mVertexShader(0), mFragmentShader(0),
        mResolutionLocation(0), mPositionLocation(0), mUvLocation(0), mTextureLocation(0), mTexture(0)
    {
        initialize();
    }

    Image::Image(const std::string& fileName, int32_t x, int32_t y, int32_t width, int32_t height) : 
        mFileName(), mProgram(0), mVertexShader(0), mFragmentShader(0), mX(x), mY(y), mWidth(width), mHeight(height),
        mResolutionLocation(0), mPositionLocation(0), mUvLocation(0), mTextureLocation(0), mTexture(0)
    {
        initialize();
        loadLocalFile(fileName);
    }

    Image::~Image()
    {
        mFileName = "";
        if (mTexture != 0)
        {
            glDeleteTextures(1, &mTexture);
            mTexture = 0;
        }
        glDetachShader(mProgram, mFragmentShader);
        glDetachShader(mProgram, mVertexShader);
        glDeleteShader(mFragmentShader);
        glDeleteShader(mVertexShader);
        glDeleteProgram(mProgram);
    }

    bool Image::createProgram(const GLchar* vertexShaderString, const char* fragmentShaderString)
    {
        GLint stat;

        mFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        const GLchar *fragmentSource = (const GLchar *)fragmentShaderString;
        glShaderSource(mFragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(mFragmentShader);
        glGetShaderiv(mFragmentShader, GL_COMPILE_STATUS, &stat);

        if (!stat)
        {
            Logger::log(LogLevel::Error, "error compiling the fragment shader %d", glGetError());

            GLint maxLength = 0;
            glGetShaderiv(mFragmentShader, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<char> errorLog(maxLength);
            glGetShaderInfoLog(mFragmentShader, maxLength, &maxLength, (char *) &errorLog[0]);

            Logger::log(LogLevel::Error, "compile error: %s", (char *) &errorLog[0]);

            glDeleteShader(mFragmentShader);
            return false;
        }

        mVertexShader = glCreateShader(GL_VERTEX_SHADER);
        const GLchar *vertextSource = (const GLchar *)vertexShaderString;
        glShaderSource(mVertexShader, 1, &vertextSource, NULL);
        glCompileShader(mVertexShader);
        glGetShaderiv(mVertexShader, GL_COMPILE_STATUS, &stat);

        if (!stat)
        {
            char log[1000];
            GLsizei len;
            glGetShaderInfoLog(mVertexShader, 1000, &len, log);
            Logger::log(LogLevel::Error, "compile error: %s", log);

            Logger::log(LogLevel::Error, "error compiling the vertex shader %d", glGetError());

            return false;
        }

        mProgram = glCreateProgram();
        glAttachShader(mProgram, mFragmentShader);
        glAttachShader(mProgram, mVertexShader);

        return true;
    }

    void Image::initialize()
    {
        bool success = createProgram(imageVertexShaderString, imageFragmentShaderString);
        if (success)
        {
            mPositionLocation = 0;
            mUvLocation = 1;
            glBindAttribLocation(mProgram, mPositionLocation, "a_position");
            glBindAttribLocation(mProgram, mUvLocation,  "a_uv");

            GLint stat;

            glLinkProgram(mProgram);  // needed to put attribs into effect
            glGetProgramiv(mProgram, GL_LINK_STATUS, &stat);

            if (!stat)
            {
                char log[1000];
                GLsizei len;
                glGetProgramInfoLog(mProgram, 1000, &len, log);
                Logger::log(LogLevel::Error, "error linking the program %s", log);
            }
            mTextureLocation = glGetUniformLocation(mProgram, "s_texture");

            glUseProgram(mProgram);
        }
    }

    void Image::draw()
    {
        if (mTexture == 0)
        {
            return;
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glUniform1i(mTextureLocation, 1);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        static const float vertices[4][2] =
        {
            {-1, -1},
            {1, -1},
            {-1, 1},
            {1, 1}
        };

        static const float uvCoordinates[4][2] =
        {
            { 0, 1},
            { 1, 1},
            { 0, 0},
            { 1, 0}
        };

        glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glVertexAttribPointer(mUvLocation, 2, GL_FLOAT, GL_FALSE, 0, uvCoordinates);
        glEnableVertexAttribArray(mPositionLocation);
        glEnableVertexAttribArray(mUvLocation);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(mPositionLocation);
        glDisableVertexAttribArray(mUvLocation);
    }

    void Image::fileName(std::string& fileName)
    {
        fileName = mFileName;
    }

    bool Image::loadLocalFile(const std::string& fileName)
    {
        bool success = false;
        if (mFileName != fileName)
        {
            mFileName = fileName;

            if (mTexture != 0)
            {
                glDeleteTextures(1, &mTexture);
                mTexture = 0;
            }

            unsigned char *image = nullptr;
            int32_t width = 0;
            int32_t height = 0;
            bool isPngImage = false;
            if (-1 != fileName.find(".png"))
            {
                success = loadPng(fileName, image, width, height);
                isPngImage = true;
            }
            else
            {
                success = loadJpeg(fileName, image, width, height);
            }
            if (success)
            {
                glGenTextures(1, &mTexture);
                glBindTexture(GL_TEXTURE_2D, mTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                

                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glTexImage2D(GL_TEXTURE_2D, 0, (isPngImage)?GL_RGBA:GL_RGB,
                            width, height, 0, (isPngImage)?GL_RGBA:GL_RGB,
                            GL_UNSIGNED_BYTE, image);
            }
            free(image);
        }
        return success;
    }

    void Image::bounds(int32_t& x, int32_t& y, int32_t& width, int32_t& height)
    {
        x = mX;
        y = mY;
        width = mWidth;
        height = mHeight;
    }

    void Image::setBounds(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        mX = x;
        mY = y;
        mWidth = width;
        mHeight = height;
    }

    bool Image::loadJpeg(std::string fileName, unsigned char *&image, int32_t &width, int32_t &height)
    {
        FILE *file;
        int depth;
        file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            Logger::log(LogLevel::Error, "unable to open %s", fileName.c_str());
            return false;
        }
        fflush(stdout);
        struct jpeg_decompress_struct cinfo;
        jpegErrorManagerStruct jpegError;
        cinfo.err = jpeg_std_error(&jpegError.pub);
        jpegError.pub.error_exit = onJpegError;
        if (setjmp(jpegError.setjmp_buffer))
        {
            Logger::log(LogLevel::Error, "error decoding: %s", jpegLastErrorMsg);
            jpeg_destroy_decompress(&cinfo);
            fclose(file);
            return false;
        }
        jpeg_create_decompress(&cinfo);
        jpeg_stdio_src(&cinfo, file);
        jpeg_read_header(&cinfo, 0);
        cinfo.scale_num = 1;
        cinfo.scale_denom = 1;
        jpeg_start_decompress(&cinfo);
        width = cinfo.output_width;
        height = cinfo.output_height;
        depth = cinfo.num_components;
        int32_t rowStride = width * depth;
        image = (unsigned char *) malloc(width * height * depth);
        JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, rowStride, 1);
        int64_t nextImageIndex = 0;
        fflush(stdout);
        while( cinfo.output_scanline < cinfo.output_height )
        {
            jpeg_read_scanlines( &cinfo, buffer, 1 );
            for(int i = 0; i < rowStride; i++)
            {
                image[nextImageIndex++] = buffer[0][i];
            }
        }
        fflush(stdout);
        fclose(file);
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fflush(stdout);
        return true;
    }

    bool Image::loadPng(std::string fileName, unsigned char *&image, int32_t &width, int32_t &height)
    {
        FILE *file;
        int depth;
        file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            Logger::log(LogLevel::Error, "unable to open %s", fileName.c_str());
            return false;
        }
        fflush(stdout);

        unsigned char pngHeader[8];
        memset(pngHeader, 0, sizeof(pngHeader));
        fread(pngHeader, 1, 8, file);
        if (png_sig_cmp(pngHeader, 0, 8))
        {
            Logger::log(LogLevel::Error, "not a png file [%s]", fileName.c_str());
            fclose(file);
            return false;
        }

        png_structp pngPointer;
        png_infop infoPointer;

        pngPointer = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (NULL == pngPointer)
        {
            Logger::log(LogLevel::Error, "unable to create png structure [%s]", fileName.c_str());
            fclose(file);
            return false;
        }

        infoPointer = png_create_info_struct(pngPointer);
        if (NULL == infoPointer)
        {
            Logger::log(LogLevel::Error, "unable to create info structure [%s]", fileName.c_str());
            png_destroy_read_struct(&pngPointer, NULL, NULL);
            fclose(file);
            return false;
        }

        bool ret = false;
        if (!setjmp(png_jmpbuf(pngPointer)))
        {
            png_init_io(pngPointer, file);
            png_set_sig_bytes(pngPointer, 8);
 
            png_read_info(pngPointer, infoPointer);

            width = png_get_image_width(pngPointer, infoPointer);
            height = png_get_image_height(pngPointer, infoPointer);
            png_byte colorType = png_get_color_type(pngPointer, infoPointer);
            png_byte bitDepth = png_get_bit_depth(pngPointer, infoPointer);
             
            if (bitDepth == 16)
            {
                png_set_strip_16(pngPointer);
            }
             
            if (colorType == PNG_COLOR_TYPE_PALETTE)
            {
                png_set_palette_to_rgb(pngPointer);
            }

            if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
            {
                png_set_gray_to_rgb(pngPointer);
            }

            if (png_get_valid(pngPointer, infoPointer, PNG_INFO_tRNS))
            {
                png_set_tRNS_to_alpha(pngPointer);
            }
            
            png_set_add_alpha(pngPointer, 0xff, PNG_FILLER_AFTER);
            
            png_read_update_info(pngPointer, infoPointer);

            if (!setjmp(png_jmpbuf(pngPointer)))
            {
                uint32_t rowBytes = png_get_rowbytes(pngPointer,infoPointer);
                png_bytep* rowPointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
                if (NULL != rowPointers)
                {
                    image = (unsigned char*) malloc(rowBytes * height);
                    if (NULL != image)
                    {
                        for (int row=0; row<height; row++)
                        {
                            rowPointers[row] = (png_byte*)((png_byte*)image + (row*rowBytes));
                        }
                        
                        png_read_image(pngPointer, rowPointers);

                        png_read_end(pngPointer, NULL);

                        free(rowPointers);
                        rowPointers = NULL;
                        ret = true;
                    }
                }
                else
                {
                    Logger::log(LogLevel::Error, "unable to create memory for image data [%s]", fileName.c_str());
                }
            }
            else
            {
                Logger::log(LogLevel::Error, "unable to read png info [%s]", fileName.c_str());
            }
        }
        else
        {
            Logger::log(LogLevel::Error, "unable to initialize png [%s]", fileName.c_str());
        }
        png_destroy_read_struct(&pngPointer, &infoPointer, NULL);
        fclose(file);
        return ret;
    }
}