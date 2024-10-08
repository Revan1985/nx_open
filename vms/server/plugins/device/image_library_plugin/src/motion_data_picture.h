// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

class MotionDataPicture
:
    public nxcip::Picture
{
public:
    MotionDataPicture();
    virtual ~MotionDataPicture();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

    //!Returns pixel format
    virtual nxcip::PixelFormat pixelFormat() const override;
    virtual int planeCount() const override;
    //!Width (pixels)
    virtual int width() const override;
    //!Height (pixels)
    virtual int height() const override;
    //!Length of horizontal line in bytes
    virtual int xStride( int planeNumber ) const override;
    //!Returns pointer to horizontal line \a lineNumber (starting with 0)
    virtual const void* scanLine( int planeNumber, int lineNumber ) const override;
    virtual void* scanLine( int planeNumber, int lineNumber ) override;
    /*!
        \return Picture data. Returned buffer MUST be aligned on \a MEDIA_DATA_BUFFER_ALIGNMENT - byte boundary (this restriction helps for some optimization).
            \a nx::kit::utils::mallocAligned and \a nx::kit::utils::freeAligned routines can be used for that purpose
    */
    virtual void* data() override;
    virtual const void* data() const override;

    void setPixel( int x, int y, int val );
    void fillRect( int x, int y, int width, int height, int val );

private:
    nxpt::CommonRefManager m_refManager;
    uint8_t* m_data;
    size_t m_width;
    size_t m_height;
    size_t m_stride;
};
