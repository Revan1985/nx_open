// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/vms/api/data/json_rpc.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::network::http { class AsyncHttpClientPtr; }
namespace nx::network::rest { struct JsonResult; }
namespace nx::network::rest { class UbjsonResult; }

namespace rest {

#if defined(__arm__)
    // Some devices lack kernel support for atomic int64.
    typedef qint32 Handle;
#else
    typedef qint64 Handle;
#endif

template<typename ResultType>
using Callback = nx::utils::MoveOnlyFunc<void(bool success, Handle requestId, ResultType result)>;

class ServerConnection;
using ServerConnectionPtr = std::shared_ptr<ServerConnection>;
using ServerConnectionWeakPtr = std::weak_ptr<ServerConnection>;

using JsonRpcBatchResultCallback = Callback<std::vector<nx::vms::api::JsonRpcResponse>>;
using JsonResultCallback = Callback<nx::network::rest::JsonResult>;
using UbJsonResultCallback = Callback<nx::network::rest::UbjsonResult>;

static constexpr std::chrono::seconds kDefaultVmsApiTimeout = std::chrono::seconds(30);

// Should be less than rest::kDefaultVmsApiTimeout which is used by client by default.
static constexpr std::chrono::seconds kDefaultDelegateTimeout = std::chrono::seconds(15);

}; // namespace rest
