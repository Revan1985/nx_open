// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/crud_model.h>
#include <nx/utils/member_detector.h>
#include <nx/utils/type_traits.h>
#include <nx/vms/api/data/data_fwd.h>

namespace ec2::details {

#define MEMBER_CHECKER(MEMBER) NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(MEMBER##Exists, MEMBER)
MEMBER_CHECKER(fromDbTypes);
MEMBER_CHECKER(toDbTypes);
MEMBER_CHECKER(kResourceTypeId);
MEMBER_CHECKER(id);
#undef MEMBER_CHECKER

template<typename F, typename... Args, size_t... Is>
Result applyFuncImpl(std::tuple<Args...>&& t, F&& f, std::index_sequence<Is...>)
{
    Result result;
    (... && (result = f(std::get<sizeof...(Args) - Is - 1>(std::move(t)))));
    return result;
}

template<typename F, typename... Args>
Result applyFunc(std::tuple<Args...>&& t, F&& f)
{
    return applyFuncImpl(
        std::move(t), std::forward<F>(f), std::make_index_sequence<sizeof...(Args)>{});
}

template<typename Data, typename Functor>
auto invokeOnVector(Data&& data, Functor&& functor)
{
    if constexpr (nx::utils::Is<std::vector, std::decay_t<decltype(data)>>())
        return functor(std::decay_t<decltype(data[0])>());
    else
        return functor(std::forward<Data>(data));
}

template<typename Data, typename Functor>
auto invokeOnOptional(Data&& data, Functor&& functor)
{
    if constexpr (nx::utils::Is<std::optional, std::decay_t<decltype(data)>>())
        return invokeOnVector(std::decay_t<decltype(*data)>(), std::forward<Functor>(functor));
    else
        return invokeOnVector(std::forward<Data>(data), std::forward<Functor>(functor));
}

enum ApplyType
{
    applyAnd,
    applyComma,
    applyOr
};

template<ApplyType type, typename Model, typename Functor>
void invokeOnDbTypes(Functor&& functor)
{
    if constexpr (toDbTypesExists<Model>::value)
    {
        const auto invoke =
            [&functor](auto&& data)
            {
                return invokeOnOptional(std::forward<decltype(data)>(data),
                    [&functor](auto&& data)
                    {
                        return functor(std::forward<decltype(data)>(data));
                    });
            };
        if constexpr (type == applyAnd)
        {
            std::apply([&invoke](auto&&... args) { (... && invoke(std::move(args))); },
                typename Model::DbListTypes{});
        }
        else if constexpr (type == applyComma)
        {
            std::apply([&invoke](auto&&... args) { (..., invoke(std::move(args))); },
                typename Model::DbListTypes{});
        }
        else if constexpr (type == applyOr)
        {
            std::apply([&invoke](auto&&... args) { (... || invoke(std::move(args))); },
                typename Model::DbListTypes{});
        }
    }
    else
    {
        functor(Model());
    }
}

inline void throwError(Result r)
{
    using Exception = nx::network::rest::Exception;
    switch (r.error)
    {
        case ErrorCode::badRequest:
        case ErrorCode::dbError:
        case ErrorCode::notImplemented:
        case ErrorCode::unsupported:
            throw Exception::badRequest(std::move(r.message));
        case ErrorCode::forbidden:
            throw Exception::forbidden(std::move(r.message));
        case ErrorCode::notFound:
            throw Exception::notFound(std::move(r.message));
        default:
            NX_ASSERT(false, "Unexpected code: %1", r.toString());
            throw Exception::internalServerError(NX_FMT("Unexpected code: %1", r.toString()));
    }
}

template<typename IgnoredDbType, typename DbType, typename Model, typename Id>
void assertModelToDbTypesProducedValidResult(const DbType& value, const Id& id)
{
    using nx::utils::model::getId;

    if constexpr (!std::is_same_v<DbType, IgnoredDbType>
        && !std::is_same_v<DbType, nx::vms::api::ResourceStatusData>)
    {
        if constexpr (nx::utils::Is<std::vector, DbType>())
        {
            NX_ASSERT(std::find_if(value.begin(),
                          value.end(),
                          [&id](const auto& item)
                          {
                              return item.checkResourceExists
                                  == nx::vms::api::CheckResourceExists::yes
                                  || getId(item) != id;
                          })
                    == value.end(),
                "%1 toDbTypes() must fill %2 with resource id %3 and checkResourceExists != yes",
                typeid(Model),
                typeid(DbType),
                id);
        }
        else
        {
            NX_ASSERT(value.checkResourceExists != nx::vms::api::CheckResourceExists::yes
                    && getId(value) == id,
                "%1 toDbTypes() must fill %2 with resource id %3 and checkResourceExists != yes",
                typeid(Model),
                typeid(DbType),
                id);
        }
    }
}

template<typename T>
constexpr ApiCommand::Value getReadCommand()
{
    static_assert(std::is_same_v<T, nx::vms::api::StoredFileDataList>
        || std::is_same_v<T, nx::vms::api::UserDataList>
        || std::is_same_v<T, nx::vms::api::UserGroupDataList>
        || std::is_same_v<T, nx::vms::api::LayoutDataList>
        || std::is_same_v<T, nx::vms::api::ShowreelDataList>
        || std::is_same_v<T, nx::vms::api::LookupListDataList>
        || std::is_same_v<T, nx::vms::api::VideowallDataList>
        || std::is_same_v<T, nx::vms::api::LicenseDataList>
        || std::is_same_v<T, nx::vms::api::CameraDataList>
        || std::is_same_v<T, nx::vms::api::CameraAttributesDataList>
        || std::is_same_v<T, nx::vms::api::WebPageDataList>
        || std::is_same_v<T, nx::vms::api::StorageDataList>
        || std::is_same_v<T, nx::vms::api::MediaServerDataList>
        || std::is_same_v<T, nx::vms::api::MediaServerUserAttributesDataList>
        || std::is_same_v<T, nx::vms::api::ResourceStatusDataList>
        || std::is_same_v<T, nx::vms::api::ResourceParamWithRefDataList>
        || std::is_same_v<T, nx::vms::api::AnalyticsPluginDataList>
        || std::is_same_v<T, nx::vms::api::AnalyticsEngineDataList>
        || std::is_same_v<T, nx::vms::api::rules::RuleList>);
    if constexpr (std::is_same_v<T, nx::vms::api::StoredFileDataList>)
        return ApiCommand::Value::getStoredFiles;
    else if constexpr (std::is_same_v<T, nx::vms::api::UserDataList>)
        return ApiCommand::Value::getUsers;
    else if constexpr (std::is_same_v<T, nx::vms::api::UserGroupDataList>)
        return ApiCommand::Value::getUserGroups;
    else if constexpr (std::is_same_v<T, nx::vms::api::LayoutDataList>)
        return ApiCommand::Value::getLayouts;
    else if constexpr (std::is_same_v<T, nx::vms::api::ShowreelDataList>)
        return ApiCommand::Value::getShowreels;
    else if constexpr (std::is_same_v<T, nx::vms::api::LookupListDataList>)
        return ApiCommand::Value::getLookupLists;
    else if constexpr (std::is_same_v<T, nx::vms::api::VideowallDataList>)
        return ApiCommand::Value::getVideowalls;
    else if constexpr (std::is_same_v<T, nx::vms::api::LicenseDataList>)
        return ApiCommand::Value::getLicenses;
    else if constexpr (std::is_same_v<T, nx::vms::api::CameraDataList>)
        return ApiCommand::Value::getCameras;
    else if constexpr (std::is_same_v<T, nx::vms::api::CameraAttributesDataList>)
        return ApiCommand::Value::getCameraUserAttributesList;
    else if constexpr (std::is_same_v<T, nx::vms::api::WebPageDataList>)
        return ApiCommand::Value::getWebPages;
    else if constexpr (std::is_same_v<T, nx::vms::api::StorageDataList>)
        return ApiCommand::Value::getStorages;
    else if constexpr (std::is_same_v<T, nx::vms::api::MediaServerDataList>)
        return ApiCommand::Value::getMediaServers;
    else if constexpr (std::is_same_v<T, nx::vms::api::MediaServerUserAttributesDataList>)
        return ApiCommand::Value::getMediaServerUserAttributesList;
    else if constexpr (std::is_same_v<T, nx::vms::api::ResourceStatusDataList>)
        return ApiCommand::Value::getStatusList;
    else if constexpr (std::is_same_v<T, nx::vms::api::ResourceParamWithRefDataList>)
        return ApiCommand::Value::getResourceParams;
    else if constexpr (std::is_same_v<T, nx::vms::api::AnalyticsPluginDataList>)
        return ApiCommand::Value::getAnalyticsPlugins;
    else if constexpr (std::is_same_v<T, nx::vms::api::AnalyticsEngineDataList>)
        return ApiCommand::Value::getAnalyticsEngines;
    else if constexpr (std::is_same_v<T, nx::vms::api::rules::RuleList>)
        return ApiCommand::Value::getVmsRules;
    else
        return ApiCommand::NotDefined;
}

constexpr ApiCommand::Value parentUpdateCommand(const ApiCommand::Value command)
{
    switch (command)
    {
        case ApiCommand::Value::saveCamera:
            return ApiCommand::Value::saveCameras;
        case ApiCommand::Value::saveCameraUserAttributes:
            return ApiCommand::Value::saveCameraUserAttributesList;
        case ApiCommand::Value::saveMediaServerUserAttributes:
            return ApiCommand::Value::saveMediaServerUserAttributesList;
        case ApiCommand::Value::setResourceParam:
            return ApiCommand::Value::setResourceParams;
        case ApiCommand::Value::saveLayout:
            return ApiCommand::Value::saveLayouts;
        case ApiCommand::Value::addLicense:
            return ApiCommand::Value::addLicenses;
        default:
            return ApiCommand::NotDefined;
    }
}

template<typename T>
constexpr ApiCommand::Value getUpdateCommand()
{
    static_assert(std::is_same_v<T, nx::vms::api::StoredFileData>
        || std::is_same_v<T, nx::vms::api::UserGroupData>
        || std::is_same_v<T, nx::vms::api::UserData>
        || std::is_same_v<T, nx::vms::api::UserDataEx>
        || std::is_same_v<T, nx::vms::api::LayoutData>
        || std::is_same_v<T, nx::vms::api::ShowreelData>
        || std::is_same_v<T, nx::vms::api::LookupListData>
        || std::is_same_v<T, nx::vms::api::VideowallData>
        || std::is_same_v<T, nx::vms::api::LicenseData>
        || std::is_same_v<T, nx::vms::api::CameraData>
        || std::is_same_v<T, nx::vms::api::CameraAttributesData>
        || std::is_same_v<T, nx::vms::api::WebPageData>
        || std::is_same_v<T, nx::vms::api::StorageData>
        || std::is_same_v<T, nx::vms::api::MediaServerData>
        || std::is_same_v<T, nx::vms::api::MediaServerUserAttributesData>
        || std::is_same_v<T, nx::vms::api::ResourceStatusData>
        || std::is_same_v<T, nx::vms::api::ResourceParamWithRefData>
        || std::is_same_v<T, nx::vms::api::AnalyticsPluginData>
        || std::is_same_v<T, nx::vms::api::AnalyticsEngineData>
        || std::is_same_v<T, nx::vms::api::rules::Rule>);
    if constexpr (std::is_same_v<T, nx::vms::api::StoredFileData>)
        return ApiCommand::Value::updateStoredFile;
    else if constexpr (std::is_same_v<T, nx::vms::api::UserData>)
        return ApiCommand::Value::saveUser;
    else if constexpr (std::is_same_v<T, nx::vms::api::UserDataEx>)
        return ApiCommand::Value::saveUser;
    else if constexpr (std::is_same_v<T, nx::vms::api::UserGroupData>)
        return ApiCommand::Value::saveUserGroup;
    else if constexpr (std::is_same_v<T, nx::vms::api::LayoutData>)
        return ApiCommand::Value::saveLayout;
    else if constexpr (std::is_same_v<T, nx::vms::api::ShowreelData>)
        return ApiCommand::Value::saveShowreel;
    else if constexpr (std::is_same_v<T, nx::vms::api::LookupListData>)
        return ApiCommand::Value::saveLookupList;
    else if constexpr (std::is_same_v<T, nx::vms::api::VideowallData>)
        return ApiCommand::Value::saveVideowall;
    else if constexpr (std::is_same_v<T, nx::vms::api::LicenseData>)
        return ApiCommand::Value::addLicense;
    else if constexpr (std::is_same_v<T, nx::vms::api::CameraData>)
        return ApiCommand::Value::saveCamera;
    else if constexpr (std::is_same_v<T, nx::vms::api::CameraAttributesData>)
        return ApiCommand::Value::saveCameraUserAttributes;
    else if constexpr (std::is_same_v<T, nx::vms::api::WebPageData>)
        return ApiCommand::Value::saveWebPage;
    else if constexpr (std::is_same_v<T, nx::vms::api::StorageData>)
        return ApiCommand::Value::saveStorage;
    else if constexpr (std::is_same_v<T, nx::vms::api::MediaServerData>)
        return ApiCommand::Value::saveMediaServer;
    else if constexpr (std::is_same_v<T, nx::vms::api::MediaServerUserAttributesData>)
        return ApiCommand::Value::saveMediaServerUserAttributes;
    else if constexpr (std::is_same_v<T, nx::vms::api::ResourceStatusData>)
        return ApiCommand::Value::setResourceStatus;
    else if constexpr (std::is_same_v<T, nx::vms::api::ResourceParamWithRefData>)
        return ApiCommand::Value::setResourceParam;
    else if constexpr (std::is_same_v<T, nx::vms::api::AnalyticsPluginData>)
        return ApiCommand::Value::saveAnalyticsPlugin;
    else if constexpr (std::is_same_v<T, nx::vms::api::AnalyticsEngineData>)
        return ApiCommand::Value::saveAnalyticsEngine;
    else if constexpr (std::is_same_v<T, nx::vms::api::rules::Rule>)
        return ApiCommand::Value::saveVmsRule;
    else
        return ApiCommand::NotDefined;
}

template<typename T>
std::vector<ApiCommand::Value> commands(ApiCommand::Value deleteCommand)
{
    std::vector<ApiCommand::Value> r{deleteCommand};
    invokeOnDbTypes<applyComma, T>(
        [&r](auto&& data) { r.push_back(getUpdateCommand<std::decay_t<decltype(data)>>()); });
    return r;
}

template<typename T>
Result validateResourceTypeId(const T& data)
{
    if constexpr (kResourceTypeIdExists<T>::value)
    {
        if (data.typeId != T::kResourceTypeId)
        {
            throw nx::network::rest::Exception::invalidParameter(
                "typeId", "Does not match its resource data structure");
        }
    }

    return ErrorCode::ok;
}

constexpr ApiObjectType commandToObjectType(ApiCommand::Value deleteCommand)
{
    switch (deleteCommand)
    {
        case ApiCommand::removeMediaServer:
            return ApiObject_Server;
        case ApiCommand::removeCamera:
            return ApiObject_Camera;
        case ApiCommand::removeUser:
            return ApiObject_User;
        case ApiCommand::removeLayout:
            return ApiObject_Layout;
        case ApiCommand::removeVideowall:
            return ApiObject_Videowall;
        case ApiCommand::removeStorage:
            return ApiObject_Storage;
        case ApiCommand::removeWebPage:
            return ApiObject_WebPage;
        case ApiCommand::removeAnalyticsPlugin:
            return ApiObject_AnalyticsPlugin;
        case ApiCommand::removeAnalyticsEngine:
            return ApiObject_AnalyticsEngine;
        case ApiCommand::removeVmsRule:
            return ApiObject_VmsRule;
        default:
            return ApiObject_NotDefined;
    }
}

} // namespace ec2::details
