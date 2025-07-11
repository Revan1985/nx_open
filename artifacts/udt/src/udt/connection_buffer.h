/*****************************************************************************
Copyright (c) 2001 - 2009, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the
above copyright notice, this list of conditions
and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
nor the names of its contributors may be used to
endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
Yunhong Gu, last updated 05/05/2009
*****************************************************************************/

#pragma once

#include <fstream>
#include <mutex>
#include <optional>

#include "common.h"
#include "queue.h"

/**
 * Socket's send buffer.
 */
class SendBuffer
{
public:
    SendBuffer(int size = 32, int mss = UDT::kDefaultMSS);
    ~SendBuffer();

    // Functionality:
    //    Insert a user buffer into the sending list.
    // Parameters:
    //    0) [in] data: pointer to the user data block.
    //    1) [in] len: size of the block.
    //    2) [in] ttl: time to live in milliseconds
    //    3) [in] order: if the block should be delivered in order, for DGRAM only
    // Returned value:
    //    None.

    void addBuffer(
        const char* data, int len,
        std::chrono::milliseconds ttl = std::chrono::milliseconds(-1), bool order = false);

    // Functionality:
    //    Read a block of data from file and insert it into the sending list.
    // Parameters:
    //    0) [in] ifs: input file stream.
    //    1) [in] len: size of the block.
    // Returned value:
    //    actual size of data added from the file.

    int addBufferFromFile(std::fstream& ifs, int len);

    // Functionality:
    //    Find data position to pack a DATA packet from the furthest reading point.
    // Parameters:
    //    0) [out] msgno: message number of the packet.

    std::optional<BufferRef> readData(int32_t& msgno);

    // Functionality:
    //    Find data position to pack a DATA packet for a retransmission.
    // Parameters:
    //    0) [in] offset: offset from the last ACK point.
    //    1) [out] msgno: message number of the packet.
    //    2) [out] msglen: length of the message

    std::optional<BufferRef> readData(const int offset, int32_t& msgno, int& msglen);

    // Functionality:
    //    Update the ACK point and may release/unmap/return the user data according to the flag.
    // Parameters:
    //    0) [in] offset: number of packets acknowledged.
    // Returned value:
    //    None.

    void ackData(int offset);

    // Functionality:
    //    Read size of data still in the sending list.
    // Parameters:
    //    None.
    // Returned value:
    //    Current size of the data in the sending list.

    int getCurrBufSize() const;

private:
    void increase();

private:
    mutable std::mutex m_mutex;           // used to synchronize buffer operation

    struct Block
    {
        char* m_pcData;                   // pointer to the data block
        int m_iLength;                    // length of the block

        int32_t m_iMsgNo;                 // message number
        std::chrono::microseconds m_OriginTime;            // original request time
        std::chrono::milliseconds m_iTTL;                       // time to live (milliseconds)

        Block* m_pNext;                   // next block
    } *m_pBlock, *m_pFirstBlock, *m_pCurrBlock, *m_pLastBlock;

    // m_pBlock:         The head pointer
    // m_pFirstBlock:    The first block
    // m_pCurrBlock:    The current block
    // m_pLastBlock:     The last block (if first == last, buffer is empty)

    struct BufferNode
    {
        char* m_pcData;            // buffer
        int m_iSize;            // size
        BufferNode* m_pNext;            // next buffer
    } *m_pBuffer;            // physical buffer

    int32_t m_iNextMsgNo;                // next message number

    int m_iSize;                // buffer size (number of packets)
    int m_iMSS;                          // maximum segment/packet size

    int m_iCount;            // number of used blocks

private:
    SendBuffer(const SendBuffer&);
    SendBuffer& operator=(const SendBuffer&);
};

//-------------------------------------------------------------------------------------------------

/**
 * Socket's receive buffer.
 */
class ReceiveBuffer
{
public:
    ReceiveBuffer(int bufsize = 65536);
    ~ReceiveBuffer();

    // Functionality:
    //    Write data into the buffer.
    // Parameters:
    //    0) [in] unit: pointer to a data unit containing new packet
    //    1) [in] offset: offset from last ACK point.
    // Returned value:
    //    false if data is repeated.

    bool addData(Unit unit, int offset);

    // Functionality:
    //    Read data into a user buffer.
    // Parameters:
    //    0) [in] data: pointer to user buffer.
    //    1) [in] len: length of user buffer.
    // Returned value:
    //    size of data read.

    int readBuffer(char* data, int len);

    // Functionality:
    //    Read data directly into file.
    // Parameters:
    //    0) [in] file: C++ file stream.
    //    1) [in] len: expected length of data to write into the file.
    // Returned value:
    //    size of data read.

    int readBufferToFile(std::fstream& ofs, int len);

    // Functionality:
    //    Update the ACK point of the buffer.
    // Parameters:
    //    0) [in] len: size of data to be acknowledged.
    // Returned value:
    //    1 if a user buffer is fulfilled, otherwise 0.

    void ackData(int len);

    // Functionality:
    //    Query how many buffer space left for data receiving.
    // Parameters:
    //    None.
    // Returned value:
    //    size of available buffer space (including user buffer) for data receiving.

    int getAvailBufSize() const;

    // Functionality:
    //    Query how many data has been continuously received (for reading).
    // Parameters:
    //    None.
    // Returned value:
    //    size of valid (continuous) data for reading.

    int getRcvDataSize() const;

    // Functionality:
    //    mark the message to be dropped from the message list.
    // Parameters:
    //    0) [in] msgno: message number.
    // Returned value:
    //    None.

    void dropMsg(int32_t msgno);

    // Functionality:
    //    read a message.
    // Parameters:
    //    0) [out] data: buffer to write the message into.
    //    1) [in] len: size of the buffer.
    // Returned value:
    //    actual size of data read.

    int readMsg(char* data, int len);

    // Functionality:
    //    Query how many messages are available now.
    // Parameters:
    //    None.
    // Returned value:
    //    number of messages available for recvmsg.

    int getRcvMsgNum();

private:
    int getRcvDataSize(const std::lock_guard<std::mutex>&) const;

    bool scanMsg(
        const std::lock_guard<std::mutex>& lock,
        int& start, int& end, bool& passack);

private:
    mutable std::mutex m_mutex;           // used to synchronize buffer operation

    std::vector<std::optional<Unit>> m_pUnit;                     // pointer to the protocol buffer
    int m_iSize;                         // size of the protocol buffer

    int m_iStartPos;                     // the head position for I/O (inclusive)
    int m_iLastAckPos;                   // the last ACKed position (exclusive)
                                         // EMPTY: m_iStartPos = m_iLastAckPos   FULL: m_iStartPos = m_iLastAckPos + 1
    int m_iMaxPos;            // the furthest data position

    int m_iNotch;            // the starting read point of the first unit

private:
    ReceiveBuffer();
    ReceiveBuffer(const ReceiveBuffer&);
    ReceiveBuffer& operator=(const ReceiveBuffer&);
};
