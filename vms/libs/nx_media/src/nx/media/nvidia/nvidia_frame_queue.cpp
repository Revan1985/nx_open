// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_frame_queue.h"

#include <nvcuvid.h>

#include <nx/media/nvidia/nvidia_utils.h>
#include <nx/utils/log/log.h>

namespace nx::media::nvidia {

static constexpr int kMaxFrameQueue = 10;

FrameQueue::~FrameQueue()
{
    clear();
}

void FrameQueue::clear()
{
    std::scoped_lock lock(m_mutex);
    for (auto& frame: m_frames)
        freeFrame(frame);
    for (auto& frame: m_otherSizeFrames)
        freeFrame(frame);
    m_frames.clear();
    m_otherSizeFrames.clear();
}

void FrameQueue::configure(int widthInBytes, int bufferHeight)
{
    std::scoped_lock lock(m_mutex);
    if (m_widthInBytes == widthInBytes && m_bufferHeight == bufferHeight)
        return;

    NX_DEBUG(this, "RECONFIGURE, m_emptyFrames: %1, m_readyFrames: %2 overall: %3",
            m_emptyFrames.size(), m_readyFrames.size(), m_frames.size());

    // Keep frames (that wait for render) to delete if decoder will destroyed or release them on
    // releaseFrame call.
    for (auto& frame: m_frames)
    {
        if (std::find(m_readyFrames.begin(), m_readyFrames.end(), frame) != m_readyFrames.end() &&
            std::find(m_emptyFrames.begin(), m_emptyFrames.end(), frame) != m_emptyFrames.end())
        {
            m_otherSizeFrames.insert(frame);
        }
    }
    // release all free frames that have different buffer size, frame that waiting for rendering
    // will be released on releaseFrame call.
    for (auto& frame: m_emptyFrames)
        freeFrame(frame);

    for (auto& frame: m_readyFrames)
        freeFrame(frame);

    m_readyFrames.clear();
    m_emptyFrames.clear();
    m_frames.clear();

    m_widthInBytes = widthInBytes;
    m_bufferHeight = bufferHeight;
}

uint8_t* FrameQueue::getFreeFrame()
{
    std::scoped_lock lock(m_mutex);
    if (m_emptyFrames.empty())
    {
        if (m_frames.size() >= kMaxFrameQueue)
            return nullptr;

        NX_DEBUG(this, "Alloc frame, m_emptyFrames: %1, m_readyFrames: %2 overall: %3",
            m_emptyFrames.size(), m_readyFrames.size(), m_frames.size());

        auto frame = allocFrame();
        if (frame == nullptr)
            return nullptr;
        m_readyFrames.push_back(frame);
        m_frames.push_back(frame);
        return frame;
    }

    uint8_t* frame = m_emptyFrames.front();
    m_emptyFrames.pop_front();
    m_readyFrames.push_back(frame);
    return frame;
}

uint8_t* FrameQueue::getNextFrame()
{
    std::scoped_lock lock(m_mutex);
    if (m_readyFrames.empty())
        return nullptr;

    uint8_t* frame = m_readyFrames.front();
    m_readyFrames.pop_front();
    getCount++;
    return frame;
}

void FrameQueue::releaseFrame(uint8_t* frame)
{
    releaseCount++;
    NX_DEBUG(this, "Release frame, m_emptyFrames: %1, m_readyFrames: %2 overall: %3, releaseCount - getCount: %4-%5",
        m_emptyFrames.size(), m_readyFrames.size(), m_frames.size(), releaseCount, getCount);
    std::scoped_lock lock(m_mutex);
    if(m_otherSizeFrames.find(frame) != m_otherSizeFrames.end())
    {
        m_otherSizeFrames.erase(frame);
        freeFrame(frame);
    }
    else
    {
        m_emptyFrames.push_back(frame);
    }
}

uint8_t* FrameQueue::allocFrame()
{
    uint8_t* frame = nullptr;

    auto status = NvidiaDriverApiProxy::instance().cuMemAllocPitch(
        (CUdeviceptr*)&frame, &m_pitch, m_widthInBytes, m_bufferHeight, 16);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to alloc frame buffer: %1", toString(status));
        return nullptr;
    }
    return frame;
}

void FrameQueue::freeFrame(uint8_t* frame)
{
    auto status = NvidiaDriverApiProxy::instance().cuMemFree((CUdeviceptr)frame);
    if (status != CUDA_SUCCESS)
        NX_WARNING(this, "Failed to free frame buffer: %1", toString(status));
}

} // namespace nx::media::nvidia
