// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <iostream>
#include <map>
#include <list>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <set>

#include <gtest/gtest.h>

#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/raw_json_text.h>
#include <nx/reflect/json/object.h>
#include <nx/reflect/json/serializer.h>

#include "serialization_acceptance_tests.h"

namespace nx::reflect::test {

template<class T>
void deserializedAsExpected(
    const T& value, const std::tuple<T, DeserializationResult>& deserializationContext)
{
    EXPECT_TRUE(std::get<1>(deserializationContext));
    EXPECT_EQ(value, std::get<0>(deserializationContext));
}

template <class T>
void deserializationFailed(
    const std::string& wrongJson, const DeserializationResult& expected)
{
    const auto [_, deserialized] = nx::reflect::json::deserialize<T>(wrongJson);
    ASSERT_FALSE(deserialized);
    ASSERT_EQ(expected.success, deserialized.success);
    ASSERT_EQ(expected.errorDescription, deserialized.errorDescription);
    ASSERT_EQ(expected.firstBadFragment, deserialized.firstBadFragment);
    ASSERT_EQ(expected.firstNonDeserializedField, deserialized.firstNonDeserializedField);
}

struct JsonTypeSet
{
    template<typename... Args>
    static auto serialize(Args&&... args)
    {
        return nx::reflect::json::serialize(std::move(args)...);
    }

    template<typename... Args>
    static auto deserialize(Args&&... args)
    {
        return nx::reflect::json::deserialize(std::move(args)...);
    }
};

INSTANTIATE_TYPED_TEST_SUITE_P(
    Json,
    FormatAcceptance,
    JsonTypeSet);

//-------------------------------------------------------------------------------------------------

struct X
{
    std::string s;

    bool operator==(const X& right) const { return s == right.s; }
};

NX_REFLECTION_INSTRUMENT(X, (s))

struct Stringizable
{
    std::string s;

    std::string toString() const { return s; }

    static Stringizable fromString(const std::string_view& s) { return {std::string(s)}; }

    bool operator==(const Stringizable& right) const { return s == right.s; }
    bool operator<(const Stringizable& right) const { return s < right.s; }
};

NX_REFLECTION_TAG_TYPE(Stringizable, useStringConversionForSerialization)

struct StringizablePrime
{
    std::string s;

    Stringizable toString() const { return Stringizable{s}; }

    static StringizablePrime fromString(const std::string_view& s) { return { std::string(s) }; }

    bool operator==(const StringizablePrime& right) const { return s == right.s; }
};

NX_REFLECTION_TAG_TYPE(StringizablePrime, useStringConversionForSerialization)

} // namespace nx::reflect::test

namespace std {

template<>
struct hash<nx::reflect::test::Stringizable>
{
    size_t operator()(const nx::reflect::test::Stringizable& s) const
    {
        return hash<string>()(s.s);
    }
};

} // namespace std

namespace nx::reflect::test {

template<typename T>
struct Foo
{
    int num = 0;
    T t;

    bool operator==(const Foo& right) const
    {
        return num == right.num
            && t == right.t;
    }
};

using FooOptional = Foo<std::optional<std::string>>;
NX_REFLECTION_INSTRUMENT(FooOptional, (num)(t))

using FooStringizable = Foo<Stringizable>;
NX_REFLECTION_INSTRUMENT(FooStringizable, (num)(t))

using FooStringizablePrime = Foo<StringizablePrime>;
NX_REFLECTION_INSTRUMENT(FooStringizablePrime, (num)(t))

using FooObjArray = Foo<std::vector<X>>;
NX_REFLECTION_INSTRUMENT(FooObjArray, (num)(t))

using FooStringizableArray = Foo<std::vector<Stringizable>>;
NX_REFLECTION_INSTRUMENT(FooStringizableArray, (num)(t))

using FooStringizableSet = Foo<std::set<Stringizable>>;
NX_REFLECTION_INSTRUMENT(FooStringizableSet, (num)(t))

using FooStringizableMap = Foo<std::map<Stringizable, Stringizable>>;
NX_REFLECTION_INSTRUMENT(FooStringizableMap, (num)(t))

using FooStringizableUnorderedSet = Foo<std::unordered_set<Stringizable>>;
NX_REFLECTION_INSTRUMENT(FooStringizableUnorderedSet, (num)(t))

using FooStringizableUnorderedMap = Foo<std::unordered_map<Stringizable, Stringizable>>;
NX_REFLECTION_INSTRUMENT(FooStringizableUnorderedMap, (num)(t))

using FooIntVec = Foo<std::vector<int>>;
NX_REFLECTION_INSTRUMENT(FooIntVec, (num)(t))

using FooBool = Foo<bool>;
NX_REFLECTION_INSTRUMENT(FooBool, (num)(t))

class Json:
    public ::testing::Test
{
protected:
    template<typename T>
    void testSerialization(
        const std::string& json,
        const T& value)
    {
        assertSerializedTo(json, value);
        assertDeserializedFrom(json, value);
    }

    template<typename T>
    void assertSerializedTo(
        const std::string& expected,
        const T& value)
    {
        const auto serialized = json::serialize(value);
        ASSERT_EQ(expected, serialized);
    }

    template<typename T>
    void assertDeserializedFrom(
        const std::string& json,
        const T& expected)
    {
        const auto deserialized = json::deserialize<T>(json);
        deserializedAsExpected(expected, deserialized);
    }
};

struct FooBuiltInTypes
{
    int n = 1;
    bool b = true;
    double d = 2.2;

    bool operator==(const FooBuiltInTypes& right) const
    {
        return n == right.n && b == right.b && std::abs(d - right.d) < 0.00001;
    }
};

NX_REFLECTION_INSTRUMENT(FooBuiltInTypes, (n)(b)(d))

void PrintTo(const FooBuiltInTypes& val, ::std::ostream* os)
{
    *os << json::serialize(val);
}

TEST_F(Json, builtin_type_support)
{
    testSerialization(R"({"n":12,"b":true,"d":56.193})", FooBuiltInTypes{12, true, 56.193});
    testSerialization(R"({"n":12,"b":false,"d":56.193})", FooBuiltInTypes{12, false, 56.193});

    deserializedAsExpected(
        FooBuiltInTypes{12, false, 56.193},
        json::deserialize<FooBuiltInTypes>(R"({"n":12,"b":"false","d":"56.193"})"));

    deserializedAsExpected(
        FooBuiltInTypes{12, true, 56.193},
        json::deserialize<FooBuiltInTypes>(R"({"n":12,"b":"true","d":"56.193"})"));

    deserializedAsExpected(
        FooBuiltInTypes{12, true, 56.0},
        json::deserialize<FooBuiltInTypes>(R"({"n":12,"b":"true","d":56})"));
}

TEST_F(Json, support_nan_in_double)
{
    static const FooBuiltInTypes kDefaultValues;

    FooBuiltInTypes valueWithNaN{
        .n = 0,
        .b = false,
        .d = std::numeric_limits<double>::quiet_NaN()
    };

    const auto serialized = json::serialize(valueWithNaN);
    ASSERT_EQ(R"({"n":0,"b":false,"d":null})", serialized);

    // Null value should not override existing field contents.
    deserializedAsExpected(
        kDefaultValues,
        json::deserialize<FooBuiltInTypes>(R"({"n":1,"b":"true","d":null})"));
}

struct FooString
{
    std::string s;

    bool operator==(const FooString& right) const { return s == right.s; }
};

NX_REFLECTION_INSTRUMENT(FooString, (s))

TEST_F(Json, string_support)
{
    testSerialization(R"({"s":"Hello"})", FooString{ "Hello" });
    testSerialization(R"({"s":"Hello\nHello"})", FooString{"Hello\nHello"});
}

struct FooOptionalWithCustomDefault: FooOptional
{
    FooOptionalWithCustomDefault() { t = "default"; }
    FooOptionalWithCustomDefault(int n, std::optional<std::string> s): FooOptional{n, s} {}
};

TEST_F(Json, std_optional_is_supported)
{
    testSerialization(R"({"num":12})", FooOptional{12, std::nullopt});
    testSerialization(R"({"num":12,"t":"Hello"})", FooOptional{12, "Hello"});

    // Checking that an optional field is set to nullopt regardless of the default value.
    testSerialization(R"({"num":12})", FooOptionalWithCustomDefault{12, std::nullopt});

    testSerialization(
        R"(["Hello, world"])",
        std::make_optional(std::vector<std::string>{{"Hello, world"}}));
}

TEST_F(Json, stringizable_type_is_supported)
{
    testSerialization(R"({"num":12,"t":"Hello"})", FooStringizable{12, {"Hello"}});
}

// This type is not tagged with
// NX_REFLECTION_TAG_TYPE(NotTaggedStringizable, useStringConversionForSerialization).
// So, it cannot be used in json or other serialization.
struct NotTaggedStringizable
{
    std::string toString() const { return "test"; }
    static NotTaggedStringizable fromString(const std::string_view&) { return {}; }
};

using FooNotTaggedStringizable = Foo<NotTaggedStringizable>;
NX_REFLECTION_INSTRUMENT(FooNotTaggedStringizable, (num)(t))

namespace {

template<typename T, typename = std::void_t<>>
struct IsSerializable: std::false_type{};

template<typename T>
struct IsSerializable<
    T,
    std::void_t<decltype(nx::reflect::json::serialize(std::declval<const T&>()))>
>: std::true_type {};

} // namespace

TEST_F(Json, not_tagged_stringizable_type_is_not_supported)
{
    // nx::reflect::json::serialize(NotTaggedStringizable());
    // Compile error on the previous line actually tells that
    // NX_REFLECTION_TAG_TYPE(NotTaggedStringizable, useStringConversionForSerialization)
    // is required to serialize. But, it is not clear how to put this check into a static_assert
    // like static_assert(!IsSerializable<NotTaggedStringizable>::value);

    static_assert(IsSerializable<Stringizable>::value);
}

// This functionality is useful to support types that provide "QString toString() const".
// QString in its turn has "std::string toStdString() const".
TEST_F(Json, type_that_stringizes_to_stringizable_is_also_supported)
{
    testSerialization(R"({"num":12,"t":"Hello"})", FooStringizablePrime{12, {"Hello"}});
}

using FooStringVector = Foo<std::vector<std::string>>;
NX_REFLECTION_INSTRUMENT(FooStringVector, (num)(t))

using FooStringList = Foo<std::list<std::string>>;
NX_REFLECTION_INSTRUMENT(FooStringList, (num)(t))

TEST_F(Json, sequence_containers_are_supported)
{
    testSerialization(R"({"num":12,"t":["ab","ac"]})", FooStringVector{12, {{"ab"}, {"ac"}}});
    testSerialization(R"({"num":12,"t":[]})", FooStringVector{12, {}});

    testSerialization(R"({"num":12,"t":["ab","ac"]})", FooStringList{12, {{"ab"}, {"ac"}}});
    testSerialization(R"({"num":12,"t":[]})", FooStringList{12, {}});
}

TEST_F(Json, std_vector_of_objects_is_supported)
{
    testSerialization(
        R"({"num":12,"t":[{"s":"ab"},{"s":"ac"}]})",
        FooObjArray{12, {{{"ab"}, {"ac"}}}});

    testSerialization(
        R"({"num":12,"t":[]})",
        FooObjArray{12, {}});
}

TEST_F(Json, std_vector_of_stringizable_objects)
{
    testSerialization(
        R"({"num":12,"t":["ab","ac"]})",
        FooStringizableArray{12, {{{"ab"}, {"ac"}}}});

    testSerialization(
        R"({"num":12,"t":[]})",
        FooStringizableArray{12, {}});

    testSerialization(
        R"({"num":12,"t":["ab","ac"]})",
        FooStringizableSet{12, {{{"ab"}, {"ac"}}}});

    testSerialization(
        R"({"num":12,"t":["ab"]})",
        FooStringizableUnorderedSet{12, {{{"ab"}}}});
}

TEST_F(Json, map_with_stringizable_keys)
{
    testSerialization(
        R"({"num":12,"t":{"key1":"val1","key2":"val2"}})",
        FooStringizableMap{12, {{{"key1"}, {"val1"}},  {{"key2"}, {"val2"}}}});

    testSerialization(
        R"({"num":12,"t":{"key1":"val1"}})",
        FooStringizableUnorderedMap{12, {{{"key1"}, {"val1"}}}});

    testSerialization(
        R"({"k1":"v1","k2":"v2"})",
        std::map<std::string, std::string>{{"k1", "v1"}, {"k2", "v2"}});

    testSerialization(
        R"({"k1":"v1"})",
        std::unordered_map<std::string, std::string>{{"k1", "v1"}});
}

TEST_F(Json, map_keys_are_overwritten_by_deserialization)
{
    std::map<std::string, std::string> m{{"color","yellow"}};

    ASSERT_TRUE(json::deserialize(R"({"color":"red","shape":"square"})", &m));
    ASSERT_EQ("red", m["color"]);
    ASSERT_EQ("square", m["shape"]);
}

TEST_F(Json, keys_are_added_to_multimap_by_deserialization)
{
    std::multimap<std::string, std::string> m{{"color","yellow"}};
    ASSERT_TRUE(json::deserialize(R"({"color":"red","shape":"square"})", &m));

    const auto expected = std::multimap<std::string, std::string>{
        {"color","yellow"},
        {"color","red"},
        {"shape","square"}
    };

    ASSERT_EQ(expected, m);
}

TEST_F(Json, multimap_support)
{
    testSerialization(
        R"({"foo":"bar1","foo":"bar2","fu":"bar"})",
        std::multimap<std::string, std::string>{{"foo", "bar1"}, {"foo", "bar2"}, {"fu", "bar"}});
}

//-------------------------------------------------------------------------------------------------

struct FooChrono
{
    std::chrono::seconds s;
    std::chrono::milliseconds ms;
    std::chrono::microseconds us;
    std::chrono::system_clock::time_point t;

    bool operator==(const FooChrono& right) const
    {
        return s == right.s && ms == right.ms && us == right.us && t == right.t;
    }
};

NX_REFLECTION_INSTRUMENT(FooChrono, (s)(ms)(us)(t))

TEST_F(Json, std_chrono_support)
{
    using namespace std::chrono;

    testSerialization(
        R"({"s":"3","ms":"13","us":"23","t":"45"})",
        FooChrono{
            seconds(3), milliseconds(13),
            microseconds(23), system_clock::time_point(milliseconds(45))});
    deserializedAsExpected(
        FooChrono{
            seconds(3),
            milliseconds(13),
            microseconds(23),
            system_clock::time_point(milliseconds(45))},
        json::deserialize<FooChrono>(R"({"s":3,"ms":13,"us":23,"t":"45"})"));
}

//-------------------------------------------------------------------------------------------------
// Testing custom JSON serialization/deserialization functions.

struct CustomSerializable
{
    int y = 0;

    bool operator==(const CustomSerializable& right) const { return y == right.y; }
};

// Also checking that the custom serialization/deserialization functions have greater priority
// than reflection with NX_REFLECTION_INSTRUMENT.
NX_REFLECTION_INSTRUMENT(CustomSerializable, (y))

// Serializes value to a string representation of y instead of "{"y": 12}" produced by reflect::json.
static void serialize(
    json::SerializationContext* ctx,
    const CustomSerializable& value)
{
    ctx->composer.writeInt(value.y);
}

static DeserializationResult deserialize(
    const json::DeserializationContext& ctx,
    CustomSerializable* data)
{
    return json::deserialize(ctx, &data->y);
}

using FooCustomSerializable = Foo<CustomSerializable>;
NX_REFLECTION_INSTRUMENT(FooCustomSerializable, (num)(t))

TEST_F(Json, custom_functions_are_invoked_and_have_greater_priority_than_reflection)
{
    testSerialization(
        R"({"num":12,"t":23})",
        FooCustomSerializable{12, {23}});
}

//-------------------------------------------------------------------------------------------------

TEST_F(Json, numeric_field_can_be_deserialized_from_string)
{
    auto [value, ok] = json::deserialize<FooStringizable>(R"({"num":"231","t":"foo"})");
    ASSERT_TRUE(ok);
    ASSERT_EQ(231, value.num);
    ASSERT_EQ("foo", value.t.s);
}

//-------------------------------------------------------------------------------------------------

TEST_F(Json, invalid_json)
{
    // String is expected, but number is found.
    ASSERT_FALSE(
        std::get<DeserializationResult>(json::deserialize<FooStringizable>(R"({"t":231})")));
}

//-------------------------------------------------------------------------------------------------

TEST_F(Json, array_is_supported_as_a_top_level_value_type)
{
    testSerialization(
        R"([{"s":"1"},{"s":"2"}])",
        std::vector<X>{{"1"}, {"2"}});
}

//-------------------------------------------------------------------------------------------------

struct NonTemplateContainer: std::vector<X>
{
    using std::vector<X>::vector;
};

TEST_F(Json, non_template_container_is_supported)
{
    static_assert(IsSequenceContainerV<NonTemplateContainer>, "");

    testSerialization(
        R"([{"s":"1"},{"s":"2"}])",
        NonTemplateContainer{{"1"}, {"2"}});
}

struct StringizableNonTemplateContainer: std::vector<std::string>
{
    using std::vector<std::string>::vector;

    std::string toString() const
    {
        std::string res;
        std::for_each(begin(), end(), [&res](auto& val) { res += val + ","; });
        if (!res.empty())
            res.pop_back();
        return res;
    }

    static StringizableNonTemplateContainer fromString(const std::string_view& str)
    {
        StringizableNonTemplateContainer result;
        std::size_t tokenStart = 0;
        for (std::size_t pos = str.find(',', tokenStart);
            tokenStart != str.npos;
            pos = str.find(',', tokenStart))
        {
            result.push_back(std::string(str.substr(tokenStart, pos - tokenStart)));
            tokenStart = pos == str.npos ? str.npos : pos + 1;
        }

        return result;
    }
};

NX_REFLECTION_TAG_TYPE(StringizableNonTemplateContainer, useStringConversionForSerialization)

using FooStringizableNonTemplateContainer = Foo<StringizableNonTemplateContainer>;
NX_REFLECTION_INSTRUMENT(FooStringizableNonTemplateContainer, (num)(t))

TEST_F(Json, stringizable_non_template_container_is_serialized_as_a_string)
{
    testSerialization(
        R"({"num":12,"t":"1,2"})",
        FooStringizableNonTemplateContainer{12, {{"1"}, {"2"}}});
}

//-------------------------------------------------------------------------------------------------

using FooX = Foo<X>;
NX_REFLECTION_INSTRUMENT(FooX, (num)(t))

TEST_F(Json, std_variable_support)
{
    using V = std::variant<X, FooX>;

    assertSerializedTo(R"({"s":"X"})", V{X{"X"}});
    assertSerializedTo(R"({"num":27,"t":{"s":"X"}})", V{FooX{27, X{"X"}}});
}

struct Throwable
{
    int x = 0;

    Throwable() = default;
    Throwable(const Throwable&) { throw 1; }
    Throwable& operator=(const Throwable&) { throw 1; }
};

NX_REFLECTION_INSTRUMENT(Throwable, (x))

struct WithVariant
{
    using V = std::variant<X, Throwable>;

    V v;
};

NX_REFLECTION_INSTRUMENT(WithVariant, (v))

// valueless std::variant is expected to be serialized into null.
TEST_F(Json, std_variable_valueless)
{
    // Constructing valueless_by_exception std::variant.
    WithVariant val{WithVariant::V{X{"x"}}};
    try { val.v = Throwable(); } catch (int) {}

    assertSerializedTo(R"({"v":null})", val);
}

struct VariantAB
{
    int a = 0;
    int b = 0;

    bool operator==(const VariantAB&) const = default;
};
NX_REFLECTION_INSTRUMENT(VariantAB, (a)(b))

struct VariantAC
{
    int a = 0;
    int c = 0;

    bool operator==(const VariantAC&) const = default;
};
NX_REFLECTION_INSTRUMENT(VariantAC, (a)(c))

TEST_F(Json, deserialize_variant)
{
    using Variant = std::variant<VariantAB, VariantAC>;
    #define ASSERT_VARIANT(JSON, VALUE) \
        { \
            auto [data, r] = nx::reflect::json::deserialize<Variant>(JSON); \
            ASSERT_TRUE(r); \
            const Variant expected = VALUE; \
            ASSERT_EQ(data, expected); \
        }
    ASSERT_VARIANT(R"json({"a": 1, "b": 2})json", (VariantAB{1, 2}))
    ASSERT_VARIANT(R"json({"a": 1, "c": 3})json", (VariantAC{1, 3}))
    ASSERT_VARIANT(R"json({"c": 3, "a": 1})json", (VariantAC{1, 3}))
    ASSERT_VARIANT(R"json({"b": 2})json", (VariantAB{0, 2}))
    ASSERT_VARIANT(R"json({"c": 3})json", (VariantAC{0, 3}))
    ASSERT_VARIANT(R"json({})json", (VariantAB{0, 0}))

    #define ASSERT_ERROR(JSON, ERROR) \
        { \
            auto [_, r] = nx::reflect::json::deserialize<Variant>(JSON); \
            ASSERT_FALSE(r); \
            ASSERT_EQ(r.errorDescription, ERROR); \
        }
    ASSERT_ERROR(R"json({"a": 1, "b": 2, "c": 3})json", "`b` is unexpected")
    ASSERT_ERROR(R"json({"d": 4})json", "`d` is unexpected")
}

//-------------------------------------------------------------------------------------------------

struct MillisAndInt { std::chrono::milliseconds ms; int n; };
NX_REFLECTION_INSTRUMENT(MillisAndInt, (ms)(n));

static constexpr char kMillisAndIntJson[] = "{\"ms\": \"\", \"n\": \"\"}";

TEST_F(Json, exception_thrown_during_deserialization_is_handled)
{
    const auto [parsed, success] = json::deserialize<MillisAndInt>(kMillisAndIntJson);
    ASSERT_FALSE(success);
    ASSERT_EQ(success.firstBadFragment, "{\"ms\":\"\",\"n\":\"\"}");
}

//-------------------------------------------------------------------------------------------------

struct DataWithDefaultValues { std::string s = "hello"; int n = -123; std::string t; };
NX_REFLECTION_INSTRUMENT(DataWithDefaultValues, (s)(n)(t));

static constexpr char kDataWithDefaultValuesJson[] = "{\"t\": \"ttt\"}";

TEST_F(Json, default_value_is_preserved_if_attribute_is_missing_in_json_doc)
{
    const auto [parsed, success] = json::deserialize<DataWithDefaultValues>(
        kDataWithDefaultValuesJson);

    ASSERT_TRUE(success);
    ASSERT_EQ(DataWithDefaultValues().s, parsed.s);
    ASSERT_EQ(DataWithDefaultValues().n, parsed.n);
    ASSERT_EQ("ttt", parsed.t);
}

TEST_F(Json, error_reporting)
{
    deserializationFailed<FooBuiltInTypes>(
        R"({"n":12,"b":"not a bool","d":56.193})",
        DeserializationResult(
            false,
            "Either a bool value or a string value \"true\" or \"false\" is expected",
            "\"not a bool\"",
            "b"));

    deserializationFailed<FooBuiltInTypes>(
        R"({"n":false,"b":"true","d":56.193})",
        DeserializationResult(
            false, "Either a number or a string is expected for an integral value", "false", "n"));

    deserializationFailed<FooBuiltInTypes>(
        R"("not an object")",
        DeserializationResult(false, "Object value expected", "\"not an object\""));

    deserializationFailed<X>(
        R"({"s":1})", DeserializationResult(false, "String value is expected", "1", "s"));

    deserializationFailed<std::vector<int>>(
        R"({"s":1})", DeserializationResult(false, "Array is expected", R"({"s":1})"));

    deserializationFailed<FooIntVec>(
        R"({"num": 0,"t":[1,2,3,false]})",
        DeserializationResult(
            false, "Either a number or a string is expected for an integral value", "false", "t"));
}

//-------------------------------------------------------------------------------------------------

struct S
{
    std::string str;
    std::map<std::string, int> map;
    std::vector<bool> vec;
    bool b;
    FooBool foo;

    bool operator==(const S& r) const
    {
        return std::tie(str, map, vec, b, foo) == std::tie(r.str, r.map, r.vec, r.b, r.foo);
    }
};

NX_REFLECTION_INSTRUMENT(S, (str)(map)(vec)(b)(foo))

struct V
{
    std::variant<std::string, std::nullptr_t> field = "value";
};

NX_REFLECTION_INSTRUMENT(V, (field))

TEST_F(Json, errors_skipped_if_requested_str_err)
{
    const S expectedValue{std::string(), {{"a", 1}, {"b", 2}}, {true, false}, false, {1, false}};
    S value = expectedValue;
    const auto result = nx::reflect::json::deserialize(
        R"({"str":123,"map":{"a":1,"b":2},"vec":[true,false],"b":false,"foo":{"num":1,"t":false}})",
        &value,
        json::DeserializationFlag::ignoreFieldTypeMismatch);
    ASSERT_EQ(expectedValue, value);
    ASSERT_TRUE(result.success);
}

TEST_F(Json, errors_skipped_if_requested_map_err)
{
    const S expectedValue{"str", {{"a", 1}, {"b", 2}}, {true, false}, false, {1, false}};
    S value = expectedValue;
    const auto result = nx::reflect::json::deserialize(
        R"({"str":"str","map":{"a":1,"b":2,"c":false},"vec":[true,false],"b":false,"foo":{"num":1,"t":false}})",
        &value,
        json::DeserializationFlag::ignoreFieldTypeMismatch);
    ASSERT_EQ(expectedValue, value);
    ASSERT_TRUE(result.success);
}

TEST_F(Json, errors_skipped_if_requested_vector_err)
{
    const S expectedValue{"str", {{"a", 1}, {"b", 2}}, {true, false}, false, {1, false}};
    S value = expectedValue;
    const auto result = nx::reflect::json::deserialize(
        R"({"str":"str","map":{"a":1,"b":2},"vec":[true,"ququ",false],"b":false,"foo":{"num":1,"t":false}})",
        &value,
        json::DeserializationFlag::ignoreFieldTypeMismatch);
    ASSERT_EQ(expectedValue, value);
    EXPECT_TRUE(result.success);
}

TEST_F(Json, errors_skipped_if_requested_bool_err)
{
    const S expectedValue{"str", {{"a", 1}, {"b", 2}}, {true, false}, bool(), {1, false}};
    S value = expectedValue;
    const auto result = nx::reflect::json::deserialize(
        R"({"str":"str","map":{"a":1,"b":2},"vec":[true,false],"b":10,"foo":{"num":1,"t":false}})",
        &value,
        json::DeserializationFlag::ignoreFieldTypeMismatch);
    ASSERT_EQ(expectedValue, value);
    EXPECT_TRUE(result.success);
}

TEST_F(Json, errors_skipped_if_requested_obj_err)
{
    const S expectedValue{"str", {{"a", 1}, {"b", 2}}, {true, false}, false, FooBool()};
    S value = expectedValue;
    const auto result = nx::reflect::json::deserialize(
        R"({"str":"str","map":{"a":1,"b":2},"vec":[true,false],"b":false,"foo":"not_an_obj"})",
        &value,
        json::DeserializationFlag::ignoreFieldTypeMismatch);
    ASSERT_EQ(expectedValue, value);
    EXPECT_TRUE(result.success);
}

TEST_F(Json, explicit_null_deserialization)
{
    const S expectedValue{"str", {{"a", 1}, {"b", 2}}, {true, false}, true, FooBool()};
    S value = expectedValue;
    const auto result = nx::reflect::json::deserialize(
        R"({"str":null,"map":null,"vec":null,"b":null,"foo":null})", &value);
    EXPECT_EQ(expectedValue, value);
    EXPECT_TRUE(result.success);
}

TEST_F(Json, simple_type_as_json)
{
    testSerialization("\"qweasd123\"", std::string("qweasd123"));
    testSerialization("123", 123);
    testSerialization("null", nullptr);
}

TEST_F(Json, null_overwrites_value)
{
    V value;
    const auto result = nx::reflect::json::deserialize(
        R"({"field": null})", &value);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(value.field));
}

//-------------------------------------------------------------------------------------------------

struct DurationAsNumber
{
    std::chrono::milliseconds d = std::chrono::milliseconds::zero();

    bool operator==(const DurationAsNumber& rhs) const = default;
};

NX_REFLECTION_INSTRUMENT(DurationAsNumber, (d))
NX_REFLECTION_TAG_TYPE(DurationAsNumber, jsonSerializeChronoDurationAsNumber)

TEST_F(Json, chrono_duration_is_serialized_as_number_if_asked_explicitely)
{
    testSerialization("{\"d\":123}", DurationAsNumber{.d = std::chrono::milliseconds(123)});
}

TEST_F(Json, array_with_chrono_duration_is_serialized_as_number_if_asked_explicitely)
{
    using ms = std::chrono::milliseconds;
    std::vector<DurationAsNumber> v = { DurationAsNumber{.d = ms(1)}, DurationAsNumber{.d = ms(2)} };
    testSerialization("[{\"d\":1},{\"d\":2}]", v);
}

//-------------------------------------------------------------------------------------------------

struct WithRawJsonText
{
    json::RawJsonText foo;

    bool operator==(const WithRawJsonText& rhs) const = default;
};

NX_REFLECTION_INSTRUMENT(WithRawJsonText, (foo))

TEST_F(Json, RawJsonText)
{
    testSerialization("{\"foo\":123}", WithRawJsonText{.foo = {.jsonText = "123"}});
    testSerialization("{\"foo\":[\"bar\"]}", WithRawJsonText{.foo = {.jsonText = "[\"bar\"]"}});

    testSerialization("123", json::RawJsonText{.jsonText = "123"});
    testSerialization("{\"foo\":[\"bar\"]}", json::RawJsonText{.jsonText = "{\"foo\":[\"bar\"]}"});
}

//-------------------------------------------------------------------------------------------------

TEST_F(Json, JsonObject)
{
    {
        json::Object value;
        value.set("foo", 123);
        testSerialization("{\"foo\":123}", value);
    }

    {
        json::Object value;
        value.set("bar", FooBuiltInTypes{.n = 321, .b = false, .d = 1.5});
        testSerialization("{\"bar\":{\"n\":321,\"b\":false,\"d\":1.5}}", value);
    }

    {
        const auto [value, success] = json::deserialize<json::Object>(R"({"foo":123})");
        ASSERT_TRUE(success);
        ASSERT_TRUE(value.contains("foo"));
        ASSERT_EQ(123, value.get<int>("foo"));
        ASSERT_FALSE(value.get<std::string>("foo"));
        ASSERT_FALSE(value.get<int>("bar"));
    }
}

class DerivedFromJsonObject:
    public json::Object
{
};

TEST_F(Json, DerivedFromJsonObject)
{
    DerivedFromJsonObject value;
    value.set("foo", 123);
    testSerialization("{\"foo\":123}", value);
}

} // namespace nx::reflect::test
