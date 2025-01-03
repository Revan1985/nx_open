// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "discovery_manager.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#endif
#include <cstdio>

#include "camera_manager.h"
#include "dir_iterator.h"
#include "plugin.h"

DiscoveryManager::DiscoveryManager()
:
    m_refManager( ImageLibraryPlugin::instance()->refManager() )
{
}

void* DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        addRef();
        return this;
    }
    if (memcmp(&interfaceID, &nxcip::IID_CameraDiscoveryManager2, sizeof(nxcip::IID_CameraDiscoveryManager2)) == 0)
    {
        addRef();
        return this;
    }
    if (memcmp(&interfaceID, &nxcip::IID_CameraDiscoveryManager3, sizeof(nxcip::IID_CameraDiscoveryManager3)) == 0)
    {
        addRef();
        return this;
    }

    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

int DiscoveryManager::addRef() const
{
    return m_refManager.addRef();
}

int DiscoveryManager::releaseRef() const
{
    return m_refManager.releaseRef();
}

static const char* VENDOR_NAME = "IMAGE_LIBRARY";

void DiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int DiscoveryManager::findCameras( nxcip::CameraInfo* /*cameras*/, const char* /*localInterfaceIPAddr*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int DiscoveryManager::findCameras2(nxcip::CameraInfo2* /*cameras*/, const char* /*serverURL*/)
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* /*login*/, const char* /*password*/ )
{
    static constexpr char kFilePrefix[] = "file://";
    const char* path = address;
    if( strncmp( address, kFilePrefix, sizeof(kFilePrefix) - 1 ) == 0 )
    if (strncmp(address, kFilePrefix, sizeof(kFilePrefix) - 1) == 0)
    {
        path += sizeof(kFilePrefix) - 1; //< Skip the prefix.
        if (path[0] == '/') //< Skip the third slash, if any.
            ++path;
    }

    const size_t pathLen = strlen( path );
    if( pathLen == 0 || pathLen > FILENAME_MAX )
        return 0;

    struct stat fStat;
    memset( &fStat, 0, sizeof(fStat) );

    if( path[pathLen-1] == '/' || path[pathLen-1] == '\\' )
    {
        //removing trailing separator
        char tmpNameBuf[FILENAME_MAX+1];
        strcpy( tmpNameBuf, path );
        for( char* pos = tmpNameBuf+pathLen-1;
            pos >= tmpNameBuf && (*pos == '/' || *pos == '\\');
            --pos )
        {
            *pos = '\0';
        }
        if( stat( tmpNameBuf, &fStat ) != 0 )
            return 0;
    }
    else
    {
        if( stat( path, &fStat ) != 0 )
            return 0;
    }

    if( !(fStat.st_mode & S_IFDIR) )
        return 0;

    //path is a path to local directory

    //checking, whether the path dir contains jpg images
    DirIterator dirIterator( path );

    //m_dirIterator.setRecursive( true );
    dirIterator.setEntryTypeFilter( FsEntryType::etRegularFile );
    dirIterator.setWildCardMask( "*.jp*g" );    //jpg or jpeg

    //reading directory
    bool isImageLibrary = false;
    while( dirIterator.next() )
    {
        isImageLibrary = true;
        break;
    }
    if( !isImageLibrary )
        return 0;

    strcpy( cameras[0].url, path );
    strcpy( cameras[0].uid, path );
    strcpy( cameras[0].modelName, path );

    return 1;
}

//!Implementation of nxcip::CameraDiscoveryManager2::checkHostAddress2
int DiscoveryManager::checkHostAddress2(nxcip::CameraInfo2* cameras, const char* address, const char* login, const char* password)
{
    return checkHostAddress(cameras, address, login, password);
}

int DiscoveryManager::fromMDNSData(
    const char* /*discoveredAddress*/,
    const unsigned char* /*mdnsResponsePacket*/,
    int /*mdnsResponsePacketSize*/,
    nxcip::CameraInfo* /*cameraInfo*/ )
{
    return 0;
}

int DiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
{
    return 0;
}

nxcip::BaseCameraManager* DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
{
    const nxcip::CameraInfo& infoCopy( info );
    if( strlen(infoCopy.auxiliaryData) == 0 )
    {
        //TODO/IMPL checking, if audio is present at info.url
    }
    return new CameraManager( infoCopy );
}

int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}

int DiscoveryManager::getCapabilities() const
{
    return Capability::findLocalResources;
}
