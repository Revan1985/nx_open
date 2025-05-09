// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmarks_manager_p.h"

#include <api/helpers/bookmark_request_data.h>
#include <api/server_rest_connection.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/rest/params.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/datetime.h>
#include <nx/vms/api/data/bookmark_models.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/api/helpers/bookmark_api_converter.h>
#include <nx/vms/common/bookmark/bookmark_facade.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_timer.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::core;

namespace {

/** Each query can be updated no more often than once in that period. */
static constexpr int kMinimimumRequestTimeoutMs = 10000;

/** Timer precision to update all queries. */
static constexpr int kQueriesCheckTimeoutMs = 1000;

/** Reserved value for invalid requests. */
static constexpr auto kInvalidRequestId = rest::Handle();

/** Cache of bookmarks we have just added or removed. */
static constexpr int kPendingDiscardTimeoutMs = 30000;

QnCameraBookmarkList::iterator findBookmark(QnCameraBookmarkList &list, const nx::Uuid &bookmarkId)
{
    return std::find_if(list.begin(), list.end(), [&bookmarkId](const QnCameraBookmark &bookmark)
    {
        return bookmark.guid == bookmarkId;
    });
}

bool checkBookmarkForQuery(const QnCameraBookmarksQueryPtr &query, const QnCameraBookmark &bookmark)
{
    if (!query->cameras().empty())
    {
        bool match = false;
        for (const auto& cameraId: query->cameras())
        {
            if (bookmark.cameraId == cameraId)
            {
                match = true;
                break;
            }
        }
        if (!match)
            return false;
    }

    return query->filter().checkBookmark(bookmark);
}

template<class ResponseType>
QnCameraBookmarksManagerPrivate::RawResponseType makeUbjsonResponseWrapper(
    QObject* guard,
    std::function<void (bool, rest::Handle, ResponseType)> callback)
{
    return
        [internalCallback = nx::utils::AsyncHandlerExecutor(guard).bind(std::move(callback))](
            bool success,
            rest::Handle handle,
            QByteArray body,
            nx::network::http::HttpHeaders /*headers*/) mutable
        {
            ResponseType response;

            if (success)
                response = QnUbjson::deserialized(body, ResponseType(), &success);

            internalCallback(success, handle, std::move(response));
        };
}

QString debugBookmarksRange(const QnCameraBookmarkList& bookmarks)
{
    if (bookmarks.empty())
        return "empty";

    return nx::format("%1 - %2",
        nx::utils::timestampToDebugString(bookmarks.front().startTimeMs),
        nx::utils::timestampToDebugString(bookmarks.back().endTime()));
}

enum class CheckResult
{
    dataFound,
    tryMore
};

/**
 *  Result structure for fetched data check. Contains successful mark of operation and new central
 *  point to fetch in case of check fail.
 */
struct CheckResultData
{
    CheckResult result = CheckResult::tryMore;
    std::chrono::milliseconds nextCentralPointMs;
};

/**
 * Checks if fetched data contains enough tail. Used in bookmarks requests to old servers (with
 * version less than 6.0) where we don't have central point parameter. Thus we have to request data
 * sequentially and check if results have enough tail and body.
 */
CheckResultData checkFetchedData(
    const FetchedData<QnCameraBookmarkList>& fetched,
    int length,
    EventSearch::FetchDirection direction)
{
    const int maxBodyLength = length / 2;
    const int maxTailLength = length - maxBodyLength;

    if (fetched.data.empty())
        return CheckResultData{CheckResult::dataFound, {}};

    const bool fetchedAllNeeded =
        fetched.ranges.tail.length + fetched.ranges.body.length < maxBodyLength + maxTailLength;

    // If data size is less than maximum we expect that we found all existing records.
    // Also, as our request uses central point (start time or end time) of currently existing data
    // (at least at the moment of last request) we suppose that maxTailLength/2 data size is enough
    // to be represented to user.
    if (fetchedAllNeeded || fetched.ranges.tail.length > maxTailLength / 2)
        return CheckResultData{CheckResult::dataFound, {}};

    if (fetched.data.begin()->startTimeMs >= fetched.data.rbegin()->startTimeMs)
        return CheckResultData{CheckResult::tryMore, {}};

    // Looking for the new period to be fetched to get an appropriate tail data.
    const int shift = std::min(maxTailLength / 2, fetched.ranges.body.length / 2);

    if (direction == EventSearch::FetchDirection::older)
    {
        auto newBodyStartTime = fetched.data.at(shift).startTimeMs;
        const auto bodyStartTime = fetched.data.begin()->startTimeMs;
        if (newBodyStartTime >= bodyStartTime && bodyStartTime.count())
            newBodyStartTime = bodyStartTime - 1ms;

        return CheckResultData{CheckResult::tryMore, newBodyStartTime};
    }

    auto newBodyStartTime = fetched.data.at(int(fetched.data.size()) - shift).startTimeMs;
    const auto bodyStartTime = fetched.data.rbegin()->startTimeMs;
    if (newBodyStartTime <= bodyStartTime && bodyStartTime.count() != QnTimePeriod::kMaxTimeValue)
        newBodyStartTime = bodyStartTime + 1ms;

    return CheckResultData{CheckResult::tryMore, newBodyStartTime};
}

nx::network::http::HttpHeaders makeProxyToServerHeaders(const std::optional<nx::Uuid>& serverId)
{
    if (!serverId)
        return {};

    nx::network::http::HttpHeaders result;
    nx::network::http::insertHeader(
        &result, {Qn::SERVER_GUID_HEADER_NAME, serverId.value().toSimpleStdString()});

    return result;
}

} // namespace

/************************************************************************/
/* OperationInfo                                                        */
/************************************************************************/
QnCameraBookmarksManagerPrivate::OperationInfo::OperationInfo()
    : operation(OperationType::Add)
    , callback()
{
}

QnCameraBookmarksManagerPrivate::OperationInfo::OperationInfo(
    OperationType operation,
    const nx::Uuid &bookmarkId,
    OperationCallbackType callback)
    : operation(operation)
    , callback(callback)
    , bookmarkId(bookmarkId)
{
}

/************************************************************************/
/* QueryInfo                                                            */
/************************************************************************/
QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo()
    : queryRef()
    , requestTimer()
    , requestId(kInvalidRequestId)
{
}

QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo(const QnCameraBookmarksQueryPtr &query)
    : queryRef(query.toWeakRef())
    , requestTimer()
    , requestId(kInvalidRequestId)
{
}

/************************************************************************/
/* PendingInfo                                                          */
/************************************************************************/
QnCameraBookmarksManagerPrivate::PendingInfo::PendingInfo(const QnCameraBookmark &bookmark, Type type)
    : bookmark(bookmark)
    , type(type)
{
    discardTimer.invalidate();
}

QnCameraBookmarksManagerPrivate::PendingInfo::PendingInfo(const nx::Uuid &bookmarkId, Type type)
    : type(type)
{
    bookmark.guid = bookmarkId;
    discardTimer.invalidate();
}

/************************************************************************/
/* QnCameraBookmarksManagerPrivate                                      */
/************************************************************************/
QnCameraBookmarksManagerPrivate::QnCameraBookmarksManagerPrivate(
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    m_operationsTimer(new QTimer(this))
{
    /*
     Bookmarks updating strategy:
     * clear cache or removing camera from pool
     * once a time update all auto-updating queries independently
        - return results at once, without local filtering (?)
        - make sure queries are listening only in bookmarks mode
     * implement full-text search on the client side
     * make sure bookmark periods exists before requesting to avoid a lot of empty requests
     * filter request period by chunks and by already loaded data
     * update chunks on history change and bookmarks adding/deleting, then forcefully re-request required periods
     */

    m_operationsTimer->setInterval(kQueriesCheckTimeoutMs);
    m_operationsTimer->setSingleShot(false);
    connect(m_operationsTimer, &QTimer::timeout, this, &QnCameraBookmarksManagerPrivate::checkPendingBookmarks);
    connect(m_operationsTimer, &QTimer::timeout, this, &QnCameraBookmarksManagerPrivate::checkQueriesUpdate);

    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
                removeCameraFromQueries(resource);
        });
}

QnCameraBookmarksManagerPrivate::~QnCameraBookmarksManagerPrivate()
{}

std::optional<nx::Uuid> QnCameraBookmarksManagerPrivate::getServerForBookmark(
    const QnCameraBookmark& bookmark)
{
    QnVirtualCameraResourcePtr camera =
        resourcePool()->getResourceById<QnVirtualCameraResource>(bookmark.cameraId);
    if (!camera)
        return {};

    QnMediaServerResourcePtr server =
        cameraHistoryPool()->getMediaServerOnTime(camera, bookmark.startTimeMs.count());
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online)
        return {};

    return server->getId();
}

void QnCameraBookmarksManagerPrivate::startOperationsTimer()
{
    if (!m_operationsTimer->isActive())
    {
        m_operationsTimer->start();
        checkQueriesUpdate();
    }
}

int QnCameraBookmarksManagerPrivate::sendPostRequest(
    const QString& path,
    QnMultiserverRequestData &request,
    std::optional<nx::Uuid> serverId)
{
    auto serverApi = connectedServerApi();

    if (!serverApi)
        return kInvalidRequestId;

    request.format = Qn::SerializationFormat::ubjson;

    auto callback = [this](
            bool success,
            rest::Handle handle,
            const rest::ServerConnection::ErrorOrEmpty& /*response*/)
        {
            handleBookmarkOperation(success, handle);
        };

    return serverApi->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
        /*helper*/ nullptr,
        nx::network::http::Method::post,
        path,
        request.toParams(),
        /*body*/ {},
        std::move(callback),
        this,
        makeProxyToServerHeaders(serverId));
}

rest::Handle QnCameraBookmarksManagerPrivate::sendRestRequest(
    const QString& path,
    const nx::vms::api::BookmarkV3& bookmark,
    const nx::network::http::Method& method,
    RestRequestCallback&& callback)
{
    auto serverApi = connectedServerApi();

    if (!serverApi)
        return kInvalidRequestId;

    return serverApi->sendRequest<rest::ErrorOrData<QByteArray>>(
        systemContext()->getSessionTokenHelper(),
        method,
        path,
        nx::network::rest::Params{},
        nx::reflect::json::serialize(bookmark),
        std::move(callback),
        this);
}

int QnCameraBookmarksManagerPrivate::sendGetRequest(
    const QString& path,
    QnMultiserverRequestData &request,
    RawResponseType callback)
{
    const auto serverApi = connectedServerApi();

    if (!serverApi)
    {
        executeDelayedParented([callback]() { callback(false, kInvalidRequestId, {}, {}); }, this);
        return kInvalidRequestId;
    }

    request.format = Qn::SerializationFormat::ubjson;

    return serverApi->getRawResult(path, request.toParams(), std::move(callback));
}

int QnCameraBookmarksManagerPrivate::getBookmarksAsync(
    const QnCameraBookmarkSearchFilter &filter,
    BookmarksCallbackType callback)
{
    nx::vms::common::GetBookmarksRequestData requestData;
    requestData.filter = filter;
    requestData.format = Qn::SerializationFormat::ubjson;
    if (!NX_ASSERT(requestData.filter.startTimeMs.count() >= 0, "Invalid start time passed"))
        requestData.filter.startTimeMs = {};
    if (!NX_ASSERT(requestData.filter.endTimeMs.count() >= 0, "Invalid end time passed"))
        requestData.filter.endTimeMs = {};

    return sendGetRequest(
        "/ec2/bookmarks",
        requestData,
        makeUbjsonResponseWrapper(this, callback));
}

int QnCameraBookmarksManagerPrivate::getBookmarkstAroundPointHeuristic(
    const QnCameraBookmarkSearchFilter& filter,
    const QnCameraBookmarkList& source,
    int maxTriesCount,
    BookmarksAroundPointCallbackType callback)
{
    using namespace nx::vms::client::core;

    nx::vms::common::GetBookmarksRequestData requestData;
    requestData.format = Qn::SerializationFormat::ubjson;
    requestData.filter = filter;

    const auto handler = makeUbjsonResponseWrapper(this, BookmarksCallbackType(
        [this, source, callback, filter, maxTriesCount]
            (bool success, rest::Handle requestId, QnCameraBookmarkList bookmarks)
        {
            if (!requestId)
                return;

            if (!success)
            {
                callback(/*success*/ false, requestId, {});
                return;
            }

            const auto direction = EventSearch::directionFromSortOrder(filter.orderBy.order);
            auto fetched = makeFetchedData<nx::vms::common::QnBookmarkFacade>(
                source, bookmarks, FetchRequest{direction, *filter.centralTimePointMs});
            const auto checkResult = checkFetchedData(fetched, filter.limit, direction);

            if (checkResult.result == CheckResult::dataFound)
            {
                truncateFetchedData(fetched, direction, filter.limit);
                callback(/*success*/ true, requestId, std::move(fetched));
            }
            else if (checkResult.nextCentralPointMs.count() && maxTriesCount > 0)
            {
                QnCameraBookmarkSearchFilter updatedFilter = filter;
                updatedFilter.startTimeMs = checkResult.nextCentralPointMs;
                getBookmarkstAroundPointHeuristic(updatedFilter, source, maxTriesCount - 1, callback);
            }
            else
            {
                callback(/*success*/ false, requestId,  BookmarkFetchedData{});
            }
        }));

    return sendGetRequest("/ec2/bookmarks", requestData, handler);
}

int QnCameraBookmarksManagerPrivate::getBookmarkTagsAsync(
    int maxTags, BookmarkTagsCallbackType callback)
{
    nx::vms::common::GetBookmarkTagsRequestData requestData;
    requestData.limit = maxTags;
    requestData.format = Qn::SerializationFormat::ubjson;

    return sendGetRequest(
        "/ec2/bookmarks/tags",
        requestData,
        makeUbjsonResponseWrapper(this, callback));
}

void QnCameraBookmarksManagerPrivate::addCameraBookmark(
    const QnCameraBookmark &bookmark,
    OperationCallbackType callback)
{
    addCameraBookmarkInternal(bookmark, nx::Uuid(), callback);
}

void QnCameraBookmarksManagerPrivate::addExistingBookmark(const QnCameraBookmark& bookmark)
{
    addUpdatePendingBookmark(bookmark);

    const auto it = m_pendingBookmarks.find(bookmark.guid);
    if (NX_ASSERT(it != m_pendingBookmarks.end()))
        it->discardTimer.start();
}

void QnCameraBookmarksManagerPrivate::acknowledgeEvent(
    const QnCameraBookmark& bookmark,
    const nx::Uuid& eventRuleId,
    OperationCallbackType callback)
{
    addCameraBookmarkInternal(bookmark, eventRuleId, callback);
}

void QnCameraBookmarksManagerPrivate::addCameraBookmarkInternal(
    const QnCameraBookmark& bookmark,
    const nx::Uuid& eventRuleId,
    OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark must not be added");
    if (!bookmark.isValid())
        return;

    const auto serverId = getServerForBookmark(bookmark);

    const auto operationType = !eventRuleId.isNull()
        ? OperationInfo::OperationType::Acknowledge
        : OperationInfo::OperationType::Add;

    auto handle = kInvalidRequestId;

    if (operationType == OperationInfo::OperationType::Acknowledge)
    {
        nx::vms::common::UpdateBookmarkRequestData request(bookmark, eventRuleId);
        handle = sendPostRequest("/ec2/bookmarks/acknowledge", request, serverId);
    }
    else
    {
        nx::vms::common::UpdateBookmarkRequestData request(bookmark);
        handle = sendPostRequest("/ec2/bookmarks/add", request, serverId);
    }

    if (handle != kInvalidRequestId)
        m_operations[handle] = OperationInfo(operationType, bookmark.guid, std::move(callback));
    else
        callback(false);

    addUpdatePendingBookmark(bookmark);
}

void QnCameraBookmarksManagerPrivate::updateCameraBookmark(
    const QnCameraBookmark& bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark must not be added");
    if (!bookmark.isValid())
        return;

    auto restCallback =
        [this](bool success, rest::Handle handle, rest::ErrorOrData<QByteArray> /*response*/)
        {
            handleBookmarkOperation(success, handle);
        };

    auto bookmarkModel = nx::vms::common::bookmarkToApi<nx::vms::api::BookmarkV3>(
        bookmark, getServerForBookmark(bookmark).value_or(nx::Uuid{}), /*includeDigest*/ true);
    const auto path = nx::format("rest/v4/devices/%1/bookmarks/%2",
        bookmarkModel.deviceId, bookmarkModel.id).toQString();
    const auto handle = sendRestRequest(path, bookmarkModel, nx::network::http::Method::patch,
        std::move(restCallback));

    if (handle != kInvalidRequestId)
    {
        m_operations[handle] = OperationInfo(
            OperationInfo::OperationType::Update, bookmark.guid, std::move(callback));
    }
    else
    {
        callback(false);
    }

    addUpdatePendingBookmark(bookmark);
}

void QnCameraBookmarksManagerPrivate::deleteCameraBookmark(
    const nx::Uuid &bookmarkId,
    OperationCallbackType callback)
{
    nx::vms::common::DeleteBookmarkRequestData request(bookmarkId);
    auto handle = sendPostRequest("/ec2/bookmarks/delete", request);
    m_operations[handle] = OperationInfo(
        OperationInfo::OperationType::Delete,
        bookmarkId,
        std::move(callback));

    addRemovePendingBookmark(bookmarkId);
}

void QnCameraBookmarksManagerPrivate::handleBookmarkOperation(bool success, rest::Handle handle)
{
    const auto opIt = m_operations.find(handle);
    if (opIt == m_operations.end())
        return;

    const auto operationInfo = std::move(*opIt);
    m_operations.erase(opIt);

    if (operationInfo.callback)
        operationInfo.callback(success);

    const auto bookIt = m_pendingBookmarks.find(operationInfo.bookmarkId);
    if (bookIt != m_pendingBookmarks.end())
    {
        if (success)
            m_pendingBookmarks.erase(bookIt);
        else
            bookIt->discardTimer.start();
    }
}

bool QnCameraBookmarksManagerPrivate::isQueryUpdateRequired(const QUuid &queryId)
{
    NX_ASSERT(m_queries.contains(queryId), "Query should be registered");
    if (!m_queries.contains(queryId))
        return false;

    const QueryInfo info = m_queries[queryId];
    QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();

    if (!NX_ASSERT(query, "Interface does not allow query to be null") || !query->active())
        return false;

    switch (query->state())
    {
        case QnCameraBookmarksQuery::State::invalid:
            return true;

        case QnCameraBookmarksQuery::State::queued:
            return !info.requestTimer.isValid()
                || info.requestTimer.hasExpired(kMinimimumRequestTimeoutMs) || !query->isValid();

        case QnCameraBookmarksQuery::State::requested:
        case QnCameraBookmarksQuery::State::partial:
        case QnCameraBookmarksQuery::State::actual:
            return false;

        default:
            NX_ASSERT(false, "Should never get here");
            break;
    }
    return false;
}

void QnCameraBookmarksManagerPrivate::checkQueriesUpdate()
{
    for (auto queryId : m_queries.keys())
    {
        bool updateRequired = isQueryUpdateRequired(queryId);
        if (!updateRequired)
            continue;

        const QueryInfo info = m_queries[queryId];
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        if (!NX_ASSERT(query, "Interface does not allow query to be null"))
            continue;

        sendQueryRequest(query, BookmarksCallbackType());
    }
}

void QnCameraBookmarksManagerPrivate::registerQuery(const QnCameraBookmarksQueryPtr &query)
{
    NX_ASSERT(query, "Interface does not allow query to be null");
    NX_ASSERT(!m_queries.contains(query->id()), "Query should be registered and unregistered only once");

    // Do not store shared pointer in the functor so it can be safely unregistered.
    NX_VERBOSE(this, "Register query %1, filter is %2",
        query->id(),
        query->filter());
    QUuid queryId = query->id();
    m_queries.insert(queryId, QueryInfo(query));
    connect(query.get(), &QnCameraBookmarksQuery::queryChanged, this,
        [this, queryId]
        {
            updateQueryAsync(queryId);
        });

    updateQueryAsync(queryId);
    startOperationsTimer();
}

void QnCameraBookmarksManagerPrivate::unregisterQuery(const QUuid &queryId)
{
    NX_VERBOSE(this, "Unregister query %1", queryId);
    NX_ASSERT(m_queries.contains(queryId), "Query should be registered and unregistered only once");
    m_queries.remove(queryId);
}

rest::Handle QnCameraBookmarksManagerPrivate::sendQueryRequest(
    const QnCameraBookmarksQueryPtr& query,
    BookmarksCallbackType callback)
{
    // Do not store shared pointer here so it can be safely unregistered in the process.
    QUuid queryId = query->id();
    NX_ASSERT(m_queries.contains(queryId), "Query must be registered");

    QueryInfo &info = m_queries[queryId];

    if (!query->isValid())
    {
        query->setState(QnCameraBookmarksQuery::State::actual);

        if (callback)
            callback(false, kInvalidRequestId, QnCameraBookmarkList());

        return kInvalidRequestId;
    }

    query->setState(QnCameraBookmarksQuery::State::requested);

    QnCameraBookmarkSearchFilter filter = query->filter();
    if (query->requestChunkSize() > 0)
        filter.limit = query->requestChunkSize();

    auto internalCallback =
        [this, queryId, callback]
        (bool success, int requestId, QnCameraBookmarkList bookmarks)
        {
            handleQueryReply(queryId, success, requestId, std::move(bookmarks), callback);
        };

    info.requestId = getBookmarksAsync(filter, internalCallback);
    info.requestTimer.restart();
    NX_VERBOSE(this, "Sending initial request [%1] with filter %2",
        info.requestId,
        filter);
    return info.requestId;
}

void QnCameraBookmarksManagerPrivate::sendQueryTailRequest(
    const QnCameraBookmarksQueryPtr& query,
    std::chrono::milliseconds timePoint,
    BookmarksCallbackType callback)
{
    query->setState(QnCameraBookmarksQuery::State::partial);
    QnCameraBookmarkSearchFilter filter = query->filter();
    filter.limit = query->requestChunkSize();
    filter.startTimeMs = timePoint;
    filter.endTimeMs = std::chrono::milliseconds(DATETIME_NOW);

    auto internalCallback =
        [this, queryId = query->id(), callback]
        (bool success, int requestId, QnCameraBookmarkList bookmarks)
        {
            handleQueryReply(queryId, success, requestId, std::move(bookmarks), callback);
        };

    QueryInfo& info = m_queries[query->id()];
    info.requestId = getBookmarksAsync(filter, internalCallback);
    info.requestTimer.restart();
    NX_VERBOSE(this,
        "Sending partial request [%1] with filter %2",
        info.requestId,
        filter);
}

void QnCameraBookmarksManagerPrivate::handleQueryReply(
    const QUuid& queryId,
    bool success,
    int requestId,
    QnCameraBookmarkList bookmarks,
    BookmarksCallbackType callback)
{
    const bool isReplyActual = success
        && requestId != kInvalidRequestId
        && m_queries.contains(queryId)
        && m_queries[queryId].requestId == requestId;

    if (!isReplyActual)
    {
        NX_VERBOSE(this, "Reply %1 for query %2 is not actual", requestId, queryId);
        if (callback)
            callback(success, requestId, bookmarks);

        return;
    }

    QueryInfo& info = m_queries[queryId];
    const QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
    NX_VERBOSE(this,
        "Processing reply [%1]: %2 bookmarks received (%3)%4",
        requestId,
        bookmarks.size(),
        debugBookmarksRange(bookmarks),
        (query ? QString{", query state is %1"}.arg(query->state()) : QString{}));
    if (!query)
        return;

    const bool isPartialData =
        query->requestChunkSize() > 0 && (int) bookmarks.size() == query->requestChunkSize();

    NX_ASSERT(query->state() == QnCameraBookmarksQuery::State::requested
        || query->state() == QnCameraBookmarksQuery::State::partial);

    // On initial request reply, reset cached data with received, otherwise merge loaded
    // data with already cached. Only default sort order is supported yet.
    if (query->state() == QnCameraBookmarksQuery::State::partial)
    {
        NX_ASSERT(query->filter().orderBy == QnBookmarkSortOrder::defaultOrder);
        bookmarks = QnCameraBookmark::mergeCameraBookmarks(
            query->systemContext(), {query->cachedBookmarks(), bookmarks});
    }
    query->setCachedBookmarks(bookmarks);
    const bool limitExceeded =
        query->filter().limit > 0 && (int) bookmarks.size() >= query->filter().limit;

    if (isPartialData && !limitExceeded)
    {
        sendQueryTailRequest(query, bookmarks.back().startTimeMs, std::move(callback));
    }
    else
    {
        query->setState(QnCameraBookmarksQuery::State::actual);
        info.requestId = kInvalidRequestId;
        mergeWithPendingBookmarks(query, bookmarks);
        NX_VERBOSE(this, "Query %1 is actual, bookmarks count %2", queryId, bookmarks.size());
        if (callback)
            callback(success, requestId, bookmarks);
    }
}

void QnCameraBookmarksManagerPrivate::updateQueryAsync(const QUuid& queryId)
{
    if (!m_queries.contains(queryId))
        return;

    QueryInfo& info = m_queries[queryId];
    info.requestId = kInvalidRequestId;
    const QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
    if (NX_ASSERT(query))
    {
        query->setState(QnCameraBookmarksQuery::State::queued);
        query->setCachedBookmarks({});
    }
}

QnCameraBookmarksQueryPtr QnCameraBookmarksManagerPrivate::createQuery(
    const QnCameraBookmarkSearchFilter& filter)
{
    QnCameraBookmarksQueryPtr query(new QnCameraBookmarksQuery(systemContext(), filter));
    QUuid queryId = query->id();
    registerQuery(query);

    connect(query.get(), &QObject::destroyed, this,
        [this, queryId]()
        {
            unregisterQuery(queryId);
        });

    return query;
}

void QnCameraBookmarksManagerPrivate::addUpdatePendingBookmark(const QnCameraBookmark& bookmark)
{
    m_pendingBookmarks.insert(bookmark.guid, PendingInfo(bookmark, PendingInfo::AddBookmark));
    startOperationsTimer();

    for (QueryInfo& info: m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        if (!NX_ASSERT(query, "Interface does not allow query to be null"))
            continue;

        if (!checkBookmarkForQuery(query, bookmark))
            continue;

        auto bookmarks = query->cachedBookmarks();
        auto it = findBookmark(bookmarks, bookmark.guid);
        if (it != bookmarks.end())
            bookmarks.erase(it);

        it = std::lower_bound(bookmarks.begin(), bookmarks.end(), bookmark);
        bookmarks.insert(it, bookmark);
        query->setCachedBookmarks(std::move(bookmarks));
    }
}

void QnCameraBookmarksManagerPrivate::addRemovePendingBookmark(const nx::Uuid& bookmarkId)
{
    m_pendingBookmarks.insert(bookmarkId, PendingInfo(bookmarkId, PendingInfo::RemoveBookmark));
    startOperationsTimer();

    for (QueryInfo& info: m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        if (!NX_ASSERT(query, "Interface does not allow query to be null"))
            continue;

        auto bookmarks = query->cachedBookmarks();
        auto it = findBookmark(bookmarks, bookmarkId);
        if (it == bookmarks.end())
            continue;

        bookmarks.erase(it);
        query->setCachedBookmarks(std::move(bookmarks));
    }
}

void QnCameraBookmarksManagerPrivate::mergeWithPendingBookmarks(
    const QnCameraBookmarksQueryPtr query,
    QnCameraBookmarkList& bookmarks)
{
    for (const PendingInfo& info: m_pendingBookmarks)
    {
        if (info.type == PendingInfo::RemoveBookmark)
        {
            auto it = findBookmark(bookmarks, info.bookmark.guid);
            if (it == bookmarks.end())
                continue;

            bookmarks.erase(it);
        }
        else
        {
            if (!checkBookmarkForQuery(query, info.bookmark))
                continue;

            auto it = findBookmark(bookmarks, info.bookmark.guid);
            if (it != bookmarks.end())
            {
                *it = info.bookmark;
            }
            else
            {
                it = std::lower_bound(bookmarks.begin(), bookmarks.end(), info.bookmark);
                bookmarks.insert(it, info.bookmark);
            }
        }
    }
}

void QnCameraBookmarksManagerPrivate::checkPendingBookmarks()
{
    for (auto it = m_pendingBookmarks.begin(); it != m_pendingBookmarks.end(); /* no inc */)
    {
        if (it->discardTimer.isValid() && it->discardTimer.hasExpired(kPendingDiscardTimeoutMs))
            it = m_pendingBookmarks.erase(it);
        else
            ++it;
    }
}

void QnCameraBookmarksManagerPrivate::removeCameraFromQueries(const QnResourcePtr& resource)
{
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    nx::Uuid cameraId = camera->getId();

    for (QueryInfo& info: m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        if (!NX_ASSERT(query, "Interface does not allow query to be null"))
            continue;

        if (!query->removeCamera(camera->getId()))
            continue;

        auto bookmarks = query->cachedBookmarks();
        for (auto it = bookmarks.begin(); it != bookmarks.end();)
        {
            if (it->cameraId == cameraId)
                it = bookmarks.erase(it);
            else
                ++it;
        }
        query->setCachedBookmarks(std::move(bookmarks));
    }
}
