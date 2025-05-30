// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores sound file name, displayed as combobox with button. */
class NX_VMS_RULES_API SoundField: public SimpleTypeActionField<QString, SoundField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "sound")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<QString, SoundField>::SimpleTypeActionField;
    static QJsonObject openApiDescriptor(const QVariantMap& properties)
    {
        auto descriptor = SimpleTypeField::openApiDescriptor(properties);
        descriptor[utils::kDescriptionProperty] = "Notification sound file name.";
        descriptor[utils::kExampleProperty] = "bycyclebell.mp3";
        return descriptor;
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
