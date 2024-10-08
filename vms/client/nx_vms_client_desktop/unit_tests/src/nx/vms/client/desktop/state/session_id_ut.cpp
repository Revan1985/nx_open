// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/state/session_id.h>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

static const auto kExampleSystemLocalId = nx::Uuid::createUuid();
static const QString kExampleSystemId = kExampleSystemLocalId.toSimpleString();
static const QString kExampleUserName = "SomeUser@someSystem.com";

} // namespace

TEST(SessionId, dependsOnParameters)
{
    const SessionId sessionId(kExampleSystemLocalId, kExampleSystemId, kExampleUserName);
    const SessionId sessionIdFromAnotherName(
        kExampleSystemLocalId, kExampleSystemId, kExampleUserName.mid(2));
    const SessionId sessionIdFromAnotherSystem(
        kExampleSystemLocalId, kExampleSystemId.mid(2), kExampleUserName.toUpper());

    ASSERT_NE(sessionId, sessionIdFromAnotherName);
    ASSERT_NE(sessionId, sessionIdFromAnotherSystem);
}

TEST(SessionId, generationIsCaseInsensitive)
{
    const SessionId sessionId(kExampleSystemLocalId, kExampleSystemId, kExampleUserName);
    const SessionId sessionIdFromLowerCase(
        kExampleSystemLocalId, kExampleSystemId, kExampleUserName.toLower());
    const SessionId sessionIdFromUpperCase(
        kExampleSystemLocalId, kExampleSystemId, kExampleUserName.toUpper());

    ASSERT_EQ(sessionId, sessionIdFromLowerCase);
    ASSERT_EQ(sessionId, sessionIdFromUpperCase);
}

TEST(SessionId, stringRepresentationContainsOnlyUrlAllowedSymbols)
{
    static const QString kAllowedPathSymbols = "-_=";

    const SessionId sessionId(kExampleSystemLocalId, kExampleSystemId, kExampleUserName);
    const auto stringRepresentation = sessionId.toQString();

    for (const QChar& symbol: stringRepresentation)
        ASSERT_TRUE(symbol.isLetterOrNumber() || kAllowedPathSymbols.contains(symbol));
}

TEST(SessionId, serializationIsTwoWay)
{
    const SessionId sessionId(kExampleSystemLocalId, kExampleSystemId, kExampleUserName);
    const QByteArray serialized = sessionId.serialized();
    const SessionId deserialized = SessionId::deserialized(serialized);

    ASSERT_EQ(sessionId, deserialized);
}

} // namespace test
} // namespace nx::vms::client::desktop
