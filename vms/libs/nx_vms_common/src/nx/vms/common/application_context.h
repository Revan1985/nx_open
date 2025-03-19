// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/connection_types.h>

class QnLongRunableCleanup;
class QnStoragePluginFactory;

namespace nx::metric { struct ApplicationMetricsStorage; }
namespace nx::i18n { class TranslationManager; }

namespace nx::vms::common {

/**
 * Main context of the VMS applications. Exists through all application lifetime and is accessible
 * from anywhere using `instance()` method.
 *
 * Works as a storage for the most basic and generic singletons, needed in all common projects.
 * Initializes networking and ffmpeg library, stores pool for all long-runnable threads, keeps the
 * single synchronized time source.
 */
class NX_VMS_COMMON_API ApplicationContext: public QObject
{
protected:
    using PeerType = nx::vms::api::PeerType;

public:
    enum class FeatureFlag
    {
        none = 0,
        networking = 1 << 0,
        translations = 1 << 1,

        all = -1
    };
    Q_DECLARE_FLAGS(FeatureFlags, FeatureFlag)

    struct Features
    {
        FeatureFlags flags = FeatureFlag::none;

        Features() = default;
        Features(FeatureFlags flags): flags(flags){}

        Features& withFlag(FeatureFlag flag)
        {
            flags.setFlag(flag);
            return *this;
        }

        static Features all() { return {FeatureFlag::all}; }
        static Features none() { return {FeatureFlag::none}; }
    };

    ApplicationContext(
        PeerType localPeerType = PeerType::notDefined,
        const QString& customCloudHost = QString{},
        Features features = Features::all(),
        QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    const Features& commonFeatures() const;

    /**
     * Main context of the VMS applications. Exists through all application lifetime.
     */
    static ApplicationContext* instance();

    PeerType localPeerType() const;

    /** Application language locale code (in form `en_US`). */
    QString locale() const;

    /** Set application language locale code (in form `en_US`). */
    void setLocale(const QString& value);

    /** Application-wide translation manager. */
    nx::i18n::TranslationManager* translationManager() const;

    void setModuleShortId(const nx::Uuid& id, int number);
    int moduleShortId(const nx::Uuid& id) const;
    QString moduleDisplayName(const nx::Uuid& id) const;

    QnStoragePluginFactory* storagePluginFactory() const;
    QnLongRunableCleanup* longRunableCleanup() const;
    nx::metric::ApplicationMetricsStorage* metrics() const;

    virtual bool isCertificateValidationLevelStrict() const;

    template<typename ContextType>
    ContextType* as()
    {
        return qobject_cast<ContextType*>(this);
    }

protected:
    void stopAll();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

inline ApplicationContext* appContext() { return ApplicationContext::instance(); }

} // namespace nx::vms::common
