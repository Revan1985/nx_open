// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../simple_type_field.h"

namespace nx::vms::rules {

/** Stores HTTP content type as a string. */
class NX_VMS_RULES_API ContentTypeField: public SimpleTypeActionField<QString>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.contentType")

    Q_PROPERTY(QString value READ value WRITE setValue)

public:
    ContentTypeField() = default;
};

} // namespace nx::vms::rules
