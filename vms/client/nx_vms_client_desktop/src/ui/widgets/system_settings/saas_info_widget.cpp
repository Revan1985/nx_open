// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_info_widget.h"
#include "ui_saas_info_widget.h"

#include <memory>

#include <QtGui/QDesktopServices>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/saas/services_sort_model.h>
#include <nx/vms/client/desktop/saas/services_usage_item_delegate.h>
#include <nx/vms/client/desktop/saas/services_usage_model.h>
#include <nx/vms/client/desktop/saas/tier_usage_item_delegate.h>
#include <nx/vms/client/desktop/saas/tier_usage_model.h>
#include <nx/vms/client/desktop/saas/tier_usage_sort_model.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/license/saas/saas_service_usage_helper.h>
#include <ui/common/palette.h>

namespace {

static constexpr QSize kPlaceholderImageSize(64, 64);

static constexpr auto kSaasStateLabelFontSize = 18;
static constexpr auto kSaasStateLabelFontWeight = QFont::Medium;

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kDark17Theme = {
    {QnIcon::Normal, {.primary = "dark17"}}};

NX_DECLARE_COLORIZED_ICON(kNoServicesPlaceholder, "64x64/Outline/noservices.svg", kDark17Theme)

QFont placeholderMessageCaptionFont()
{
    static constexpr auto kPlaceholderMessageCaptionFontSize = 16;
    static constexpr auto kPlaceholderMessageCaptionFontWeight = QFont::Medium;

    QFont font;
    font.setPixelSize(kPlaceholderMessageCaptionFontSize);
    font.setWeight(kPlaceholderMessageCaptionFontWeight);
    return font;
}

QFont placeholderMessageFont()
{
    static constexpr auto kPlaceholderMessageFontSize = 14;
    static constexpr auto kPlaceholderMessageFontWeight = QFont::Normal;

    QFont font;
    font.setPixelSize(kPlaceholderMessageFontSize);
    font.setWeight(kPlaceholderMessageFontWeight);
    return font;
}

bool hasServices(const nx::vms::common::saas::ServiceManager* serviceManager)
{
    if (!NX_ASSERT(serviceManager))
        return false;

    for (const auto& [_, saasPurchase]: serviceManager->data().services)
    {
        if (saasPurchase.used > 0 || saasPurchase.quantity > 0)
            return true;
    }

    return false;
}

QUrl channelPartnerUrl(const nx::vms::common::saas::ServiceManager* serviceManager)
{
    const auto webPagesInfo = serviceManager->data().channelPartner.supportInformation.sites;
    return !webPagesInfo.empty()
        ? QUrl::fromUserInput(webPagesInfo.front().value)
        : QUrl();
}

} // namespace

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// SaasInfoWidget::Private declaration.

struct SaasInfoWidget::Private
{
    SaasInfoWidget* const q;
    std::unique_ptr<Ui::SaasInfoWidget> ui = std::make_unique<Ui::SaasInfoWidget>();

    std::unique_ptr<saas::ServicesUsageModel> servicesUsageModel =
        std::make_unique<saas::ServicesUsageModel>(q->systemContext()->saasServiceManager());

    std::unique_ptr<saas::TierUsageModel> tierUsageModel =
        std::make_unique<saas::TierUsageModel>(q->systemContext()->saasServiceManager());

    std::unique_ptr<saas::ServicesSortModel> servicesSortModel =
        std::make_unique<saas::ServicesSortModel>();

    std::unique_ptr<saas::TierUsageSortModel> tierUsageSortModel =
        std::make_unique<saas::TierUsageSortModel>();

    void setupHeaderUi();
    void setupPlaceholderPageUi();
    void setupServicesUsagePageUi();
    void setupServicesViewHeader();
    void setupTiersViewHeader();

    void updateUi();
};

//-------------------------------------------------------------------------------------------------
// SaasInfoWidget::Private definition.

void SaasInfoWidget::Private::setupHeaderUi()
{
    ui->headerWidget->setContentsMargins(nx::style::Metrics::kDefaultTopLevelMargins);

    const QColor saasStateValueLabelColor(
        qApp->palette().color(QPalette::Normal, QPalette::BrightText));
    setPaletteColor(ui->saasStateValueLabel, QPalette::WindowText, saasStateValueLabelColor);

    QFont saasStateLabelFont;
    saasStateLabelFont.setPixelSize(kSaasStateLabelFontSize);
    saasStateLabelFont.setWeight(kSaasStateLabelFontWeight);
    ui->saasStateValueLabel->setFont(saasStateLabelFont);
}

void SaasInfoWidget::Private::setupPlaceholderPageUi()
{
    const auto placeholderPixmap =
        qnSkin->icon(kNoServicesPlaceholder).pixmap(kPlaceholderImageSize);
    ui->placeholderIconLabel->setPixmap(qnSkin->maximumSizePixmap(placeholderPixmap));
    ui->placeholderMessageCaptionLabel->setFont(placeholderMessageCaptionFont());
    ui->placeholderMessageLabel->setFont(placeholderMessageFont());
    q->connect(ui->channelPartnerContactButton, &QPushButton::clicked, q,
        [this]
        {
            QDesktopServices::openUrl(channelPartnerUrl(q->systemContext()->saasServiceManager()));
        });
}

void SaasInfoWidget::Private::setupServicesUsagePageUi()
{
    ui->servicesContentsWidget->setContentsMargins(nx::style::Metrics::kDefaultTopLevelMargins);

    servicesSortModel->setSourceModel(servicesUsageModel.get());
    servicesSortModel->sort(saas::ServicesUsageModel::ServiceNameColumn);
    ui->servicesUsageItemView->setModel(servicesSortModel.get());
    ui->servicesUsageItemView->setItemDelegate(
        new saas::ServicesUsageItemDelegate(ui->servicesUsageItemView));

    tierUsageSortModel->setSourceModel(tierUsageModel.get());
    tierUsageSortModel->sort(saas::TierUsageModel::LimitTypeColumn);
    ui->tierUsageItemView->setModel(tierUsageSortModel.get());
    ui->tierUsageItemView->setItemDelegate(
        new saas::TierUsageItemDelegate(ui->tierUsageItemView));
    ui->tierUsageItemView->expandAll();

    setupServicesViewHeader();
    setupTiersViewHeader();
}

void SaasInfoWidget::Private::setupServicesViewHeader()
{
    const auto servicesViewHeader = ui->servicesUsageItemView->header();
    servicesViewHeader->setSectionsMovable(false);
    servicesViewHeader->setSectionResizeMode(
        saas::ServicesUsageModel::ServiceNameColumn, QHeaderView::Stretch);
    servicesViewHeader->setSectionResizeMode(
        saas::ServicesUsageModel::ServiceTypeColumn, QHeaderView::Stretch);
    servicesViewHeader->setSectionResizeMode(
        saas::ServicesUsageModel::ServiceOveruseWarningIconColumn, QHeaderView::ResizeToContents);
    servicesViewHeader->setSectionResizeMode(
        saas::ServicesUsageModel::TotalQantityColumn, QHeaderView::ResizeToContents);
    servicesViewHeader->setSectionResizeMode(
        saas::ServicesUsageModel::UsedQantityColumn, QHeaderView::ResizeToContents);
}

void SaasInfoWidget::Private::setupTiersViewHeader()
{
    const auto tiersViewHeader = ui->tierUsageItemView->header();
    if (tiersViewHeader->count() == 0)
        return;

    tiersViewHeader->setSectionsMovable(false);
    tiersViewHeader->setSectionResizeMode(
        saas::TierUsageModel::LimitTypeColumn, QHeaderView::Stretch);
    tiersViewHeader->setSectionResizeMode(
        saas::TierUsageModel::AllowedLimitColumn, QHeaderView::ResizeToContents);
    tiersViewHeader->setSectionResizeMode(
        saas::TierUsageModel::UsedLimitColumn, QHeaderView::ResizeToContents);
}

void SaasInfoWidget::Private::updateUi()
{
    const auto serviceManager = q->systemContext()->saasServiceManager();

    ui->channelPartnerContactButton->setHidden(!channelPartnerUrl(serviceManager).isValid());
    ui->stackedWidget->setCurrentWidget(
        hasServices(serviceManager) || serviceManager->hasTierOveruse()
            ? ui->servicesUsagePage
            : ui->placeholderPage);

    ui->servicesUsageGroupBox->setVisible(hasServices(serviceManager));
    ui->tierUsageGroupBox->setVisible(serviceManager->hasTierOveruse());

    QString saasStateLabelText;
    if (serviceManager->saasActive())
        saasStateLabelText = tr("Active");
    else if (serviceManager->saasSuspended())
        saasStateLabelText = tr("Suspended");
    else if (serviceManager->saasShutDown())
        saasStateLabelText = tr("Shut down");

    ui->saasStateValueLabel->setText(saasStateLabelText);
}

//-------------------------------------------------------------------------------------------------
// ServicesWidget definition.

SaasInfoWidget::SaasInfoWidget(SystemContext* systemContext, QWidget* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private({this}))
{
    d->ui->setupUi(this);
    d->setupHeaderUi();
    d->setupPlaceholderPageUi();
    d->setupServicesUsagePageUi();

    connect(systemContext->saasServiceManager(),
        &nx::vms::common::saas::ServiceManager::dataChanged,
        this,
        &SaasInfoWidget::loadDataToUi);

    connect(d->tierUsageModel.get(), &saas::TierUsageModel::modelDataUpdated, this,
        [this]
        {
            d->setupTiersViewHeader();
            d->ui->tierUsageItemView->expandAll();
            const auto serviceManager = this->systemContext()->saasServiceManager();
            d->ui->tierUsageGroupBox->setVisible(serviceManager->hasTierOveruse());
        });
}

SaasInfoWidget::~SaasInfoWidget()
{
}

void SaasInfoWidget::loadDataToUi()
{
    d->updateUi();
}

void SaasInfoWidget::showEvent(QShowEvent*)
{
    d->servicesUsageModel->setCamerasChangesTracking(systemContext(), true);
    d->tierUsageModel->setResourcesChangesTracking(systemContext(), true);
}

void SaasInfoWidget::hideEvent(QHideEvent*)
{
    d->servicesUsageModel->setCamerasChangesTracking(systemContext(), false);
    d->tierUsageModel->setResourcesChangesTracking(systemContext(), false);
}

} // namespace nx::vms::client::desktop
