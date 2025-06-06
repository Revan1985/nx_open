// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Builds sound volume float value from 0 to 1. Displayed by slider. */
class NX_VMS_RULES_API VolumeField: public SimpleTypeActionField<float, VolumeField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "volume")

    Q_PROPERTY(float value READ value WRITE setValue NOTIFY valueChanged)

    static constexpr auto kDefaultVolume = 0.8f;

public:
    explicit VolumeField(const FieldDescriptor* descriptor):
        SimpleTypeActionField<float, VolumeField>{descriptor}
    {
        setValue(kDefaultVolume);
    };

    static QJsonObject openApiDescriptor(const QVariantMap& properties)
    {
        auto descriptor = SimpleTypeField::openApiDescriptor(properties);
        descriptor[utils::kMinProperty] = 0.0f;
        // TODO: #vbutkevich probably default value must be set in manifest for each action.
        descriptor[utils::kDefaultProperty] = kDefaultVolume;
        descriptor[utils::kMaxProperty] = 1.0f;
        descriptor[utils::kExampleProperty] = 0.5f;
        return descriptor;
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
