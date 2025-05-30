// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QString>

#include <nx/utils/url.h>

namespace nx::vms::utils {

struct NX_VMS_COMMON_API ResourceParamsData
{
    QString location;
    QByteArray value;

    static ResourceParamsData load(const nx::Url& url);
    static ResourceParamsData load(QFile&& file);

    static ResourceParamsData getWithGreaterVersion(
        const std::vector<ResourceParamsData>& datas);
};

} // namespace nx::vms::utils
