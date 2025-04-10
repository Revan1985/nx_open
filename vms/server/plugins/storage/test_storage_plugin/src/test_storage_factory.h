// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <mutex>
#include <string>
#include <unordered_set>

#include <common.h>
#include <url.h>
#include <vfs.h>

#include <storage/third_party_storage.h>

// {2E2C7A3D-256D-4018-B40E-512D72510BEC}
static const nxpl::NX_GUID IID_StorageFactory =
{ { 0x2e, 0x2c, 0x7a, 0x3d, 0x25, 0x6d, 0x40, 0x18, 0xb4, 0xe, 0x51, 0x2d, 0x72, 0x51, 0xb, 0xec } };

class TestStorageFactory:
    public nx_spl::StorageFactory,
    public PluginRefCounter<TestStorageFactory>
{
public:
    virtual const char** STORAGE_METHOD_CALL findAvailable() const override;

    virtual nx_spl::Storage* STORAGE_METHOD_CALL createStorage(
        const char* url,
        int*        ecode
    ) override;

    virtual const char* STORAGE_METHOD_CALL storageType() const override;

    virtual const char* lastErrorMessage(int ecode) const override;

public: // plugin interface implementation
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

    virtual int addRef() const override;
    virtual int releaseRef() const override;

private:
    virtual bool readConfig(const std::string& path, std::string* outContent);
    virtual nx_spl::Storage* createStorageImpl(const utils::VfsPair& vfsPair, const ::utils::Url&);

protected:
    std::unordered_set<std::string> m_storageHosts;
    std::mutex m_storageHostsMutex;
};
