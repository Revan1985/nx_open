// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include <api/rest_types.h>
#include <api/server_rest_connection_fwd.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <nx/network/http/http_types.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/event/event_fwd.h>

struct QnMultiserverRequestData;

namespace nx::vms::api {
struct BookmarkV3;
}

class QnCameraBookmarksManagerPrivate:
    public QObject,
    public nx::vms::client::core::SystemContextAware
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnCameraBookmarksManagerPrivate(
        nx::vms::client::core::SystemContext* systemContext,
        QObject* parent = nullptr);

    virtual ~QnCameraBookmarksManagerPrivate() override;

    using RawResponseType =
        std::function<void (bool, rest::Handle, QByteArray, const nx::network::http::HttpHeaders&)>;

    /* Direct API section */

    /// @brief                  Asynchronously gathers bookmarks using specified filter.
    /// @param filter           Filter parameters.
    /// @param callback         Callback for receiving bookmarks data.
    /// @returns                Internal id of the request.
    int getBookmarksAsync(const QnCameraBookmarkSearchFilter& filter, BookmarksCallbackType callback);

    /**
     *  Gathers bookmarks around specified time point using usual bookmarks request. Heuristically
     *  extends (or shrinks) request time period to find appropriate data. Tries it maxTriesCount
     *  times maximum.
     */
    int getBookmarkstAroundPointHeuristic(
        const QnCameraBookmarkSearchFilter& filter,
        const QnCameraBookmarkList& source,
        int maxTriesCount,
        BookmarksAroundPointCallbackType callback);

    int getBookmarkTagsAsync(int maxTags, BookmarkTagsCallbackType callback);

    /// @brief                  Add the bookmark to the camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void addCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    void addExistingBookmark(const QnCameraBookmark& bookmark);

    void acknowledgeEvent(
        const QnCameraBookmark& bookmark,
        const nx::Uuid& eventRuleId,
        OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Update the existing bookmark on the camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void updateCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Delete the existing bookmark from the camera.
    /// @param bookmarkId       Target bookmark id.
    /// @param callback         Callback with operation result.
    void deleteCameraBookmark(const nx::Uuid &bookmarkId, OperationCallbackType callback = OperationCallbackType());

    /* Queries API section */

    /// @brief                  Instantiate new search query.
    /// @param filter           Filter parameters.
    /// @returns                Search query that is ready to be used.
    QnCameraBookmarksQueryPtr createQuery(const QnCameraBookmarkSearchFilter& filter);

    /**
     * Asynchronously execute search query.
     * @param query Target query.
     * @param callback Callback for received bookmarks data. Will be called when data is fully
     *     loaded.
     * @returns Request ID.
     */
    rest::Handle sendQueryRequest(
        const QnCameraBookmarksQueryPtr& query,
        BookmarksCallbackType callback);

    /**
     * Asynchronously execute search query, requesting data only from the given timepoint.
     * @param query Target query.
     * @param timePoint Minimal start time for the request filter
     * @param callback Callback for received bookmarks data. Will be called when data is fully
     *     loaded.
     */
    void sendQueryTailRequest(
        const QnCameraBookmarksQueryPtr& query,
        std::chrono::milliseconds timePoint,
        BookmarksCallbackType callback);

    using RestRequestCallback = rest::Callback<rest::ErrorOrData<QByteArray>>;
    rest::Handle sendRestRequest(
        const QString& path,
        const nx::vms::api::BookmarkV3& bookmark,
        const nx::network::http::Method& method,
        RestRequestCallback&& callback);

    std::optional<nx::Uuid> getServerForBookmark(const QnCameraBookmark& bookmark);

private:
    void handleQueryReply(
        const QUuid& queryId,
        bool success,
        int requestId,
        QnCameraBookmarkList bookmarks,
        BookmarksCallbackType callback);

    void handleBookmarkOperation(bool success, rest::Handle handle);

    /// @brief                  Register bookmarks search query to auto-update it if needed.
    /// @param query            Target query.
    void registerQuery(const QnCameraBookmarksQueryPtr &query);

    /// @brief                  Unregister bookmarks search query.
    /// @param queryId          Target query id.
    void unregisterQuery(const QUuid &queryId);

    /// @brief                  Check if the single query should be updated.
    /// @param queryId          Target query id.
    /// @returns                True if the query is queued to update.
    bool isQueryUpdateRequired(const QUuid &queryId);

    /// @brief                  Check if queries should be updated and update if needed.
    void checkQueriesUpdate();

    /// @brief                  Send request to update query with actual data.
    void updateQueryAsync(const QUuid &queryId);

    /** @brief                  Add the added or updated bookmark to the pending bookmarks list.
     *  Pending bookmark is the bookmark that was created, modified or deleted on the client
     *  but these changes haven't been reflected to queries caches.
     *  Pending bookmarks live inside the bookmarks manager for some time to let the queries gather actual information.
     */
    void addUpdatePendingBookmark(const QnCameraBookmark &bookmark);
    /// @brief                  Add removed bookmark to the pending bookmarks list.
    void addRemovePendingBookmark(const nx::Uuid &bookmarkId);
    /// @brief                  Merge the pending bookmarks with the given bookmarks list.
    /// @param query            The query for with merge operation is performed. The query specifies filters and cameras list.
    void mergeWithPendingBookmarks(const QnCameraBookmarksQueryPtr query, QnCameraBookmarkList &bookmarks);

    /// @brief                  Find and discard timed-out pending bookmarks.
    void checkPendingBookmarks();

    /// @brief                  Find and discard camera from all cached queries. Invalid queries will be discarded as well.
    void removeCameraFromQueries(const QnResourcePtr& resource);

    void addCameraBookmarkInternal(
        const QnCameraBookmark& bookmark,
        const nx::Uuid& eventRuleId,
        OperationCallbackType callback = OperationCallbackType());

    int sendPostRequest(
        const QString& path,
        QnMultiserverRequestData& request,
        std::optional<nx::Uuid> serverId = {});

    int sendGetRequest(const QString& path, QnMultiserverRequestData& request,
        RawResponseType callback);

    void startOperationsTimer();

private:
    QTimer* m_operationsTimer;

    struct OperationInfo {
        enum class OperationType {
            Add,
            Acknowledge,
            Update,
            Delete
        };

        OperationType operation;
        OperationCallbackType callback;
        nx::Uuid bookmarkId;

        OperationInfo();
        OperationInfo(OperationType operation, const nx::Uuid &bookmarkId, OperationCallbackType callback);
    };

    QMap<rest::Handle, OperationInfo> m_operations;

    struct QueryInfo
    {
        /** Weak reference to Query object. */
        QnCameraBookmarksQueryWeakPtr queryRef;

        /** Time of the last request. */
        QElapsedTimer requestTimer;

        /** Id of the last request. */
        rest::Handle requestId;

        QueryInfo();
        QueryInfo(const QnCameraBookmarksQueryPtr &query);
    };

    /** Cached bookmarks by query. */
    QHash<QUuid, QueryInfo> m_queries;

    struct PendingInfo {
        QnCameraBookmark bookmark;
        QElapsedTimer discardTimer;

        enum Type {
            AddBookmark,
            RemoveBookmark
        };

        Type type;

        PendingInfo(const QnCameraBookmark &bookmark, Type type);
        PendingInfo(const nx::Uuid &bookmarkId, Type type);
    };

    /**
     * Cache for case when we have sent getBookmarks request and THEN added/removed a bookmark
     * locally. So when we've got reply for getBookmarks, we may fix it correspondingly.
     */
    QHash<nx::Uuid, PendingInfo> m_pendingBookmarks;
};
