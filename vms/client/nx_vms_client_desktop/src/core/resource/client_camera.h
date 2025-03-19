// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/camera.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

class QnArchiveStreamReader;

class NX_VMS_CLIENT_DESKTOP_API QnClientCameraResource:
    public nx::vms::client::core::Camera
{
    Q_OBJECT
    using base_type = nx::vms::client::core::Camera;

public:
    explicit QnClientCameraResource(const nx::Uuid& resourceTypeId);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) override;
    virtual AudioLayoutConstPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) const override;

    virtual Qn::ResourceFlags flags() const override;

    static void setAuthToCameraGroup(
        const QnVirtualCameraResourcePtr& camera,
        const QAuthenticator& authenticator);

    QnAbstractStreamDataProvider* createDataProvider(Qn::ConnectionRole role);

    /**
     * Debug log representation. Used by toString(const T*).
     */
    virtual QString idForToStringFromPtr() const override;

    /**
    * Whether client should automatically send PTZ Stop command when camera loses focus.
    * Enabled by default, can be disabled by setting a special resource property.
    */
    bool autoSendPtzStopCommand() const;
    void setAutoSendPtzStopCommand(bool value);

signals:
    void dataDropped();
    void footageAdded();

protected slots:
    virtual void resetCachedValues() override;

private:
    Qn::ResourceFlags calculateFlags() const;
private:
    mutable std::atomic<Qn::ResourceFlags> m_cachedFlags{};
};
