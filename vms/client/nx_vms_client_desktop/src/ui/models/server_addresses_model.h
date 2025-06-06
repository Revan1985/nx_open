// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/url.h>

class QnServerAddressesModel: public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef QAbstractItemModel base_type;

public:
    enum {
        AddressColumn = 0,
        InUseColumn,
        ColumnCount
    };

    enum EditingError {
        InvalidUrl,
        ExistingUrl
    };

    explicit QnServerAddressesModel(QObject *parent = 0);

    void setPort(int port);
    int port() const;

    void setAddressList(const QList<nx::Url> &addresses);
    QList<nx::Url> addressList() const;

    void setManualAddressList(const QList<nx::Url> &addresses);
    QList<nx::Url> manualAddressList() const;

    void setIgnoredAddresses(const QSet<nx::Url> &ignoredAddresses);
    QSet<nx::Url> ignoredAddresses() const;

    void addAddress(const nx::Url &url, bool isManualAddress = true);
    void removeAddressAtIndex(const QModelIndex &index);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void clear();
    void resetModel(const QList<nx::Url> &addresses, const QList<nx::Url> &manualAddresses, const QSet<nx::Url> &ignoredAddresses, int port);

    bool isManualAddress(const QModelIndex &index) const;

    // Reimplemented from QAbstractItemModel
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

signals:
    void urlEditingFailed(const QModelIndex &index, int error);

private:
    nx::Url addressAtIndex(const QModelIndex &index, int defaultPort = -1) const;

private:
    bool m_readOnly;
    int m_port;
    QList<nx::Url> m_addresses;
    QList<nx::Url> m_manualAddresses;
    QSet<nx::Url> m_ignoredAddresses;
};

class QnSortedServerAddressesModel : public QSortFilterProxyModel {
public:
    explicit QnSortedServerAddressesModel(QObject *parent = 0);

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};
