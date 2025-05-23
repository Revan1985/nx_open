// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_tooltip.h"

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickTextDocument>
#include <QtWidgets/QVBoxLayout>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/core/image_providers/image_provider.h>
#include <nx/vms/client/core/thumbnails/generic_image_store.h>
#include <nx/vms/client/core/thumbnails/thumbnail_image_provider.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>
#include <nx/vms/client/desktop/ui/right_panel/models/right_panel_models_adapter.h>
#include <nx/vms/client/desktop/utils/qml_property.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::client::core;

struct ThumbnailTooltip::Private
{
    ThumbnailTooltip* const q;

    QPointer<ImageProvider> imageProvider;
    nx::utils::ScopedConnections providerConnections;

    const QmlProperty<QString> text{q->widget(), "text"};
    const QmlProperty<QSize> maximumContentSize{q->widget(), "maximumContentSize"};
    const QmlProperty<bool> thumbnailVisible{q->widget(), "thumbnailVisible"};
    const QmlProperty<QRectF> thumbnailHighlightRect{q->widget(), "thumbnailHighlightRect"};
    const QmlProperty<qreal> previewAspectRatio{q->widget(), "previewAspectRatio"};
    const QmlProperty<QUrl> previewSource{q->widget(), "previewSource"};
    const QmlProperty<int> previewState{q->widget(), "previewState"};
    const QmlProperty<QVariantList> attributes{q->widget(), "attributes"};

    QString previewImageId;
    core::analytics::AttributeList sourceAttributes;
    QRectF desiredHighlightRect;
    bool forceNoHighlight = false;

    ~Private()
    {
        if (previewImageId.isEmpty())
            return;

        imageStore()->removeImage(previewImageId);
        previewSource = QString();
    }

    void updatePreviewAspectRatio()
    {
        static constexpr qreal kDefaultSizeHint = 16.0 / 9.0;
        const QSizeF sizeHint = imageProvider ? imageProvider->sizeHint() : QSize();

        previewAspectRatio = sizeHint.isValid()
            ? (sizeHint.width() / sizeHint.height())
            : kDefaultSizeHint;
    }

    void updatePreviewImage()
    {
        if (!previewImageId.isEmpty())
            imageStore()->removeImage(previewImageId);

        const QImage image = imageProvider ? imageProvider->image() : QImage();

        forceNoHighlight = QnLexical::deserialized<bool>(image.text(
            CameraThumbnailProvider::kFrameFromPluginKey));

        updateHighlightRect();

        previewImageId = image.isNull()
            ? QString()
            : imageStore()->addImage(image);

        previewSource = previewImageId.isEmpty()
            ? QUrl()
            : imageStore()->makeUrl(previewImageId);
    }

    void updatePreviewState()
    {
        previewState = (int) calculatePreviewState();
    }

    void updateHighlightRect()
    {
        thumbnailHighlightRect = forceNoHighlight ? QRectF() : desiredHighlightRect;
    }

    EventSearch::PreviewState calculatePreviewState() const
    {
        if (!imageProvider)
            return EventSearch::PreviewState::initial;

        switch (imageProvider->status())
        {
            case ThumbnailStatus::Invalid:
                return EventSearch::PreviewState::initial;

            case ThumbnailStatus::Loading:
            case ThumbnailStatus::Refreshing:
                return EventSearch::PreviewState::busy;

            case ThumbnailStatus::Loaded:
                return EventSearch::PreviewState::ready;

            case ThumbnailStatus::NoData:
                return EventSearch::PreviewState::missing;
        }

        NX_ASSERT(false, "Should never get here");
        return EventSearch::PreviewState::initial;
    }

    GenericImageStore* imageStore()
    {
        static GenericImageStore store(ThumbnailImageProvider::instance());
        return &store;
    }
};

ThumbnailTooltip::ThumbnailTooltip(WindowContext* context):
    BubbleToolTip(context, QUrl("qrc:/qml/Nx/RightPanel/private/ThumbnailTooltip.qml")),
    d(new Private{this})
{
    constexpr auto kMaxTooltipWidth = 252;
    QmlProperty<qreal>{widget(), "maximumWidth"} = kMaxTooltipWidth;
}

ThumbnailTooltip::~ThumbnailTooltip()
{
    // Required here for forward-declared scoped pointer destruction.
}

void ThumbnailTooltip::setMaximumContentSize(const QSize& size)
{
    d->maximumContentSize = size;
}

void ThumbnailTooltip::adjustMaximumContentSize(const QModelIndex& index)
{
    constexpr int kPaddings = 16;

    QSize size = index.data(Qn::MaximumTooltipSizeRole).value<QSize>();
    if (size.isEmpty())
        size = QSize(mainWindowWidget()->width() / 2, mainWindowWidget()->height() - kPaddings);

    setMaximumContentSize(size);
}

void ThumbnailTooltip::setImageProvider(ImageProvider* value)
{
    if (d->imageProvider == value)
        return;

    d->providerConnections.reset();
    d->imageProvider = value;

    const auto updatePreview =
        [this]()
        {
            d->updatePreviewAspectRatio();
            d->updatePreviewImage();
            d->updatePreviewState();
            d->thumbnailVisible = (bool) d->imageProvider;
        };

    updatePreview();

    if (!d->imageProvider)
        return;

    d->providerConnections << connect(d->imageProvider.data(), &ImageProvider::sizeHintChanged,
        this, [this]() { d->updatePreviewAspectRatio(); });

    d->providerConnections << connect(d->imageProvider.data(), &ImageProvider::imageChanged,
        this, [this]() { d->updatePreviewImage(); });

    d->providerConnections << connect(d->imageProvider.data(), &ImageProvider::statusChanged,
        this, [this]() { d->updatePreviewState(); });

    d->providerConnections << connect(d->imageProvider.data(), &QObject::destroyed,
        this, updatePreview);
}

void ThumbnailTooltip::setHighlightRect(const QRectF& rect)
{
    d->desiredHighlightRect = rect;
    d->updateHighlightRect();
}

const core::analytics::AttributeList& ThumbnailTooltip::attributes() const
{
    return d->sourceAttributes;
}

void ThumbnailTooltip::setAttributes(const core::analytics::AttributeList& value)
{
    if (d->sourceAttributes == value)
        return;

    d->sourceAttributes = value;
    d->attributes = nx::utils::toQVariantList(d->sourceAttributes);
}

} // namespace nx::vms::client::desktop
