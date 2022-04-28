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

#include "rdkshell.h"
#include "rdkshellimage.h"

#include "logger.h"
#include "essosinstance.h"
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
    
    struct DataBufferState
    {
        DataBufferState(char* dataBuffer, int size): buffer(dataBuffer), bytesLeft(size)
        {
        }
        char* buffer;
        int bytesLeft;
    };

    Image::Image() : mFileName(), mProgram(0), mVertexShader(0), mFragmentShader(0),
        mResolutionLocation(0), mPositionLocation(0), mUvLocation(0), mTextureLocation(0), mTexture(0), mApngData()
    {
        initialize();
    }

    Image::Image(const std::string& fileName, int32_t x, int32_t y, int32_t width, int32_t height) : 
        mFileName(), mProgram(0), mVertexShader(0), mFragmentShader(0), mX(x), mY(y), mWidth(width), mHeight(height),
        mResolutionLocation(0), mPositionLocation(0), mUvLocation(0), mTextureLocation(0), mTexture(0), mApngData()
    {
        initialize();
        loadLocalFile(fileName);
    }

    Image::Image(const char* imageData, int32_t width, int32_t height) : mWidth(width), mHeight(height), mApngData()
    {
        initialize();
        loadImageData(imageData, mWidth*mHeight);
    }

    Image::~Image()
    {
        mApngData.clear();
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
        }
    }

    void Image::draw(bool useBounds)
    {
        if (mTexture == 0)
        {
            return;
        }

        uint32_t numFrames = mApngData.sequence.numFrames();
        if (numFrames > 0)
        {
            double t = RdkShell::seconds();
            if (mApngData.frameTime < 0)
            {
                mApngData.currentFrame = 0;
                mApngData.frameTime = t;
            }

            for (; mApngData.currentFrame < numFrames; mApngData.currentFrame++)
            {
                double d = mApngData.sequence.getDuration(mApngData.currentFrame);
                if (mApngData.frameTime + d >= t)
                {
                    break;
                }
                mApngData.frameTime += d;
            }

            if (mApngData.currentFrame >= numFrames)
            {
                mApngData.currentFrame = numFrames - 1; // snap animation to last frame

                if (!mApngData.sequence.numPlays() || mApngData.plays < mApngData.sequence.numPlays())
                {
                    mApngData.frameTime = -1; // reset animation
                    mApngData.plays++;
                }
            }

            if (mApngData.cachedFrame != mApngData.currentFrame)
            {
                FrameData &frame = mApngData.sequence.getFrameBuffer(mApngData.currentFrame);
                glBindTexture(GL_TEXTURE_2D, mTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width(),
                                frame.height(), GL_RGBA, GL_UNSIGNED_BYTE, frame.base());
                mApngData.cachedFrame = mApngData.currentFrame;
            }
        }

        glUseProgram(mProgram);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glUniform1i(mTextureLocation, 1);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        float left = -1.0f;
        float right = 1.0f;
        float top = -1.0f;
        float bottom = 1.0f;

        if (useBounds)
        {
            uint32_t screenWidth = 0;
            uint32_t screenHeight = 0;
            RdkShell::EssosInstance::instance()->resolution(screenWidth, screenHeight);

            auto screenToClipSpace = [](float x, float screenSize) { return 2.0f * x / screenSize - 1.0f; };

            left = screenToClipSpace(mX, screenWidth);
            top = screenToClipSpace(mY, screenHeight);
            right = screenToClipSpace(mX + mWidth, screenWidth);
            bottom = screenToClipSpace(mY + mHeight, screenHeight);
        }

        const float vertices[4][2] =
        {
            {left, top},
            {right, top},
            {left, bottom},
            {right, bottom}
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

        glUseProgram(0);
    }

    void Image::draw(RdkShellRect rect)
    {
        uint32_t screenWidth, screenHeight;
        RdkShell::EssosInstance::instance()->resolution(screenWidth, screenHeight);
        glEnable(GL_SCISSOR_TEST);
        glScissor(rect.x, screenHeight-rect.height-rect.y, rect.width, rect.height);
        draw();
        glDisable(GL_SCISSOR_TEST);
    }

    void Image::fileName(std::string& fileName)
    {
        fileName = mFileName;
    }

    bool Image::loadLocalFile(const std::string& fileName, uint32_t* imageWidth, uint32_t* imageHeight)
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
            bool isPngImage = false, isBitMapImage = false, isAPngImage = false;
            if ((-1 != fileName.find(".bmp")) || (-1 != fileName.find(".BMP")))
            {
                isBitMapImage = true;
                success = loadBmp(fileName, image, width, height);
            }
            else if ((-1 != fileName.find(".jpg")) || (-1 != fileName.find(".jpeg")))
            {
                success = loadJpeg(fileName, image, width, height);
            }
            else if (-1 != fileName.find(".png"))
            {
                success = loadPng(fileName, image, width, height);
                if (success)
                {
                    isPngImage = true;
                }
                else
                {
                    success = loadAPng(fileName, image, width, height);
                    isAPngImage = true;
                }
            }
            if (success)
            {
                if (imageWidth)
                    *imageWidth = width;
                if (imageHeight)
                    *imageHeight = height;

                glGenTextures(1, &mTexture);
                glBindTexture(GL_TEXTURE_2D, mTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                

                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glTexImage2D(GL_TEXTURE_2D, 0, (isPngImage||isBitMapImage||isAPngImage)?GL_RGBA:GL_RGB,
                            width, height, 0, (isPngImage||isBitMapImage||isAPngImage)?GL_RGBA:GL_RGB,
                            GL_UNSIGNED_BYTE, image);
            }
            if (!isAPngImage)
            {
                free(image);
            }
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

    bool Image::loadImageData(const char* imageData, int32_t imageSize)
    {
        bool success = false;
        if (mTexture != 0)
        {
            glDeleteTextures(1, &mTexture);
            mTexture = 0;
        }

        unsigned char *image = nullptr;
        int32_t width = 0;
        int32_t height = 0;
        bool isPngImage = true;
        success = loadPngFromData(imageData, imageSize, image, width, height); 
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
        return success;
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
        while( cinfo.output_scanline < cinfo.output_height )
        {
            jpeg_read_scanlines( &cinfo, buffer, 1 );
            for(int i = 0; i < rowStride; i++)
            {
                image[nextImageIndex++] = buffer[0][i];
            }
        }
        fclose(file);
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
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

            if (png_get_valid(pngPointer, infoPointer, PNG_INFO_acTL))
            {
                png_destroy_read_struct(&pngPointer, &infoPointer, NULL);
                fclose(file);
                return false;
            }
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

    bool Image::loadBmp(std::string fileName, unsigned char *&image, int32_t &width, int32_t &height)
    {
        FILE *file;
        int depth;
        file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            Logger::log(LogLevel::Error, "unable to open bitmap file %s", fileName.c_str());
            return false;
        }

        int16_t bitsPerPixel=0, dataOffset=0, compressionMethod=0;
        unsigned char bmpHeader[14];
        memset(bmpHeader, 0, sizeof(bmpHeader));
        fread(bmpHeader, 1, 14, file);

        if (memcmp(bmpHeader, "BM", 2) != 0)
        {
            Logger::log(LogLevel::Error, "bitmap file header is not valid %s", fileName.c_str());
            fclose(file);
            return false;
        }

        fseek(file, 10, SEEK_SET);
        fread(&dataOffset, 4, 1, file);

        fseek(file, 18, SEEK_SET);
        fread(&width, 4, 1, file);

        fseek(file, 22, SEEK_SET);
        fread(&height, 4, 1, file);

        fseek(file, 28, SEEK_SET);
        fread(&bitsPerPixel, 2, 1, file);

        fseek(file, 30, SEEK_SET);
        fread(&compressionMethod, 4, 1, file);

        Logger::log(LogLevel::Debug, "Bitmap infoformation: filename[%s] width[%d] height[%d] bitsperpixel[%d] compressionmethod [%d]", fileName.c_str(), width, height, bitsPerPixel, compressionMethod);

        if ((width == 0) || (height == 0))
        {
            Logger::log(LogLevel::Error, "width or height is 0 for file %s", fileName.c_str());
            fclose(file);
            return false;
        }

        if (compressionMethod != 0)
        {
            Logger::log(LogLevel::Error, "compression method not supported %s", fileName.c_str());
            fclose(file);
            return false;
        }

        if (bitsPerPixel !=24)
        {
            Logger::log(LogLevel::Error, "only 24 bits per pixel supported %s", fileName.c_str());
            fclose(file);
            return false;
        }


        int32_t originalPixelSize = ((int32_t)bitsPerPixel) / 8;
        int32_t pixelSize = ((int32_t)bitsPerPixel+8) / 8;
        int rowSize = (width)*(originalPixelSize); 
        int paddedRowSize = (int)(4 * ceil((float)(width) / 4.0f))*(originalPixelSize);
        int newRowSize = (width)*(pixelSize);
        int totalSize = height*newRowSize;

        image = (unsigned char*)malloc(totalSize);
        if (NULL == image)
        {
            Logger::log(LogLevel::Error, "unable to prepare bitmap data [%s]", fileName.c_str());
            fclose(file);
            return false;
        }

        int8_t alpha = 255;
        unsigned char *dataPointer = (unsigned char*)(image+((height-1)*newRowSize));
        for (int i = 0; i <height; i++)
        {
            dataPointer = (unsigned char*)(image+((height-i-1)*newRowSize));
            for (int j=0; j<paddedRowSize; j+=3)
            {
                fseek(file, dataOffset+(i*paddedRowSize) + j, SEEK_SET);
                if ((paddedRowSize -j) <  3)
                {
                    continue;
                }
                char pixel[3];
                memset(pixel, 0, 3);
                fread(pixel, 1, 3, file);
                dataPointer[0] = pixel[2];
                dataPointer[1] = pixel[1];
                dataPointer[2] = pixel[0];
                memcpy(dataPointer+3, &alpha, 1);
                dataPointer = dataPointer+4;
            }
        }
        fclose(file);
        Logger::log(LogLevel::Debug, "completed reading bitmap file [%s]", fileName.c_str());
        return true;
    }

    void Image::BlendOver(unsigned char **rows_destination, unsigned char **rows_source, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
    {
        unsigned int i, j;
        int u, v, al;
    
        for (j = 0; j < height; j++)
        {
          unsigned char *sp = rows_source[j];
          unsigned char *dp = rows_destination[j + y] + x * 4;
    
          for (i = 0; i < width; i++, sp += 4, dp += 4)
          {
            if (sp[3] == 255)
              memcpy(dp, sp, 4);
            else if (sp[3] != 0)
            {
              if (dp[3] != 0)
              {
                u = sp[3] * 255;
                v = (255 - sp[3]) * dp[3];
                al = u + v;
                dp[0] = (sp[0] * u + dp[0] * v) / al;
                dp[1] = (sp[1] * u + dp[1] * v) / al;
                dp[2] = (sp[2] * u + dp[2] * v) / al;
                dp[3] = al / 255;
              }
              else
                memcpy(dp, sp, 4);
            }
          }
        }
    }
    
    //Based on pxCore, Copyright 2005-2018 John Robinson
    //Licensed under the Apache License, Version 2.0
    bool Image::loadAPng(std::string fileName, unsigned char *&image, int32_t &width, int32_t &height)
    {
        FILE *file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            Logger::log(LogLevel::Error, "unable to open apng file %s", fileName.c_str());
            return false;
        }

        mApngData.sequence.init();
      
        unsigned char pngHeader[8];
        memset(pngHeader, 0, sizeof(pngHeader));
        fread(pngHeader, 1, 8, file);
        if (png_sig_cmp(pngHeader, 0, 8))
        {
            Logger::log(LogLevel::Error, "not a png file [%s]", fileName.c_str());
            fclose(file);
            return false;
        }

        //unsigned int width, height, channels, rowbytes, size, i, j;
        unsigned int i, j;
      
        unsigned long size, rowbytes;
      
        png_bytepp rows_image;
        png_bytepp rows_frame;
        unsigned char *p_image;
        unsigned char *p_frame;
        unsigned char *p_temp;
      
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (png_ptr && info_ptr)
        {
            if (setjmp(png_jmpbuf(png_ptr)))
            {
              png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
              return false;
            }

            png_init_io(png_ptr, file);
            png_set_sig_bytes(png_ptr, 8);
            png_read_info(png_ptr, info_ptr);
            png_set_expand(png_ptr);
            png_set_strip_16(png_ptr);
            png_set_palette_to_rgb(png_ptr);
            png_set_gray_to_rgb(png_ptr);
            png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
            (void)png_set_interlace_handling(png_ptr);
            png_read_update_info(png_ptr, info_ptr);
            width = png_get_image_width(png_ptr, info_ptr);
            height = png_get_image_height(png_ptr, info_ptr);
            //channels = png_get_channels(png_ptr, info_ptr);
            rowbytes = png_get_rowbytes(png_ptr, info_ptr);
            size = height * rowbytes;
            p_image = (unsigned char *)malloc(size);
            p_frame = (unsigned char *)malloc(size);
            p_temp = (unsigned char *)malloc(size);
            rows_image = (png_bytepp)malloc(height * sizeof(png_bytep));
            rows_frame = (png_bytepp)malloc(height * sizeof(png_bytep));
            if (p_image && p_frame && p_temp && rows_image && rows_frame)
            {
                png_uint_32 frames = 1;
                png_uint_32 x0 = 0;
                png_uint_32 y0 = 0;
                png_uint_32 w0 = width;
                png_uint_32 h0 = height;
                unsigned int first = 0;
                unsigned short delay_num = 1;
                unsigned short delay_den = 10;
      
                png_uint_32 plays = 0;
                unsigned char dop = 0;
                unsigned char bop = 0;
    
                first = (png_get_first_frame_is_hidden(png_ptr, info_ptr) != 0) ? 1 : 0;
                if (png_get_valid(png_ptr, info_ptr, PNG_INFO_acTL))
                {
                    png_get_acTL(png_ptr, info_ptr, &frames, &plays);
                }
                mApngData.sequence.setNumPlays(plays);
                for (j = 0; j < height; j++)
                {
                    rows_image[j] = p_image + j * rowbytes;
                }
                for (j = 0; j < height; j++)
                {
                    rows_frame[j] = p_frame + j * rowbytes;
                }
                for (i = 0; i < frames; i++)
                {
                    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_acTL))
                    {
                        png_read_frame_head(png_ptr, info_ptr);
                        png_get_next_frame_fcTL(png_ptr, info_ptr, &w0, &h0, &x0, &y0, &delay_num, &delay_den, &dop, &bop);
    
                        if (!delay_den)
                        {
                            delay_den = 100;
                        }
                    }
                    if (i == first)
                    {
                        bop = PNG_BLEND_OP_SOURCE;
                        if (dop == PNG_DISPOSE_OP_PREVIOUS)
                        {
                            dop = PNG_DISPOSE_OP_BACKGROUND;
                        }
                    }
                    png_read_image(png_ptr, rows_frame);
    
                    if (dop == PNG_DISPOSE_OP_PREVIOUS)
                    {
                        memcpy(p_temp, p_image, size);
                    }
                    if (bop == PNG_BLEND_OP_OVER)
                    {
                        BlendOver(rows_image, rows_frame, x0, y0, w0, h0);
                    }
                    else
                    {
                      for (j = 0; j < h0; j++)
                      {
                          memcpy(rows_image[j + y0] + x0 * 4, rows_frame[j], w0 * 4);
                      }
                    }
                    // TODO Extra copy of frame going on here
                    if (i >= first)
                    {
                        FrameData frame;
                        frame.init(width, height);
                        for (uint32_t i = 0; i < height; i++)
                        {
                            memcpy(frame.scanline(i), rows_image[i], width * 4);
                        }

                        // frame.setUpsideDown(true);
			// premultiply
                        for (int y = 0; y < height; y++)
                        {
                            Pixel* d = frame.scanline(y);
                            Pixel* de = d + width;
                            while (d < de)
                            {
                                d->r = (d->r * d->a)/255;
                                d->g = (d->g * d->a)/255;
                                d->b = (d->b * d->a)/255;
                                d++;
                            }
                        }
                        mApngData.sequence.addBuffer(frame, (double)delay_num / (double)delay_den);
                    }
    
                    if (dop == PNG_DISPOSE_OP_PREVIOUS)
                    {
                        memcpy(p_image, p_temp, size);
                    }
                    else if (dop == PNG_DISPOSE_OP_BACKGROUND)
                    {
                        for (j = 0; j < h0; j++)
                        {
                            memset(rows_image[j + y0] + x0 * 4, 0, w0 * 4);
                        }
                    }
                }
                png_read_end(png_ptr, info_ptr);
            }
            free(rows_frame);
            free(rows_image);
            free(p_temp);
            free(p_frame);
            free(p_image);
        }
        if (mApngData.sequence.numFrames() > 0)
	{
            FrameData& data = mApngData.sequence.getFrameBuffer(0);
            mWidth = data.width();
            mHeight = data.height();
            image = (unsigned char*) data.base();
	}
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    
        return true;
    }

    void imageReadData(png_structp pngPointer, png_bytep data, size_t length)
    {
        DataBufferState* bufferState = static_cast<DataBufferState*> (png_get_io_ptr(pngPointer));
        if (length > bufferState->bytesLeft)
        {
            png_error(pngPointer, "read error");
        }
        memcpy(data, bufferState->buffer, length);
        bufferState->bytesLeft -= length;
        bufferState->buffer += length;
    }

    bool Image::loadPngFromData(const char* imageData, int32_t imageSize, unsigned char *&image, int32_t &width, int32_t &height)
    {
        int depth;
        if (NULL == imageData)
        {
            Logger::log(LogLevel::Error, "unable to access image data");
            return false;
        }

        unsigned char pngHeader[8];
        memset(pngHeader, 0, sizeof(pngHeader));
        memcpy(&pngHeader, imageData, 8);
        if (png_sig_cmp(pngHeader, 0, 8))
        {
            Logger::log(LogLevel::Error, "image data is not of png content");
            return false;
        }

        png_structp pngPointer;
        png_infop infoPointer;

        pngPointer = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (NULL == pngPointer)
        {
            Logger::log(LogLevel::Error, "unable to create png structure ");
            return false;
        }

        infoPointer = png_create_info_struct(pngPointer);
        if (NULL == infoPointer)
        {
            Logger::log(LogLevel::Error, "unable to create info structure");
            png_destroy_read_struct(&pngPointer, NULL, NULL);
            return false;
        }

        bool ret = false;
        DataBufferState* bufferState = new DataBufferState((char*) imageData+8, imageSize-8);
        if (!setjmp(png_jmpbuf(pngPointer)))
        {
            png_set_read_fn(pngPointer, bufferState, imageReadData);
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
                    Logger::log(LogLevel::Error, "unable to create memory for image data");
                }
            }
            else
            {
                Logger::log(LogLevel::Error, "unable to read png info");
            }
        }
        else
        {
            Logger::log(LogLevel::Error, "unable to initialize png");
        }
        png_destroy_read_struct(&pngPointer, &infoPointer, NULL);
        delete bufferState;
        return ret;
    }
}
