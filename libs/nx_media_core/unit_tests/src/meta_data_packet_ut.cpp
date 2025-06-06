// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/media/meta_data_packet.h>

namespace nx::media::test {

TEST(MetaData, serialization)
{
    QnMetaDataV1 packet;
    packet.metadataType = MetadataType::Motion;
    packet.m_duration = 5000;
    packet.timestamp = 12345678000; // Serializer will truncate microseconds part.

    // Serialize.
    auto data = packet.serialize();

    // Deserialize.
    QnMetaDataV1Light motionData;
    ASSERT_EQ(data.size(), sizeof(motionData));
    memcpy(&motionData, data.data(), sizeof(motionData));
    motionData.doMarshalling();
    auto packetCopy = QnMetaDataV1::fromLightData(motionData);

    ASSERT_EQ(packet.m_duration, packetCopy->m_duration);
    ASSERT_EQ(packet.timestamp, packetCopy->timestamp);
}

TEST(QnCompressedMetadata, main)
{
    const QByteArray buffer1("12345");
    const QByteArray buffer2("6789");

    QnCompressedMetadata data(MetadataType::ObjectDetection);
    data.setData(buffer1);
    ASSERT_EQ(buffer1, QByteArray::fromRawData(data.data(), data.dataSize()));

    data.setData(buffer2);
    ASSERT_EQ(buffer2, QByteArray::fromRawData(data.data(), data.dataSize()));
}

} // namespace nx::media::test
