// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sign_dialog.h"
#include "ui_sign_dialog.h"

#include <chrono>

#include <QtOpenGLWidgets/QOpenGLWidget>

#include <camera/cam_display.h>
#include <camera/gl_renderer.h>
#include <camera/sign_dialog_display.h>
#include <core/resource/avi/avi_resource.h>
#include <export/sign_helper.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/widgets/main_window.h>
#include <ui/workaround/gl_native_painting.h>
#include <utils/common/event_processors.h>

using namespace nx::vms::client::desktop;
using namespace std::chrono;

class QnSignDialogVideoOutput
{
public:
    static constexpr std::chrono::milliseconds kUpdateInterval = 16ms;
    static constexpr int kDefaultWidth = 1920;
    static constexpr int kDefaultHeight = 1080;

    virtual ~QnSignDialogVideoOutput() = default;
    virtual QWidget* getWidget() = 0;
    virtual void setRenderer(QnResourceWidgetRenderer* renderer) = 0;
    virtual void setImageSize(int width, int height) = 0;
};

// TODO: #sivanov Rewrite to QML.
class QnSignDialogGlWidget:
    public QOpenGLWidget,
    public QnSignDialogVideoOutput
{
public:
    QnSignDialogGlWidget(
        QWidget* parent = nullptr,
        Qt::WindowFlags windowFlags = {})
        :
        QOpenGLWidget(parent, windowFlags)
    {
        connect(&m_timer, &QTimer::timeout, this, [this]{ update(); });
        m_timer.start(kUpdateInterval);
        m_textureWidth = kDefaultWidth;
        m_textureHeight = kDefaultHeight;
    }

    QWidget* getWidget() override
    {
        return this;
    }

    void setImageSize(int width, int height) override
    {
        m_textureWidth = width;
        m_textureHeight = height;
        resizeEvent(0);
    }

    virtual void resizeEvent(QResizeEvent* /*event*/) override
    {
        m_videoRect = SignDialog::calcVideoRect(width(), height(), m_textureWidth, m_textureHeight);
        if (m_renderer)
            m_renderer->setChannelScreenSize(m_videoRect.size());
        update();
    }

    void setRenderer(QnResourceWidgetRenderer* renderer) override
    {
        m_renderer = renderer;
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        QnGlNativePainting::begin(this, &painter);
        if (m_renderer)
            m_renderer->paint(&painter, 0, QRectF(0.0, 0.0, 1.0, 1.0), m_videoRect, 1.0);
        QnGlNativePainting::end(&painter);
    }

private:
    QRect m_videoRect;
    QnResourceWidgetRenderer* m_renderer = nullptr;
    QTimer m_timer;
    double m_textureWidth;
    double m_textureHeight;
};

class QnSignDialogSWWidget:
    public QWidget,
    public QnSignDialogVideoOutput
{
public:
    QnSignDialogSWWidget(
        QWidget* parent = nullptr,
        Qt::WindowFlags windowFlags = {})
        :
        QWidget(parent, windowFlags)
    {
        connect(&m_timer, &QTimer::timeout, this, [this]{ update(); });
        m_timer.start(kUpdateInterval);
        m_textureWidth = kDefaultWidth;
        m_textureHeight = kDefaultHeight;
    }

    QWidget* getWidget() override
    {
        return this;
    }

    void setImageSize(int width, int height) override
    {
        m_textureWidth = width;
        m_textureHeight = height;
        resizeEvent(0);
    }

    virtual void resizeEvent(QResizeEvent* /*event*/) override
    {
        m_videoRect = SignDialog::calcVideoRect(width(), height(), m_textureWidth, m_textureHeight);
        if (m_renderer)
            m_renderer->setChannelScreenSize(m_videoRect.size());
        update();
    }

    void setRenderer(QnResourceWidgetRenderer* renderer) override
    {
        m_renderer = renderer;
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        if (m_renderer)
            m_renderer->paint(&painter, 0, QRectF(0.0, 0.0, 1.0, 1.0), m_videoRect, 1.0);
    }

private:
    QRect m_videoRect;
    QnResourceWidgetRenderer* m_renderer = nullptr;
    QTimer m_timer;
    double m_textureWidth;
    double m_textureHeight;
};

// ------------------------------------

SignDialog::SignDialog(QnResourcePtr checkResource, QWidget* parent):
    base_type(parent),
    ui(new Ui::SignDialog)
{
    ui->setupUi(this);

    setHelpTopic(this, HelpTopic::Id::Watermark);

    m_layout = new QVBoxLayout(ui->videoSpacer);
    m_layout->setSpacing(0);
    m_layout->setContentsMargins(0, 0, 0, 0);

    if (auto mainWindow = qobject_cast<MainWindow*>(parent))
    {
        if (auto gl = qobject_cast<QOpenGLWidget*>(mainWindow->viewport()); !gl)
            m_videoOutputWidget.reset(new QnSignDialogSWWidget(this));
    }

    if (!m_videoOutputWidget)
        m_videoOutputWidget.reset(new QnSignDialogGlWidget(this));
    m_layout->addWidget(m_videoOutputWidget->getWidget());

    m_srcVideoInfo = new QnSignInfo(this);
    m_srcVideoInfo->setHidden(true);

    m_resource = QnAviResourcePtr(new QnAviResource(checkResource->getUrl()));

    // Pass storage to keep password (if any) from parent AviResource to the new one.
    if (auto parentAvi = checkResource.dynamicCast<QnAviResource>())
        m_resource->setStorage(parentAvi->getStorage());

    m_reader.reset(qobject_cast<QnAbstractArchiveStreamReader*>(
        m_resource->createDataProvider(Qn::CR_Default)));

    m_reader->setCycleMode(false);

    m_camDispay.reset(new QnSignDialogDisplay(m_resource));

    connect(m_camDispay.get(), &QnSignDialogDisplay::gotSignature, ui->signInfoLabel,
        &QnSignInfo::at_gotSignature);
    connect(m_camDispay.get(), &QnSignDialogDisplay::calcSignInProgress, ui->signInfoLabel,
        &QnSignInfo::at_calcSignInProgress);
    connect(m_camDispay.get(), &QnSignDialogDisplay::gotSignatureDescription, ui->signInfoLabel,
        &QnSignInfo::at_gotSignatureDescription);
    connect(m_camDispay.get(), &QnSignDialogDisplay::gotSignatureDescription, m_srcVideoInfo,
        &QnSignInfo::at_gotSignatureDescription);

    connect(m_camDispay.get(), &QnSignDialogDisplay::calcSignInProgress, this,
        &SignDialog::at_calcSignInProgress);
    connect(m_camDispay.get(), &QnSignDialogDisplay::gotSignature, this,
        &SignDialog::at_gotSignature);

    connect(m_camDispay.get(), &QnSignDialogDisplay::gotImageSize, this,
        &SignDialog::at_gotImageSize);

    const auto ensureInitialized =
        [this]()
        {
            if (m_renderer)
                return;

            m_renderer = new QnResourceWidgetRenderer(0, m_videoOutputWidget->getWidget());
            m_videoOutputWidget->setRenderer(m_renderer);
            m_camDispay->addVideoRenderer(1, m_renderer, true);
            m_reader->addDataProcessor(m_camDispay.data());
            m_reader->setSpeed(1024 * 1024);
            m_reader->start();
            m_camDispay->start();
        };

    installEventHandler(this, QEvent::Show, this, ensureInitialized);
}

SignDialog::~SignDialog()
{
    m_camDispay->removeVideoRenderer(m_renderer);
    m_renderer->destroyAsync();

    m_reader->pleaseStop();
    m_camDispay->pleaseStop();

    m_reader->stop();
    m_camDispay->stop();
    m_camDispay->clearUnprocessedData();

    m_reader->removeDataProcessor(m_camDispay.data());
}

QRect SignDialog::calcVideoRect(
    double windowWidth,
    double windowHeight,
    double textureWidth,
    double textureHeight)
{
    QRect videoRect;
    double newTextureWidth = static_cast<uint>(textureWidth);

    double windowAspect = windowWidth / windowHeight;
    double textureAspect = textureWidth / textureHeight;
    if (windowAspect > textureAspect)
    {
        // black bars at left and right
        videoRect.setTop(0);
        videoRect.setHeight(windowHeight);
        double scale = windowHeight / textureHeight;
        double scaledWidth = newTextureWidth * scale;
        videoRect.setLeft((windowWidth - scaledWidth) / 2);
        videoRect.setWidth(scaledWidth + 0.5);
    }
    else
    {
        // black bars at the top and bottom
        videoRect.setLeft(0);
        videoRect.setWidth(windowWidth);
        if (newTextureWidth < windowWidth)
        {
            double scale = windowWidth / newTextureWidth;
            double scaledHeight = textureHeight * scale;
            videoRect.setTop((windowHeight - scaledHeight) / 2);
            videoRect.setHeight(scaledHeight + 0.5);
        }
        else
        {
            double newTextureHeight = windowWidth / textureAspect + 0.5;
            videoRect.setTop((windowHeight - newTextureHeight) / 2);
            videoRect.setHeight(newTextureHeight);
        }
    }
    return videoRect;
}

void SignDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);

    switch (event->type())
    {
        case QEvent::LanguageChange:
            ui->retranslateUi(this);
            break;
        default:
            break;
    }
}

void SignDialog::at_calcSignInProgress(QByteArray /*sign*/, int progress)
{
    ui->progressBar->setValue(progress);
}

void SignDialog::at_gotImageSize(int width, int height)
{
    if (m_videoOutputWidget)
        m_videoOutputWidget->setImageSize(width, height);
    ui->signInfoLabel->setImageSize(width, height);
}

void SignDialog::at_gotSignature(
    QByteArray calculatedSign, QByteArray signFromFrame)
{
    m_layout->addWidget(m_srcVideoInfo);
    m_srcVideoInfo->setHidden(false);
    m_videoOutputWidget->getWidget()->hide();
    m_srcVideoInfo->setDrawDetailTextMode(true);

    // It is correct. Set parameters vise versa here.
    m_srcVideoInfo->at_gotSignature(calculatedSign, signFromFrame);
}
