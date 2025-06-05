// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "datetime.h"

#include <QtCore/QTimeZone>

#include "nx_utils_ini.h"
#include "std_string_utils.h"
#include "string.h"

namespace nx::utils {

using namespace std::chrono;

namespace {

static constexpr const char* kWeekDays[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
static constexpr const char* kMonths[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

QDateTime parseRfc1123Date(const std::string_view& str)
{
    static constexpr const char* kTemplate = "%3s, %d %3s %d %d:%d:%d";
    char weekday[4], monthStr[4];
    int day, year, hour, min, sec;
    if (sscanf(str.data(), kTemplate, weekday, &day, monthStr, &year, &hour, &min, &sec) != 7)
        return QDateTime();

    for (int month = 0; month < 12; ++month)
    {
        if (strncmp(monthStr, kMonths[month], 3) == 0)
            return QDateTime(QDate(year, month + 1, day), QTime(hour, min, sec), QTimeZone::UTC);
    }
    return QDateTime();
}

// Sunday, 06-Nov-94 08:49:37 GMT (RFC 850, obsoleted by RFC 1036)
QDateTime parseRfc850Date(const std::string_view& str)
{
    static constexpr char kRfc850Template[] = "dddd, dd-MMM-yy hh:mm:ss";
    return QLocale(QLocale::C).toDateTime(
        QByteArray::fromRawData(str.data(), str.size()), kRfc850Template);
}

// Sun Nov  6 08:49:37 1994  (ANSI C's asctime() format)
QDateTime parseAnsiCDate(const std::string_view& str)
{
    // QDateTime::fromString() requires exact format match, so two templates are required:
    // Non padded if the day of the month is >= 10, and padded if day of the month is < 10
    static constexpr char kAnsiCTemplate[]       = "ddd MMM d hh:mm:ss yyyy";
    static constexpr char kAnsiCTemplatePadded[] = "ddd MMM  d hh:mm:ss yyyy";

    if (str.size() < 9)
        return QDateTime();

    return QLocale(QLocale::C).toDateTime(
        QByteArray::fromRawData(str.data(), str.size()),
        str[8] == ' ' ? kAnsiCTemplatePadded : kAnsiCTemplate);
}

} // anonymous namespace

QString timestampToRfc2822(qint64 timestampMs)
{
    if (timestampMs == DATETIME_NOW)
        return "LIVE";

    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(Qt::RFC2822Date);
}

QString timestampToISO8601(std::chrono::microseconds timestamp)
{
    if (timestamp == std::chrono::microseconds::zero())
        return {};
    return QDateTime::fromMSecsSinceEpoch(
        std::chrono::duration_cast<milliseconds>(timestamp).count())
        .toString(Qt::DateFormat::ISODate);
}

QString timestampToRfc2822(milliseconds timestamp)
{
    return timestampToRfc2822(timestamp.count());
}

QString timestampToRfc2822(std::chrono::system_clock::time_point timestamp)
{
    return timestampToRfc2822(
        duration_cast<milliseconds>(timestamp.time_since_epoch()));
}

QString timestampToDebugString(qint64 timestampMs, const QString& format)
{
    if (timestampMs == 0)
        return "0";

    if (timestampMs == DATETIME_NOW)
        return "DATETIME_NOW";

    if (timestampMs == DATETIME_INVALID)
        return "DATETIME_INVALID";

    static const qint64 kMaxTimeMs = 4'102'444'800'000; //< 2100.01.01.
    if (timestampMs > kMaxTimeMs)
        timestampMs /= 1000; //< Most probably time is in microseconds.

    const QString defaultFormat = nx::utils::ini().debugTimeRepresentation;

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(timestampMs, QTimeZone::UTC);
    return dateTime.toString(format.isEmpty() ? defaultFormat : format);
}

QString timestampToDebugString(milliseconds timestamp, const QString& format)
{
    return timestampToDebugString(timestamp.count(), format);
}

std::string formatDateTime(const QDateTime& value)
{
    // Starts with Monday to correspond to QDate::dayOfWeek() return value: 1 for Monday, 7 for Sunday

    if (value.isNull() || !value.isValid())
        return std::string();

    QDateTime utc = value.toUTC();
    const QDate& date = utc.date();
    const QTime& time = utc.time();

    char strDateBuf[256];
    snprintf(strDateBuf, sizeof(strDateBuf), "%s, %02d %s %d %02d:%02d:%02d GMT",
        kWeekDays[date.dayOfWeek() - 1],
        date.day(),
        kMonths[date.month() - 1],
        date.year(),
        time.hour(),
        time.minute(),
        time.second());

    return std::string(strDateBuf);
}

QDateTime parseDateToQDateTime(const std::string_view& str)
{
    auto date = nx::utils::trim(str);

    // Sanity check. parseAnsiCDate() has hard-coded array access.
    if (date.length() < 8)
        return QDateTime();

    // If the string ends with " GMT" it is either RFC 1123 or RFC 850
    if (nx::utils::endsWith(date, " GMT"))
    {
        // QDateTime::fromString() fails if string contains " GMT"
        // because "M" is used as a formatter.
        date = date.substr(0, date.length() - 4);
        QDateTime parsedDate;

        // RFC 1123 date: first three characters are abbreviated day of the week, like "Sun",
        // followed by a comma
        if (date[3] == ',')
            parsedDate = parseRfc1123Date(date);

        if (!parsedDate.isValid())
            parsedDate = parseRfc850Date(date);

        if (parsedDate.isValid())
        {
            parsedDate.setTimeZone(QTimeZone::UTC);
            return parsedDate;
        }
    }

    // Fall back to ANSI C date.
    return parseAnsiCDate(date);
}

std::string formatDateTime(std::chrono::system_clock::time_point tp)
{
    const auto msecsSinceEpoch =
        std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());

    QDateTime dt;
    dt.setMSecsSinceEpoch(static_cast<qint64>(msecsSinceEpoch.count()));

    return formatDateTime(dt);
}

std::chrono::system_clock::time_point parseDateTime(std::string_view datetime)
{
    auto dt = parseDateToQDateTime(datetime);
    if (!dt.isValid())
        return {};

    auto msecsSinceEpoch = dt.toMSecsSinceEpoch();

    return std::chrono::system_clock::time_point() +
        std::chrono::milliseconds(static_cast<std::size_t>(msecsSinceEpoch));
}

std::string formatDateTime(std::chrono::steady_clock::time_point tp)
{
    static const std::pair<std::chrono::system_clock::time_point,
        std::chrono::steady_clock::time_point> baseTimePoints{
        std::chrono::system_clock::now(),
        std::chrono::steady_clock::now()};
    return formatDateTime(baseTimePoints.first +
        std::chrono::duration_cast<system_clock::duration>(tp - baseTimePoints.second));
}

microseconds parseDateTimeUsec(const QString& dateTimeStr)
{
    if (dateTimeStr.toLower().trimmed() == "now")
        return microseconds{DATETIME_NOW};

    if (!dateTimeStr.startsWith('-')
        && (dateTimeStr.contains('T') || dateTimeStr.contains('-')))
    {
        const auto dt = QDateTime::fromString(trimAndUnquote(dateTimeStr), Qt::ISODateWithMs);
        if (dt.isValid())
            return duration_cast<microseconds>(milliseconds{dt.toMSecsSinceEpoch()});
    }

    const auto timestampMsec = dateTimeStr.toLongLong();
    if (timestampMsec == DATETIME_NOW)
        return microseconds{DATETIME_NOW};

    return duration_cast<microseconds>(milliseconds{timestampMsec});
}

milliseconds parseDateTimeMsec(const QString& dateTimeStr)
{
    const auto usecSinceEpoch = parseDateTimeUsec(dateTimeStr);
    if (usecSinceEpoch.count() < 0 || usecSinceEpoch == microseconds{DATETIME_NOW})
        return milliseconds{usecSinceEpoch.count()}; //< Special values are returned "as is".

    return duration_cast<milliseconds>(microseconds{usecSinceEpoch});
}

} // namespace nx::utils
