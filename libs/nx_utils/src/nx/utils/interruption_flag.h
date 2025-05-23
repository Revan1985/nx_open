// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <thread>

#include <boost/container/small_vector.hpp>

namespace nx::utils {

/**
 * InterruptionFlag and InterruptionFlag::Watcher are used to allow
 * "this" destruction in some event handler.
 *
 * Example:
 * @code{.cpp}
 * class SomeProtocolClient
 * {
 *     //..
 * private:
 *     //...
 *     InterruptionFlag m_destructionFlag;
 * }

 * void SomeProtocolClient::readFromSocket(...)
 * {
 *     //...
 *     InterruptionFlag::Watcher watcher(&m_destructionFlag);
 *     eventHandler(); //this handler can free this object
 *     if (watcher.interrupted())
 *     {
 *         // "this" destroyed, or interrupt() was invoked,
 *         // but watcher is still valid since created on stack.
 *         return;
 *     }
 *     //...
 * }
 * @endcode
 */
class NX_UTILS_API InterruptionFlag
{
public:
    InterruptionFlag();

    /**
     * Invokes InterruptionFlag::interrupt().
     */
    virtual ~InterruptionFlag();

    InterruptionFlag(const InterruptionFlag&) = delete;
    InterruptionFlag& operator=(const InterruptionFlag&) = delete;

    /**
     * After this call no Watcher will use InterruptionFlag.
     * So, InterruptionFlag instance can be safely freed.
     */
    void interrupt();

    /**
     * NOTE: Multiple objects using same InterruptionFlag instance can be stacked.
     */
    class NX_UTILS_API Watcher
    {
    public:
        Watcher(InterruptionFlag* const flag);
        ~Watcher();

        Watcher(const Watcher&) = delete;
        Watcher& operator=(const Watcher&) = delete;
        Watcher(Watcher&&) = delete;
        Watcher& operator=(Watcher&&) = delete;

        bool interrupted() const;

    private:
        bool m_interrupted = false;
        InterruptionFlag* const m_objectDestructionFlag;
    };

private:
    // The number of elements in this array is very low (e.g., one) at most times.
    // Using 15 elements on all platforms - previously the MSVC implementation of std::basic_string
    // was used and it's in-place storage was 15 elements.
    boost::container::small_vector<bool*, 15> m_watcherStates;
    std::thread::id m_lastWatchingThreadId;

    void pushWatcherState(bool* watcherState);
    void popWatcherState(bool* watcherState);
};

} // namespace nx::utils
