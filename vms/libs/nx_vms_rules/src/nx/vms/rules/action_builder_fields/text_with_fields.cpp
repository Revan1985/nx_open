// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>
#include <nx/vms/rules/utils/openapi_doc.h>

#include "../aggregated_event.h"

namespace nx::vms::rules {

struct TextWithFields::Private
{
    QString text;
    QList<ValueDescriptor> values;

    void parseText()
    {
        values.clear();

        bool inSub = false;
        int start = 0, cur = 0;

        while (cur != text.size())
        {
            if (utils::EventParameterHelper::isStartOfEventParameter(text[cur]))
            {
                if (start != cur)
                {
                    values += {
                        .type = FieldType::Text,
                        .value = text.mid(start, cur - start),
                        .start = start,
                        .length = cur - start,
                    };
                }
                start = cur;
                inSub = true;
            }

            if (utils::EventParameterHelper::isEndOfEventParameter(text[cur]) && inSub)
            {
                if (start + 1 != cur)
                {
                    ValueDescriptor desc = {.type = FieldType::Substitution,
                        .value = text.mid(start + 1, cur - start - 1),
                        .start = start};
                    desc.length = desc.value.size() + 2;
                    values += desc;
                }
                else
                {
                    // Empty brackets.
                    values += {
                        .type = FieldType::Text,
                        .value = "{}",
                        .start = start,
                    };

                }
                start = cur + 1;
                inSub = false;
            }

            ++cur;
        }

        if (start < text.size())
        {
            values += {.type = FieldType::Text,
                .value = text.mid(start),
                .start = start,
                .length = text.size() - start};
        }
    }
};

TextWithFields::TextWithFields(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    ActionBuilderField{descriptor},
    common::SystemContextAware(context),
    d(new Private())
{
}

TextWithFields::~TextWithFields()
{
}

QVariant TextWithFields::build(const AggregatedEventPtr& eventAggregator) const
{
    QString result;
    for (const auto& value: d->values)
    {
        if (value.type == FieldType::Substitution)
        {
            result += utils::EventParameterHelper::instance()->evaluateEventParameter(
                systemContext(), eventAggregator, value.value);
        }
        else
        {
            // It's just a text, add it to the result.
            result += value.value;
        }
    }

    return result;
}

QString TextWithFields::text() const
{
    return d->text;
}

void TextWithFields::parseText()
{
    d->parseText();
}

void TextWithFields::setText(const QString& text)
{
    if (d->text == text)
        return;

    d->text = text;
    d->parseText();
    emit textChanged();
}

TextWithFieldsFieldProperties TextWithFields::properties() const
{
    return TextWithFieldsFieldProperties::fromVariantMap(descriptor()->properties);
}

const TextWithFields::ParsedValues& TextWithFields::parsedValues() const
{
    return d->values;
}

QJsonObject TextWithFields::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor =
        utils::getPropertyOpenApiDescriptor(QMetaType::fromType<decltype(d->text)>());
    const bool isUrl = properties["validationPolicy"] == kUrlValidationPolicy;
    descriptor[utils::kExampleProperty] = isUrl
        ? "http://exampleServer/rest/v4/login/users/{user.name}"
        : "Event {event.name} occurred at {event.time} on {device.name}.";

    descriptor[utils::kDescriptionProperty] = "Text value supporting event parameters substitution."
        + utils::EventParameterHelper::instance()->getHtmlDescription();
    return descriptor;
}

} // namespace nx::vms::rules
