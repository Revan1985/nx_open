// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ilp_video_packet.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>

#include <nx/kit/utils.h>

ILPVideoPacket::ILPVideoPacket(
    int channelNumber,
    nxcip::UsecUTCTimestamp _timestamp,
    unsigned int flags,
    unsigned int cSeq )
:
    m_refManager( this ),
    m_channelNumber( channelNumber ),
    m_timestamp( _timestamp ),
    m_buffer( NULL ),
    m_bufSize( 0 ),
    m_flags( flags ),
    m_motionData( NULL ),
    m_cSeq( cSeq )
{
}

ILPVideoPacket::~ILPVideoPacket()
{
    if( m_buffer )
    {
        nx::kit::utils::freeAligned( m_buffer );
        m_buffer = NULL;
        m_bufSize = 0;
    }

    if( m_motionData )
    {
        m_motionData->releaseRef();
        m_motionData = NULL;
    }
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* ILPVideoPacket::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_VideoDataPacket, sizeof(nxcip::IID_VideoDataPacket) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxcip::IID_MediaDataPacket, sizeof(nxcip::IID_MediaDataPacket) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::MediaDataPacket*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::PluginInterface::addRef
int ILPVideoPacket::addRef() const
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
int ILPVideoPacket::releaseRef() const
{
    return m_refManager.releaseRef();
}

//!Implementation of nxpl::MediaDataPacket::isKeyFrame
nxcip::UsecUTCTimestamp ILPVideoPacket::timestamp() const
{
    return m_timestamp;
}

//!Implementation of nxpl::MediaDataPacket::type
nxcip::DataPacketType ILPVideoPacket::type() const
{
    return nxcip::dptVideo;
}

//!Implementation of nxpl::MediaDataPacket::data
const void* ILPVideoPacket::data() const
{
    return m_buffer;
}

//!Implementation of nxpl::MediaDataPacket::dataSize
unsigned int ILPVideoPacket::dataSize() const
{
    return (int) m_bufSize;
}

//!Implementation of nxpl::MediaDataPacket::channelNumber
unsigned int ILPVideoPacket::channelNumber() const
{
    return m_channelNumber;
}

//!Implementation of nxpl::MediaDataPacket::codecType
nxcip::CompressionType ILPVideoPacket::codecType() const
{
    return nxcip::AV_CODEC_ID_MJPEG;
}

unsigned int ILPVideoPacket::flags() const
{
    return m_flags;
}

unsigned int ILPVideoPacket::cSeq() const
{
    return m_cSeq;
}

//!Implementation of nxpl::VideoDataPacket::getMotionData
nxcip::Picture* ILPVideoPacket::getMotionData() const
{
    if( m_motionData )
        m_motionData->addRef();
    return m_motionData;
}

void ILPVideoPacket::resizeBuffer( size_t bufSize )
{
    if( bufSize < m_bufSize && bufSize > 0 )
    {
        m_bufSize = bufSize;
        return;
    }

    void* newBuffer = nx::kit::utils::mallocAligned(
        bufSize + nxcip::MEDIA_PACKET_BUFFER_PADDING_SIZE,
        nxcip::MEDIA_DATA_BUFFER_ALIGNMENT );

    if( m_bufSize > 0 )
    {
        if( newBuffer )
            memcpy( newBuffer, m_buffer, std::min<>(m_bufSize, bufSize) );
        nx::kit::utils::freeAligned( m_buffer );
        m_buffer = NULL;
        m_bufSize = 0;
    }

    m_buffer = newBuffer;
    if( m_buffer )
        m_bufSize = bufSize;
}

void* ILPVideoPacket::data()
{
    return m_buffer;
}

void ILPVideoPacket::setMotionData( nxcip::Picture* motionData )
{
    m_motionData = motionData;
    if( m_motionData )
        m_motionData->addRef();
}
