// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringListModel>

#include <nx/utils/uuid.h>

class AbstractSystemsController;
class QnSystemsModelPrivate;

class NX_VMS_CLIENT_CORE_API QnSystemsModel: public QAbstractListModel
{
    Q_OBJECT

    using base_type = QAbstractListModel;

public:
    enum RoleId
    {
        FirstRoleId = Qt::UserRole + 1,

        SearchRoleId = FirstRoleId,
        SystemNameRoleId,
        SystemIdRoleId,
        LocalIdRoleId,

        OwnerDescriptionRoleId,

        IsFactorySystemRoleId,

        IsCloudSystemRoleId,
        IsOnlineRoleId,
        IsReachableRoleId,
        IsCompatibleToMobileClient,
        IsCompatibleToDesktopClient,

        WrongCustomizationRoleId,
        WrongVersionRoleId,
        CompatibleVersionRoleId,

        VisibilityScopeRoleId,

        IsCloudOauthSupportedRoleId,
        Is2FaEnabledForSystem,

        IsSaasSuspended,
        IsSaasShutDown,
        IsSaasUninitialized,

        IsPending,

        RolesCount
    };

    static const QHash<int, QByteArray> kRoleNames;

public:
    QnSystemsModel(AbstractSystemsController* controller, QObject* parent = nullptr);
    virtual ~QnSystemsModel();

public: // overrides
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override { return kRoleNames; }

    int getRowIndex(const QString& systemId) const;

    // TODO: cloudFirstSorting would always be true when the desktop client switches to the new
    // welcome screen with organizations.
    static bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight,
        bool cloudFirstSorting = false);

private:
    QScopedPointer<QnSystemsModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnSystemsModel)
};
