// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/api/types/ptz_types.h>
#include <nx/vms/common/ptz/coordinate_space.h>
#include <nx/vms/common/ptz/vector.h>

#include "ptz_fwd.h"

static const QString kPresetsPropertyKey = "ptzPresets";
static const QString kPtzPresetMappingPropertyName = "presetMapping";

namespace nx::core::ptz {

using PresetType = nx::vms::api::ptz::PresetType;

} // namespace nx::core::ptz


struct NX_VMS_COMMON_API QnPtzPreset
{
    Q_GADGET
public:
    QnPtzPreset() {}
    QnPtzPreset(const QString& id, const QString& name):
        id(id),
        name(name)
    {
    }

    QString id;
    QString name;

    bool operator==(const QnPtzPreset& other) const = default;
};

NX_REFLECTION_INSTRUMENT(QnPtzPreset, (id)(name))
#define QnPtzPreset_Fields (id)(name)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzPreset, (json), NX_VMS_COMMON_API);

struct QnPtzPresetData
{
    QnPtzPresetData(): space(nx::vms::common::ptz::CoordinateSpace::device) {}

    bool operator==(const QnPtzPresetData& other) const = default;

    nx::vms::common::ptz::Vector position;
    nx::vms::common::ptz::CoordinateSpace space;
};

#define QnPtzPresetData_Fields (position)(space)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzPresetData, (json));

struct QnPtzPresetRecord
{
    QnPtzPresetRecord() {}
    QnPtzPresetRecord(
        const QnPtzPreset &preset,
        const QnPtzPresetData &data)
        :
        preset(preset),
        data(data)
    {
    }

    bool operator==(const QnPtzPresetRecord& other) const = default;

    QnPtzPreset preset;
    QnPtzPresetData data;
};

#define QnPtzPresetRecord_Fields (preset)(data)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzPresetRecord, (json), NX_VMS_COMMON_API);

using QnPtzPresetRecordHash = QHash<QString, QnPtzPresetRecord>;

using QnPtzPresetMapping = QMap<QString, QnPtzPreset>;
