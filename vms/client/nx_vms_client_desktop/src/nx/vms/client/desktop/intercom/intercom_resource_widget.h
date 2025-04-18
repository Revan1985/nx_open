// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>

#include <nx/utils/uuid.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

namespace nx::vms::client::desktop {

class IntercomResourceWidget: public QnMediaResourceWidget
{
    Q_OBJECT

    using base_type = QnMediaResourceWidget;

public:
    IntercomResourceWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::vms::client::desktop::WindowContext* windowContext,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);

    virtual ~IntercomResourceWidget() override;

protected:
    virtual int calculateButtonsVisibility() const override;
};

} // namespace nx::vms::client::desktop
