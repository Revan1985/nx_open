// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QThreadPool>

#include <ui/graphics/items/generic/viewport_bound_widget.h>

class QLabel;
class QPushButton;
class QSpacerItem;

namespace nx::vms::client::desktop { class BusyIndicatorGraphicsWidget; }

class QnStatusOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    QnStatusOverlayWidget(QGraphicsWidget *parent = nullptr);

    enum class Control
    {
        kNoControl      = 0x00,
        kPreloader      = 0x01,
        kImageOverlay   = 0x02,
        kIcon           = 0x04,
        kCaption        = 0x08,
        kButton         = 0x10,
        kCustomButton   = 0x20,
    };
    Q_DECLARE_FLAGS(Controls, Control);

    enum class ErrorStyle
    {
        red,
        white,
    };

    virtual void paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    void setVisibleControls(Controls controls);

    void setIconOverlayPixmap(const QPixmap& pixmap);

    void setIcon(const QPixmap& pixmap);
    void setUseErrorStyle(bool useErrorStyle);
    void setErrorStyle(ErrorStyle errorStyle);
    void setCaption(const QString& caption);
    void setButtonText(const QString& text);
    void setCustomButtonText(const QString& text);
    void setTooltip(const QString& tooltip);
    void setShowGlow(bool showGlow);

    QString tooltip();

    static void generateBackgrounds();

signals:
    void actionButtonClicked();
    void customButtonClicked();
    void closeButtonClicked();
    void tooltipUpdated(const QPoint& pos = {});

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    virtual bool eventFilter(QObject* obj, QEvent* ev) override;

    void resetPreloader();

private:
    void setupPreloader();
    void setupCentralControls();
    void setupExtrasControls();
    void initializeHandlers();
    void updateAreaSizes();
    void updateTooltip();
    QPixmap getBackgroundPixmap();
    QPixmap whiteGlowHorizontal();
    QPixmap whiteGlowVertical();
    QPixmap redGlowHorizontal();
    QPixmap redGlowVertical();

private:
    Controls m_visibleControls = Control::kNoControl;
    bool m_initialized = false;
    bool m_captionVisible = false;
    bool m_useErrorStyle = false;
    ErrorStyle m_errorStyle = ErrorStyle::red;
    bool m_showGlow = false;
    bool m_hovered = false;

    QnViewportBoundWidget* const m_preloaderHolder;
    QnViewportBoundWidget* const m_centralHolder;
    QnViewportBoundWidget* const m_extrasHolder;

    QWidget* const m_centralContainer;
    QWidget* const m_extrasContainer;

    // Preloader
    nx::vms::client::desktop::BusyIndicatorGraphicsWidget* m_preloader;

    // Icon overlay
    QGraphicsPixmapItem m_imageItem;

    // Central area
    QLabel* const m_centralAreaImage;
    QLabel* const m_caption;
    QPushButton* const m_button;
    QSpacerItem* const m_postIconSpacer;
    QSpacerItem* const m_postCaptionSpacer;

    // Extras
    QPushButton* const m_customButton;

    QString m_tooltip;

    static QThreadPool s_threadPool;

    static QImage s_whiteGlowHorizontalImage;
    static QImage s_whiteGlowVerticalImage;
    static QImage s_redGlowHorizontalImage;
    static QImage s_redGlowVerticalImage;

    QPixmap m_whiteGlowHorizontalPixmap;
    QPixmap m_whiteGlowVerticalPixmap;
    QPixmap m_redGlowHorizontalPixmap;
    QPixmap m_redGlowVerticalPixmap;
};
