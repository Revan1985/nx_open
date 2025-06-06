// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/async_stoppable.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace network {
namespace aio {

class AbstractAioThread;

/**
 * Abstract base for a class living in aio thread.
 * TODO: #akolesnikov concept is still a draft.
 */
class AbstractPollable:
    public QnStoppableAsync
{
public:
    virtual ~AbstractPollable() = default;

    virtual aio::AbstractAioThread* getAioThread() const = 0;
    /**
     * Generally, binding to aio thread can be done just after
     *   object creation before any usage.
     * Some implementation may allow more (e.g., binding if no async
     *   operations are scheduled at the moment)
     * NOTE: Re-binding is allowed.
     */
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) = 0;
    virtual void post(nx::MoveOnlyFunc<void()> func) = 0;
    virtual void dispatch(nx::MoveOnlyFunc<void()> func) = 0;
};

} // namespace aio
} // namespace network
} // namespace nx
