// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_db_client_ini.h"

namespace nx::cloud::db::api {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::cloud::db::api
