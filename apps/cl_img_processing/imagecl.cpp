/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtOpenCL module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "imagecl.h"
#include "palette.h"
#include <QtCore/qvarlengtharray.h>
#include <QtGui/qcolor.h>

class ImageCLContext
{
public:
    ImageCLContext() : context(0), glContext(0) {}
    ~ImageCLContext();

    void init(bool useGL);
    void buildProgram(const QString& cl_source_file, int width, int height);

    QCLContext *context;
    QCLContextGL *glContext;
    QCLProgram program;
    QCLKernel kernel;
};

void ImageCLContext::init(bool useGL)
{
    if (context) {
        return;
    }

    if (useGL) {
        glContext = new QCLContextGL();
        context = glContext;
        if (!glContext->create())
            return;
    } else {
        context = new QCLContext();
        if (!context->create())
            return;
    }
}
void ImageCLContext::buildProgram(const QString& cl_source_file,
        int width, int height)
{
    program = context->buildProgramFromSourceFile(cl_source_file);
    kernel = program.createKernel("main");
    kernel.setGlobalWorkSize(width, height);
    kernel.setLocalWorkSize(kernel.bestLocalWorkSizeImage2D());
}

ImageCLContext::~ImageCLContext()
{
    delete context;
}

Q_GLOBAL_STATIC(ImageCLContext, image_context)

ImageCL::ImageCL(int width, int height)
    : width(width), height(height)
    , img(width, height, QImage::Format_RGB32)
    , lastIterations(-1)
    , initialized(false)
{
}

ImageCL::~ImageCL()
{
}

void ImageCL::init(bool useGL)
{
    if (initialized)
        return;
    initialized = true;

    // Initialize the context for GL or non-GL.
    ImageCLContext *ctx = image_context();
    ctx->init(useGL);
}

GLuint ImageCL::textureId()
{
    init(true);

    ImageCLContext *ctx = image_context();
    if (!textureBuffer.create(ctx->glContext, width, height))
        qWarning("Could not create the OpenCL texture to render into.");

    // FIXME Hardcoded.
    ctx->buildProgram("./apps/cl_img_processing/cl_img_processing.cl",
            width, height);

    return textureBuffer.textureId();
}

void ImageCL::initialize()
{
    init(false);
}

QSize ImageCL::size() const { return QSize(width, height); }

static bool openclDisabled = false;

bool ImageCL::hasOpenCL()
{
    if (openclDisabled)
        return false;
    return !QCLDevice::devices(QCLDevice::Default).isEmpty();
}

void ImageCL::disableCL()
{
    openclDisabled = true;
}

void ImageCL::generate()
{
    init();

    ImageCLContext *ctx = image_context();
    QCLKernel kernel = ctx->kernel;

    if (!textureBuffer.textureId()) {
        // Create a buffer for the image in the OpenCL device.
        if (imageBuffer.isNull()) {
            imageBuffer = ctx->context->createImage2DDevice
                (QImage::Format_RGB32, size(), QCLMemoryObject::WriteOnly);
        }

        // Execute the kernel.
        kernel(imageBuffer);
    } else {
        // Finish previous GL operations on the texture.
        if (ctx->glContext->supportsObjectSharing())
            glFinish();

        // Acquire the GL texture object.
        textureBuffer.acquire();

        // Execute the kernel.
        kernel(textureBuffer);

        // Release the GL texture object and wait for it complete.
        // After the release is complete, the texture can be used by GL.
        textureBuffer.release();
    }
}

void ImageCL::paint(QPainter *painter, const QPoint& point)
{
    imageBuffer.drawImage(painter, point);
}
