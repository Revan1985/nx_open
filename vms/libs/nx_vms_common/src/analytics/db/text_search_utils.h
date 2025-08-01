// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <QtCore/QString>

#include <analytics/common/object_metadata.h>
#include <nx/utils/scope_guard.h>

namespace nx::analytics::db {

inline bool isKeyValueDelimiterChar(QChar ch) { return ch == ':' || ch == '=' || ch == '>' || ch == '<'; }
inline bool isKeyValueDelimiter(const QStringView& value, int* outSize = nullptr)
{
    if (outSize)
        *outSize = 0;
    if (value.size() == 0)
        return false;

    if (value.size() >= 2)
    {
        bool result = value.startsWith(QLatin1String(">="))
            || value.startsWith(QLatin1String("<="))
            || value.startsWith(QLatin1String("!="));
        if (result)
        {
            if (outSize)
                *outSize = 2;
            return result;
        }
    }
    bool result = isKeyValueDelimiterChar(value[0]);
    if (result && outSize)
        *outSize = 1;
    return result;
}

enum class ConditionType
{
    attributePresenceCheck,
    attributeValueMatch,
    textMatch,
    numericRangeMatch,
};

struct TokenData
{
    QString value;
    bool matchesFromStart = false;
    bool matchesTillEnd = false;
    bool isQuoted = false;

    bool operator==(const TokenData&) const = default;
};

struct TextSearchCondition
{
    ConditionType type;

    QString name;
    TokenData valueToken;
    QString text;

    // Negative condition. Text should not match (attribute or value is not exists).
    bool isNegative = false;
    nx::common::metadata::NumericRange range;

    TextSearchCondition(ConditionType type): type(type) {}

    bool operator==(const TextSearchCondition& right) const
    {
        return type == right.type
            && name == right.name
            && valueToken == right.valueToken
            && text == right.text
            && isNegative == right.isNegative;
    }
};

struct AttributePresenceCheck:
    public TextSearchCondition
{
    AttributePresenceCheck(const QString& name, bool isNegative = false):
        TextSearchCondition(ConditionType::attributePresenceCheck)
    {
        this->name = name;
        this->isNegative = isNegative;
    }
};

struct AttributeValueMatch:
    public TextSearchCondition
{
    AttributeValueMatch(const QString& name, const TokenData& value, bool isNegative = false):
        TextSearchCondition(ConditionType::attributeValueMatch)
    {
        this->name = name;
        this->valueToken = value;
        this->isNegative = isNegative;
    }
};

struct NumericRangeMatch: public TextSearchCondition
{
    NumericRangeMatch(const QString& name, const nx::common::metadata::NumericRange& range, bool isNegative = false):
        TextSearchCondition(ConditionType::numericRangeMatch)
    {
        this->name = name;
        this->range = range;
        this->isNegative = isNegative;
    }
};

struct TextMatch:
    public TextSearchCondition
{
    TextMatch(const QString& text):
        TextSearchCondition(ConditionType::textMatch)
    {
        this->text = text;
    }
};

/**
 * Format description:
 * TextFilter = *matchToken *[ SP matchToken ]
 * matchToken = textExpr | attributePresenceExpr | attributeMatchToken
 * textExpr = ['"'] TEXT ['"']
 * attributePresenceExpr = "$" TEXT
 * attributeMatchToken = TEXT ":" ['"'] TEXT ['"']
 */
class NX_VMS_COMMON_API UserTextSearchExpressionParser
{
public:
    /**
     * Found user expressions are reported to handler.
     * @param text The format is specified in the class description.
     * @return true if text is valid.
     */
    template<typename Handler>
    // requires std::is_invocable_v<Handler, TextSearchCondition>
    void parse(const QString& text, Handler handler);

    std::vector<TextSearchCondition> parse(const QString& text);

private:
    void saveToken(QStringView token);

    template<typename Handler>
    void processTokens(Handler& handler);

    /**
     * Replaces \CHAR with CHAR. Does NOT replace \n with LF or similar.
     */
    QString unescape(const QStringView& str);

    QString unescapeName(const QStringView& str);

private:
    std::vector<TokenData> m_tokens;
};

//-------------------------------------------------------------------------------------------------

/**
 * Matches given attributes against search expression defined by UserTextSearchExpressionParser.
 */
class NX_VMS_COMMON_API TextMatcher
{
public:
    TextMatcher() = default;
    TextMatcher(const QString& text);

    /**
     * Uses UserTextSearchExpressionParser to parse text.
     */
    void parse(const QString& text);
    bool empty() const;

    bool matchAttributes(const nx::common::metadata::Attributes& attributes) const;

    /**
     * @return true If all tokens of the filter were matched
     * by calls to matchAttributes or matchText().
     */
    bool matchText(const QString& text) const;


    bool hasShortTokens() const;

private:
    uint64_t matchExactAttributes(
        const nx::common::metadata::Attributes& attributes) const;

    uint64_t checkAttributesPresence(
        const nx::common::metadata::Attributes& attributes) const;

    uint64_t matchAttributeValues(
        const nx::common::metadata::Attributes& attributes) const;

    bool wordMatchAnyOfAttributes(
        const QString& word,
        const nx::common::metadata::Attributes& attributes) const;

    bool rangeMatchAttributes(
        const nx::common::metadata::NumericRange& range,
        const QString& paramName,
        const nx::common::metadata::Attributes& attributes) const;

    bool allConditionMatched(uint64_t result) const;

    static bool isAttributeNameMatching(const QString& attributeName, const QString& value);

private:
    std::vector<TextSearchCondition> m_conditions;
};

//-------------------------------------------------------------------------------------------------

template<typename Handler>
void UserTextSearchExpressionParser::parse(const QString& userText, Handler handler)
{
    enum class State
    {
        waitingTokenStart,
        readingToken,
    };

    QStringView text = userText;

    bool quoted = false;
    int tokenStart = 0;
    m_tokens.clear();
    State state = State::waitingTokenStart;

    for (int i = 0; i < text.size(); ++i)
    {
        const QChar ch = text[i];

        switch (state)
        {
            case State::waitingTokenStart:
                if (ch.isSpace())
                    continue;
                tokenStart = i;
                state = State::readingToken;
                --i; // Processing the current char again.
                break;

            case State::readingToken:
                if (ch == '\\')
                {
                    // Not processing the current character.
                    ++i;
                    continue;
                }

                if (ch == '"')
                    quoted = !quoted;
                if (quoted)
                    continue;

                int tokenSize = 0;
                bool isDelimiter = isKeyValueDelimiter(text.mid(i), &tokenSize);
                if (isDelimiter)
                {
                    saveToken(text.mid(tokenStart, i - tokenStart));

                    auto delimiterToken = text.mid(i, tokenSize);
                    i += tokenSize - 1;
                    saveToken(delimiterToken); //< Adding key/value delimiter as a separate token.
                    state = State::waitingTokenStart;
                }
                else if (ch.isSpace())
                {
                    saveToken(text.mid(tokenStart, i - tokenStart));
                    state = State::waitingTokenStart;
                }

                break;
        }
    }

    if (state == State::readingToken)
        saveToken(text.mid(tokenStart));

    processTokens(handler);
}

template<typename Handler>
void UserTextSearchExpressionParser::processTokens(Handler& handler)
{
    auto isNegative =
        [](const QStringView& token)
        { return token.startsWith('!'); };

    int lastProcessedTokenIndex = -1;
    for (int i = 0; i < (int) m_tokens.size(); ++i)
    {
        const auto& token = m_tokens[i];
        const std::optional<TokenData> nextToken =
            (i+1) < (int) m_tokens.size() ? std::make_optional(m_tokens[i+1]) : std::nullopt;
        const std::optional<TokenData> previousToken =
            (i-1) > lastProcessedTokenIndex ? std::make_optional(m_tokens[i-1]) : std::nullopt;

        auto lastProcessedTokenIndexUpdater = nx::utils::makeScopeGuard(
            [&lastProcessedTokenIndex, &i]() { lastProcessedTokenIndex = i; });

        if (isKeyValueDelimiter(token.value))
        {
            if (!previousToken)
                continue; //< Ignoring misplaced key/value delimiter.

            if (!nextToken)
            {
                // Treating "name:" similar to "$name".
                handler(AttributePresenceCheck(unescapeName(previousToken->value), isNegative((previousToken->value))));
                continue;
            }

            auto attributeName = unescapeName(previousToken->value);
            TokenData valueToken = *nextToken;
            valueToken.value = unescape(valueToken.value);
            const QString& attributeValue = valueToken.value;

            using namespace nx::common::metadata;
            const bool forceTextSearch = nextToken->isQuoted && token.value == QString("=");
            const bool negative = isNegative(previousToken->value) ^ (token.value == "!=");
            if (!forceTextSearch && AttributeEx::isNumberOrRange(attributeName, attributeValue))
            {
                NumericRange range;
                if (token.value == QString(">"))
                    range.from = RangePoint{attributeValue.toFloat(), false};
                else if (token.value == QString(">="))
                    range.from = RangePoint{attributeValue.toFloat(), true};
                else if (token.value == QString("<"))
                    range.to = RangePoint{attributeValue.toFloat(), false};
                else if (token.value == QString("<="))
                    range.to = RangePoint{attributeValue.toFloat(), true};
                else
                    range = AttributeEx::parseRangeFromValue(attributeValue);
                handler(NumericRangeMatch(attributeName, range, negative));
            }
            else
            {
                handler(AttributeValueMatch(attributeName, valueToken, negative));
            }
            // We have already consumed the next token.
            ++i;
        }
        else if (nextToken && isKeyValueDelimiter(nextToken->value))
        {
            // Skipping token: it will be processed on the next iteration.
            lastProcessedTokenIndexUpdater.disarm();
        }
        else if (token.value.startsWith('$') || token.value.startsWith(QLatin1String("!")))
        {
            handler(AttributePresenceCheck(unescapeName(token.value), isNegative(token.value)));
        }
        else
        {
            handler(TextMatch(unescape(token.value)));
        }
    }
}

NX_VMS_COMMON_API QString serializeTextSearchConditions(
    const std::vector<TextSearchCondition>& conditions);

} // namespace nx::analytics::db
