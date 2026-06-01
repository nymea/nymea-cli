// SPDX-License-Identifier: GPL-3.0-or-later

#include "engine.h"
#include "engineutils.h"

#include "generated/actiontype.h"
#include "generated/apiutils.h"
#include "generated/configurationsettimezoneparams.h"
#include "generated/configurationsettimezoneresponse.h"
#include "generated/integrationsaddthingparams.h"
#include "generated/integrationsaddthingresponse.h"
#include "generated/integrationsconfirmpairingparams.h"
#include "generated/integrationsconfirmpairingresponse.h"
#include "generated/integrationsdiscoverthingsparams.h"
#include "generated/integrationsdiscoverthingsresponse.h"
#include "generated/integrationseditthingparams.h"
#include "generated/integrationseditthingresponse.h"
#include "generated/integrationsexecuteactionparams.h"
#include "generated/integrationsexecuteactionresponse.h"
#include "generated/integrationsgetthingclassesparams.h"
#include "generated/integrationspairthingparams.h"
#include "generated/integrationspairthingresponse.h"
#include "generated/integrationsremovethingparams.h"
#include "generated/integrationsremovethingresponse.h"
#include "generated/integrationsstatechangednotificationparams.h"
#include "generated/integrationsthingaddednotificationparams.h"
#include "generated/integrationsthingchangednotificationparams.h"
#include "generated/integrationsthingremovednotificationparams.h"
#include "generated/integrationsthingsettingchangednotificationparams.h"
#include "generated/jsonrpcauthenticateresponse.h"
#include "generated/jsonrpcpushbuttonauthfinishednotificationparams.h"
#include "generated/jsonrpcrequestpushbuttonauthparams.h"
#include "generated/jsonrpcrequestpushbuttonauthresponse.h"
#include "generated/jsonrpcsetnotificationstatusparams.h"
#include "generated/jsonrpcsetnotificationstatusresponse.h"
#include "generated/package.h"
#include "generated/param.h"
#include "generated/state.h"
#include "generated/statetype.h"
#include "generated/systemcheckforupdatesparams.h"
#include "generated/systemcheckforupdatesresponse.h"
#include "generated/systemgetcapabilitiesparams.h"
#include "generated/systemgetcapabilitiesresponse.h"
#include "generated/systemgetpackagesparams.h"
#include "generated/systemgetpackagesresponse.h"
#include "generated/systemgettimeparams.h"
#include "generated/systemgettimeresponse.h"
#include "generated/systemgettimezonesparams.h"
#include "generated/systemgettimezonesresponse.h"
#include "generated/systemgetupdatestatusparams.h"
#include "generated/systemgetupdatestatusresponse.h"
#include "generated/systemrebootparams.h"
#include "generated/systemrebootresponse.h"
#include "generated/systemrestartparams.h"
#include "generated/systemrestartresponse.h"
#include "generated/systemshutdownparams.h"
#include "generated/systemshutdownresponse.h"
#include "generated/systemtimeconfigurationchangednotificationparams.h"
#include "generated/systemupdatepackagesparams.h"
#include "generated/systemupdatepackagesresponse.h"
#include "generated/systemupdatestatuschangednotificationparams.h"
#include "generated/thing.h"
#include "generated/thingclass.h"
#include "generated/usersremovetokenparams.h"
#include "generated/usersremovetokenresponse.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMetaObject>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <ftxui/component/mouse.hpp>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

namespace nymea {

namespace {

std::string settingsViewLabel(int view);
std::string powerActionLabel(int action);

#if 0
#ifndef APP_LICENSE_SPDX
#define APP_LICENSE_SPDX "GPL-3.0-or-later"
#endif

#ifndef FTXUI_VERSION
#define FTXUI_VERSION "unknown"
#endif

struct ThingDetailEntry
{
    enum class Type {
        Param,
        State,
        Action,
        None,
    };

    Type type = Type::None;
    int index = -1;
};

struct NumericRangeInputSpec
{
    double minValue = 0.0;
    double maxValue = 0.0;
    double stepSize = 0.0;
    double displayScale = 1.0;
    bool integral = false;
    int decimals = 0;
    int displayDecimals = 0;
};

std::vector<ThingDetailEntry> buildThingDetailEntries(const api::Thing* thing, const api::ThingClass* thingClass)
{
    std::vector<ThingDetailEntry> entries;
    if (thing == nullptr) {
        return entries;
    }

    entries.reserve(thing->params.size() + thing->states.size());
    for (int index = 0; index < thing->params.size(); ++index) {
        entries.push_back({ThingDetailEntry::Type::Param, index});
    }
    for (int index = 0; index < thing->states.size(); ++index) {
        entries.push_back({ThingDetailEntry::Type::State, index});
    }
    if (thingClass != nullptr) {
        for (int index = 0; index < thingClass->actionTypes.size(); ++index) {
            entries.push_back({ThingDetailEntry::Type::Action, index});
        }
    }

    return entries;
}

std::string firstNonEmpty(std::initializer_list<std::string_view> candidates)
{
    for (const std::string_view candidate : candidates) {
        if (!candidate.empty()) {
            return std::string(candidate);
        }
    }

    return {};
}

std::string optionalQStringToStd(const std::optional<QString>& value)
{
    return value.has_value() ? value->toStdString() : std::string();
}

std::string uuidToStd(const QUuid& value)
{
    return value.toString(QUuid::WithoutBraces).toStdString();
}

void appendField(std::vector<std::string>& fields, const std::string& key, const std::string& value)
{
    if (!value.empty()) {
        fields.push_back(key + "=" + value);
    }
}

std::string jsonValueToString(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString().toStdString();
    }

    if (value.isBool()) {
        return value.toBool() ? "true" : "false";
    }

    if (value.isDouble()) {
        return QString::number(value.toDouble()).toStdString();
    }

    if (value.isNull()) {
        return "null";
    }

    if (value.isUndefined()) {
        return "<undefined>";
    }

    if (value.isArray()) {
        return QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact).toStdString();
    }

    if (value.isObject()) {
        return QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact).toStdString();
    }

    return "<invalid>";
}

std::string optionalJsonValueToString(const std::optional<QJsonValue>& value)
{
    return value.has_value() ? jsonValueToString(*value) : std::string();
}

std::string optionalJsonValuesToString(const std::optional<QList<QJsonValue>>& values)
{
    if (!values.has_value()) {
        return {};
    }

    QJsonArray array;
    for (const QJsonValue& value : *values) {
        array.append(value);
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact).toStdString();
}

std::string joinFields(const std::vector<std::string>& fields)
{
    std::string result;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index > 0) {
            result += " | ";
        }
        result += fields.at(index);
    }
    return result;
}

std::string joinCommaSeparated(const std::vector<std::string>& values)
{
    std::string result;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            result += ", ";
        }
        result += values.at(index);
    }
    return result;
}

std::string optionalBoolToString(const std::optional<bool>& value)
{
    if (!value.has_value()) {
        return {};
    }

    return *value ? "true" : "false";
}

std::string optionalDoubleToString(const std::optional<double>& value)
{
    return value.has_value() ? QString::number(*value).toStdString() : std::string();
}

std::string optionalStringListToString(const std::optional<QStringList>& value)
{
    if (!value.has_value()) {
        return {};
    }

    return "[" + value->join(QStringLiteral(", ")).toStdString() + "]";
}

std::string normalizedActionDialogValue(const api::ParamType& paramType, const std::string& rawValue);
bool actionParamUsesSelector(const api::ParamType& paramType);
std::string cycleSelectableValue(const api::ParamType& paramType, const std::string& rawValue, int delta);
std::string prettyUnit(api::Unit unit);
std::string settingsViewLabel(int view);
std::string powerActionLabel(int action);

bool isNumericRangeType(api::BasicType type)
{
    switch (type) {
    case api::BasicType::Int:
    case api::BasicType::Uint:
    case api::BasicType::Double:
    case api::BasicType::Time:
        return true;
    case api::BasicType::Bool:
    case api::BasicType::String:
    case api::BasicType::Color:
    case api::BasicType::Uuid:
    case api::BasicType::StringList:
    case api::BasicType::Object:
    case api::BasicType::Variant:
        return false;
    }

    return false;
}

bool isIntegralNumericType(api::BasicType type)
{
    return type == api::BasicType::Int || type == api::BasicType::Uint || type == api::BasicType::Time;
}

std::optional<double> parseNumericString(const QString& rawValue, api::BasicType type)
{
    const QString trimmed = rawValue.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    switch (type) {
    case api::BasicType::Int:
    case api::BasicType::Time: {
        bool ok = false;
        const qint64 value = trimmed.toLongLong(&ok);
        return ok ? std::optional<double>(static_cast<double>(value)) : std::nullopt;
    }
    case api::BasicType::Uint: {
        bool ok = false;
        const quint64 value = trimmed.toULongLong(&ok);
        return ok ? std::optional<double>(static_cast<double>(value)) : std::nullopt;
    }
    case api::BasicType::Double: {
        bool ok = false;
        const double value = trimmed.toDouble(&ok);
        return ok ? std::optional<double>(value) : std::nullopt;
    }
    case api::BasicType::Bool:
    case api::BasicType::String:
    case api::BasicType::Color:
    case api::BasicType::Uuid:
    case api::BasicType::StringList:
    case api::BasicType::Object:
    case api::BasicType::Variant:
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<double> parseNumericJsonValue(const QJsonValue& value, api::BasicType type)
{
    if (value.isDouble()) {
        return value.toDouble();
    }
    if (value.isString()) {
        return parseNumericString(value.toString(), type);
    }

    return std::nullopt;
}

double defaultNumericRangeStepSize(const NumericRangeInputSpec& spec)
{
    if (spec.integral) {
        return 1.0;
    }

    const double span = std::abs(spec.maxValue - spec.minValue);
    if (span <= 1.0) {
        return 0.01;
    }
    if (span <= 10.0) {
        return 0.1;
    }
    return 1.0;
}

int numericRangeFractionalDigits(double stepSize)
{
    if (stepSize <= 0.0) {
        return 0;
    }

    QString text = QString::number(stepSize, 'f', 6);
    while (text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }

    const int dotIndex = text.indexOf(QLatin1Char('.'));
    return dotIndex >= 0 ? text.size() - dotIndex - 1 : 0;
}

bool shouldScaleRangeDisplayAsPercentage(const api::ParamType& paramType, double minValue, double maxValue)
{
    return paramType.unit.has_value() && *paramType.unit == api::Unit::UnitPercentage && minValue >= 0.0 && maxValue > 0.0 && maxValue <= 1.0 + 1e-9;
}

QString trimTrailingZeros(QString value)
{
    while (value.endsWith(QLatin1Char('0'))) {
        value.chop(1);
    }
    if (value.endsWith(QLatin1Char('.'))) {
        value.chop(1);
    }
    if (value == QStringLiteral("-0")) {
        return QStringLiteral("0");
    }
    return value;
}

std::optional<NumericRangeInputSpec> numericRangeInputSpec(const api::ParamType& paramType)
{
    if (!isNumericRangeType(paramType.type)) {
        return std::nullopt;
    }
    if (!paramType.minValue.has_value() || !paramType.maxValue.has_value()) {
        return std::nullopt;
    }
    if (paramType.allowedValues.has_value() && !paramType.allowedValues->empty()) {
        return std::nullopt;
    }

    const std::optional<double> minValue = parseNumericJsonValue(*paramType.minValue, paramType.type);
    const std::optional<double> maxValue = parseNumericJsonValue(*paramType.maxValue, paramType.type);
    if (!minValue.has_value() || !maxValue.has_value()) {
        return std::nullopt;
    }

    NumericRangeInputSpec spec;
    spec.minValue = std::min(*minValue, *maxValue);
    spec.maxValue = std::max(*minValue, *maxValue);
    spec.integral = isIntegralNumericType(paramType.type);
    spec.stepSize = paramType.stepSize.value_or(0.0);
    if (spec.stepSize <= 0.0) {
        spec.stepSize = defaultNumericRangeStepSize(spec);
    }
    if (shouldScaleRangeDisplayAsPercentage(paramType, spec.minValue, spec.maxValue)) {
        spec.displayScale = 100.0;
    }
    spec.decimals = spec.integral ? 0 : numericRangeFractionalDigits(spec.stepSize);
    spec.displayDecimals = numericRangeFractionalDigits(spec.stepSize * spec.displayScale);
    return spec;
}

double clampAndSnapNumericRangeValue(const NumericRangeInputSpec& spec, double value)
{
    value = std::clamp(value, spec.minValue, spec.maxValue);

    if (spec.stepSize > 0.0) {
        const double stepCount = std::round((value - spec.minValue) / spec.stepSize);
        value = spec.minValue + stepCount * spec.stepSize;
        value = std::clamp(value, spec.minValue, spec.maxValue);
    }

    if (spec.integral) {
        value = std::round(value);
    }

    if (std::abs(value) < 1e-9) {
        value = 0.0;
    }

    return value;
}

double rawNumericRangeValueToDisplay(const NumericRangeInputSpec& spec, double rawValue)
{
    return rawValue * spec.displayScale;
}

double displayNumericRangeValueToRaw(const NumericRangeInputSpec& spec, double displayValue)
{
    return displayValue / spec.displayScale;
}

std::string formatRawNumericRangeValue(const api::ParamType& paramType, const NumericRangeInputSpec& spec, double value)
{
    const double clampedValue = clampAndSnapNumericRangeValue(spec, value);
    if (isIntegralNumericType(paramType.type)) {
        return std::to_string(static_cast<long long>(std::llround(clampedValue)));
    }

    return trimTrailingZeros(QString::number(clampedValue, 'f', std::max(spec.decimals, 1))).toStdString();
}

std::string formatNumericRangeValue(const api::ParamType& paramType, const NumericRangeInputSpec& spec, double rawValue)
{
    const double clampedValue = clampAndSnapNumericRangeValue(spec, rawValue);
    const double displayValue = rawNumericRangeValueToDisplay(spec, clampedValue);
    if (isIntegralNumericType(paramType.type) && spec.displayScale == 1.0) {
        return std::to_string(static_cast<long long>(std::llround(displayValue)));
    }

    return trimTrailingZeros(QString::number(displayValue, 'f', spec.displayDecimals)).toStdString();
}

std::optional<double> parseDisplayedNumericString(const QString& rawValue, const NumericRangeInputSpec& spec, api::BasicType type)
{
    const QString trimmed = rawValue.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    if (spec.displayScale != 1.0 || type == api::BasicType::Double) {
        bool ok = false;
        const double value = trimmed.toDouble(&ok);
        return ok ? std::optional<double>(value) : std::nullopt;
    }

    return parseNumericString(trimmed, type);
}

std::optional<double> currentNumericRangeValue(const api::ParamType& paramType, const NumericRangeInputSpec& spec, const std::string& rawValue)
{
    if (!rawValue.empty()) {
        const std::optional<double> parsedDisplayValue = parseDisplayedNumericString(QString::fromStdString(rawValue), spec, paramType.type);
        if (!parsedDisplayValue.has_value()) {
            return std::nullopt;
        }
        return clampAndSnapNumericRangeValue(spec, displayNumericRangeValueToRaw(spec, *parsedDisplayValue));
    }

    if (paramType.defaultValue.has_value()) {
        const std::optional<double> defaultValue = parseNumericJsonValue(*paramType.defaultValue, paramType.type);
        if (defaultValue.has_value()) {
            return clampAndSnapNumericRangeValue(spec, *defaultValue);
        }
    }

    return clampAndSnapNumericRangeValue(spec, spec.minValue);
}

std::string normalizedActionSubmissionValue(const api::ParamType& paramType, const std::string& rawValue)
{
    if (const std::optional<NumericRangeInputSpec> rangeSpec = numericRangeInputSpec(paramType); rangeSpec.has_value()) {
        const std::optional<double> currentValue = currentNumericRangeValue(paramType, *rangeSpec, rawValue);
        if (currentValue.has_value()) {
            return formatRawNumericRangeValue(paramType, *rangeSpec, *currentValue);
        }
        return rawValue;
    }

    return normalizedActionDialogValue(paramType, rawValue);
}

bool actionParamUsesRangeInput(const api::ParamType& paramType)
{
    return numericRangeInputSpec(paramType).has_value();
}

bool rangeInputAcceptsCharacter(const api::ParamType& paramType, const std::string& currentValue, const std::string& typedCharacter)
{
    if (typedCharacter.size() != 1) {
        return false;
    }

    const char character = typedCharacter.front();
    if (character >= '0' && character <= '9') {
        return true;
    }

    if (character == '-' && paramType.type != api::BasicType::Uint) {
        return currentValue.empty();
    }

    const std::optional<NumericRangeInputSpec> rangeSpec = numericRangeInputSpec(paramType);
    const bool acceptsDecimalPoint = paramType.type == api::BasicType::Double || (rangeSpec.has_value() && rangeSpec->displayScale != 1.0);
    if (character == '.' && acceptsDecimalPoint) {
        return currentValue.find('.') == std::string::npos;
    }

    return false;
}

std::string rangeInputUnitLabel(const api::ParamType& paramType)
{
    if (!paramType.unit.has_value()) {
        return {};
    }

    return prettyUnit(*paramType.unit);
}

std::string rangeInputSummary(const api::ParamType& paramType)
{
    const std::optional<NumericRangeInputSpec> spec = numericRangeInputSpec(paramType);
    if (!spec.has_value()) {
        return {};
    }

    const std::string unitLabel = rangeInputUnitLabel(paramType);
    std::string summary = "Range: " + formatNumericRangeValue(paramType, *spec, spec->minValue) + " .. " + formatNumericRangeValue(paramType, *spec, spec->maxValue);
    if (!unitLabel.empty()) {
        summary += " " + unitLabel;
    }
    summary += " | Step: " + formatNumericRangeValue(paramType, *spec, spec->stepSize);
    if (!unitLabel.empty()) {
        summary += " " + unitLabel;
    }
    return summary;
}

std::string cycleRangeValue(const api::ParamType& paramType, const std::string& rawValue, int delta)
{
    const std::optional<NumericRangeInputSpec> spec = numericRangeInputSpec(paramType);
    if (!spec.has_value()) {
        return rawValue;
    }

    const double currentValue = currentNumericRangeValue(paramType, *spec, rawValue).value_or(spec->minValue);
    const double nextValue = currentValue + static_cast<double>(delta) * spec->stepSize;
    return formatNumericRangeValue(paramType, *spec, nextValue);
}

std::string formatRangeDisplayValue(const api::ParamType& paramType, const NumericRangeInputSpec& spec, double value)
{
    std::string displayValue = formatNumericRangeValue(paramType, spec, value);
    if (const std::string unitLabel = rangeInputUnitLabel(paramType); !unitLabel.empty()) {
        displayValue += " " + unitLabel;
    }
    return displayValue;
}

ftxui::Element renderRangeValueCell(const api::ParamType& paramType, const std::string& rawValue)
{
    const std::optional<NumericRangeInputSpec> spec = numericRangeInputSpec(paramType);
    if (!spec.has_value()) {
        return ftxui::text(rawValue.empty() ? std::string("<empty>") : rawValue);
    }

    const std::optional<double> currentValue = currentNumericRangeValue(paramType, *spec, rawValue);
    if (!currentValue.has_value()) {
        return ftxui::text(rawValue + " (invalid)") | ftxui::color(ftxui::Color::YellowLight);
    }

    const double span = spec->maxValue - spec->minValue;
    const double ratio = span <= 0.0 ? 1.0 : std::clamp((*currentValue - spec->minValue) / span, 0.0, 1.0);
    constexpr int kBarWidth = 16;
    const int filledCells = std::clamp(static_cast<int>(std::round(ratio * static_cast<double>(kBarWidth))), 0, kBarWidth);
    const std::string bar = "[" + std::string(static_cast<std::size_t>(filledCells), '=') + std::string(static_cast<std::size_t>(kBarWidth - filledCells), '.') + "]";

    return ftxui::hbox({
        ftxui::text(formatRangeDisplayValue(paramType, *spec, *currentValue)),
        ftxui::text(" "),
        ftxui::text(bar) | ftxui::color(ftxui::Color::GreenLight),
    });
}

bool handleParamValueEditEvent(const ftxui::Event& event, const api::ParamType& paramType, std::string& currentValue, bool& isRangeTextEditing)
{
    if (actionParamUsesSelector(paramType)) {
        isRangeTextEditing = false;
        if (event == ftxui::Event::ArrowLeft) {
            currentValue = cycleSelectableValue(paramType, currentValue, -1);
            return true;
        }
        if (event == ftxui::Event::ArrowRight || event == ftxui::Event::Character(" ")) {
            currentValue = cycleSelectableValue(paramType, currentValue, 1);
            return true;
        }
        return false;
    }

    if (actionParamUsesRangeInput(paramType)) {
        if (event == ftxui::Event::ArrowLeft) {
            currentValue = cycleRangeValue(paramType, currentValue, -1);
            isRangeTextEditing = false;
            return true;
        }
        if (event == ftxui::Event::ArrowRight) {
            currentValue = cycleRangeValue(paramType, currentValue, 1);
            isRangeTextEditing = false;
            return true;
        }
        if (event == ftxui::Event::Backspace && !currentValue.empty()) {
            isRangeTextEditing = true;
            currentValue.pop_back();
            return true;
        }
        if (event.is_character() && rangeInputAcceptsCharacter(paramType, currentValue, event.character())) {
            if (!isRangeTextEditing) {
                currentValue.clear();
            }
            isRangeTextEditing = true;
            currentValue += event.character();
            return true;
        }
        return false;
    }

    isRangeTextEditing = false;
    if (event == ftxui::Event::Backspace && !currentValue.empty()) {
        currentValue.pop_back();
        return true;
    }
    if (event.is_character()) {
        currentValue += event.character();
        return true;
    }

    return false;
}

std::string thingErrorLabel(api::ThingError error)
{
    std::string value = api::toString(error).toStdString();
    constexpr std::string_view prefix = "ThingError";
    if (value.rfind(prefix.data(), 0) == 0) {
        value.erase(0, prefix.size());
    }
    return value;
}

std::string createMethodLabel(api::CreateMethod createMethod)
{
    std::string value = api::toString(createMethod).toStdString();
    constexpr std::string_view prefix = "CreateMethod";
    if (value.rfind(prefix.data(), 0) == 0) {
        value.erase(0, prefix.size());
    }
    return value;
}

std::string setupMethodLabel(api::SetupMethod setupMethod)
{
    std::string value = api::toString(setupMethod).toStdString();
    constexpr std::string_view prefix = "SetupMethod";
    if (value.rfind(prefix.data(), 0) == 0) {
        value.erase(0, prefix.size());
    }
    return value;
}

std::string settingsViewLabel(int view)
{
    switch (view) {
    case 0:
        return "Server info";
    case 1:
        return "Timezone";
    case 2:
        return "Update";
    case 3:
        return "Shutdown";
    case 4:
        return "Restart";
    case 5:
        return "Reboot";
    }

    return "Settings";
}

std::string powerActionLabel(int action)
{
    switch (action) {
    case 0:
        return "Shutdown";
    case 1:
        return "Restart";
    case 2:
        return "Reboot";
    }

    return "Power action";
}

std::string thingClassLabel(const api::ThingClass& thingClass)
{
    return firstNonEmpty({thingClass.displayName.toStdString(), thingClass.name.toStdString(), uuidToStd(thingClass.id), "<unknown thing class>"});
}

std::string descriptorLabel(const api::ThingDescriptor& descriptor)
{
    return firstNonEmpty({descriptor.title.toStdString(), descriptor.description.toStdString(), uuidToStd(descriptor.id), "<unknown discovery result>"});
}

bool containsCreateMethod(const api::ThingClass& thingClass, api::CreateMethod createMethod)
{
    return std::find(thingClass.createMethods.begin(), thingClass.createMethods.end(), createMethod) != thingClass.createMethods.end();
}

bool isThingClassAddable(const api::ThingClass& thingClass)
{
    return containsCreateMethod(thingClass, api::CreateMethod::CreateMethodUser) || containsCreateMethod(thingClass, api::CreateMethod::CreateMethodDiscovery);
}

bool caseInsensitiveContains(const QString& value, const QString& needle)
{
    return needle.trimmed().isEmpty() || value.contains(needle, Qt::CaseInsensitive);
}

bool caseInsensitiveContainsAny(const QString& value, std::initializer_list<const char*> needles)
{
    for (const char* needle : needles) {
        if (value.contains(QString::fromLatin1(needle), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

std::string formatActionInvocation(const std::string& actionName, const std::vector<api::ParamType>& paramTypes, const std::vector<std::string>& paramValues)
{
    std::vector<std::string> values;
    values.reserve(paramTypes.size());
    for (int index = 0; index < static_cast<int>(paramTypes.size()) && index < static_cast<int>(paramValues.size()); ++index) {
        const api::ParamType& paramType = paramTypes.at(index);
        const std::string label = firstNonEmpty({paramType.displayName.toStdString(), paramType.name.toStdString(), "param"});
        const std::string value = normalizedActionDialogValue(paramType, paramValues.at(index));
        values.push_back(label + "=" + (value.empty() ? std::string("<empty>") : value));
    }
    return actionName + "(" + joinCommaSeparated(values) + ")";
}

std::string formatActionExecutionStatus(int requestId, const std::string& actionInvocation, const std::string& result)
{
    return std::to_string(requestId) + " : " + actionInvocation + ": " + result;
}

std::string busyIndicator(std::chrono::steady_clock::time_point startedAt)
{
    static constexpr std::array<std::string_view, 4> frames = {"|", "/", "-", "\\"};
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startedAt).count();
    const std::size_t index = static_cast<std::size_t>((elapsed / 150) % static_cast<long long>(frames.size()));
    return std::string(frames.at(index));
}

std::string thingLabel(const api::Thing* thing)
{
    if (thing == nullptr) {
        return std::string("<unknown thing>");
    }

    return firstNonEmpty({optionalQStringToStd(thing->name), uuidToStd(thing->id), "<unknown thing>"});
}

std::string prettyUnit(api::Unit unit)
{
    switch (unit) {
    case api::Unit::UnitMilliSeconds:
        return "ms";
    case api::Unit::UnitSeconds:
        return "s";
    case api::Unit::UnitMinutes:
        return "min";
    case api::Unit::UnitHours:
        return "h";
    case api::Unit::UnitUnixTime:
        return "unix";
    case api::Unit::UnitMeterPerSecond:
        return "m/s";
    case api::Unit::UnitKiloMeterPerHour:
        return "km/h";
    case api::Unit::UnitDegree:
        return "\u00b0";
    case api::Unit::UnitRadiant:
        return "rad";
    case api::Unit::UnitDegreeCelsius:
        return "\u00b0C";
    case api::Unit::UnitDegreeKelvin:
        return "K";
    case api::Unit::UnitMired:
        return "mired";
    case api::Unit::UnitMilliBar:
        return "mbar";
    case api::Unit::UnitBar:
        return "bar";
    case api::Unit::UnitPascal:
        return "Pa";
    case api::Unit::UnitHectoPascal:
        return "hPa";
    case api::Unit::UnitAtmosphere:
        return "atm";
    case api::Unit::UnitLumen:
        return "lm";
    case api::Unit::UnitLux:
        return "lx";
    case api::Unit::UnitCandela:
        return "cd";
    case api::Unit::UnitMilliMeter:
        return "mm";
    case api::Unit::UnitCentiMeter:
        return "cm";
    case api::Unit::UnitMeter:
        return "m";
    case api::Unit::UnitKiloMeter:
        return "km";
    case api::Unit::UnitGram:
        return "g";
    case api::Unit::UnitKiloGram:
        return "kg";
    case api::Unit::UnitDezibel:
        return "dB";
    case api::Unit::UnitBpm:
        return "bpm";
    case api::Unit::UnitKiloByte:
        return "kB";
    case api::Unit::UnitMegaByte:
        return "MB";
    case api::Unit::UnitGigaByte:
        return "GB";
    case api::Unit::UnitTeraByte:
        return "TB";
    case api::Unit::UnitMilliWatt:
        return "mW";
    case api::Unit::UnitWatt:
        return "W";
    case api::Unit::UnitKiloWatt:
        return "kW";
    case api::Unit::UnitKiloWattHour:
        return "kWh";
    case api::Unit::UnitEuroPerMegaWattHour:
        return "\u20ac/MWh";
    case api::Unit::UnitEuroCentPerKiloWattHour:
        return "ct/kWh";
    case api::Unit::UnitPercentage:
        return "%";
    case api::Unit::UnitPartsPerMillion:
        return "ppm";
    case api::Unit::UnitPartsPerBillion:
        return "ppb";
    case api::Unit::UnitEuro:
        return "\u20ac";
    case api::Unit::UnitDollar:
        return "$";
    case api::Unit::UnitHertz:
        return "Hz";
    case api::Unit::UnitAmpere:
        return "A";
    case api::Unit::UnitMilliAmpere:
        return "mA";
    case api::Unit::UnitVolt:
        return "V";
    case api::Unit::UnitMilliVolt:
        return "mV";
    case api::Unit::UnitVoltAmpere:
        return "VA";
    case api::Unit::UnitVoltAmpereReactive:
        return "var";
    case api::Unit::UnitAmpereHour:
        return "Ah";
    case api::Unit::UnitOhm:
        return "Ohm";
    case api::Unit::UnitMicroSiemensPerCentimeter:
        return "uS/cm";
    case api::Unit::UnitDuration:
        return "duration";
    case api::Unit::UnitNewton:
        return "N";
    case api::Unit::UnitNewtonMeter:
        return "Nm";
    case api::Unit::UnitRpm:
        return "rpm";
    case api::Unit::UnitMilligramPerLiter:
        return "mg/L";
    case api::Unit::UnitLiter:
        return "L";
    case api::Unit::UnitMicroGrammPerCubicalMeter:
        return "ug/m3";
    case api::Unit::UnitNone:
        return {};
    }

    return api::toString(unit).toStdString();
}

std::optional<ftxui::Color> parseHexColor(const std::string& value)
{
    if (value.empty()) {
        return std::nullopt;
    }

    std::string hex = value;
    if (hex.front() == '#') {
        hex.erase(hex.begin());
    }

    if (hex.size() == 8) {
        hex = hex.substr(2);
    }

    if (hex.size() != 6) {
        return std::nullopt;
    }

    auto parseComponent = [&](int offset) -> std::optional<int> {
        bool ok = false;
        const int component = QString::fromStdString(hex.substr(offset, 2)).toInt(&ok, 16);
        if (!ok) {
            return std::nullopt;
        }
        return component;
    };

    const std::optional<int> red = parseComponent(0);
    const std::optional<int> green = parseComponent(2);
    const std::optional<int> blue = parseComponent(4);
    if (!red.has_value() || !green.has_value() || !blue.has_value()) {
        return std::nullopt;
    }

    return ftxui::Color::RGB(*red, *green, *blue);
}

ftxui::Element renderBoolValue(bool value)
{
    const ftxui::Color color = value ? ftxui::Color::GreenLight : ftxui::Color::RedLight;
    return ftxui::hbox({
        ftxui::text("\u25cf") | ftxui::color(color),
        ftxui::text(std::string(" ") + (value ? "true" : "false")),
    });
}

ftxui::Element renderColorValue(const std::string& colorString)
{
    auto swatch = ftxui::text("    ");
    if (const std::optional<ftxui::Color> parsedColor = parseHexColor(colorString); parsedColor.has_value()) {
        swatch = swatch | ftxui::bgcolor(*parsedColor);
    }

    return ftxui::hbox({
        ftxui::text(colorString.empty() ? std::string("<empty>") : colorString),
        ftxui::text(" "),
        swatch,
    });
}

std::optional<std::string> formatUnixTimestampValue(const QJsonValue& value)
{
    qint64 secondsSinceEpoch = 0;
    if (value.isDouble()) {
        secondsSinceEpoch = static_cast<qint64>(std::llround(value.toDouble()));
    } else if (value.isString()) {
        bool ok = false;
        secondsSinceEpoch = value.toString().toLongLong(&ok);
        if (!ok) {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }

    const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(secondsSinceEpoch);
    if (!dateTime.isValid()) {
        return std::nullopt;
    }

    return dateTime.toString(QStringLiteral("yyyy.MM.dd HH:mm:ss")).toStdString();
}

ftxui::Element renderValueCell(const QJsonValue& value, std::optional<api::BasicType> basicType, std::optional<api::Unit> unit)
{
    if ((basicType.has_value() && *basicType == api::BasicType::Bool) || value.isBool()) {
        return renderBoolValue(value.toBool());
    }

    if (basicType.has_value() && *basicType == api::BasicType::Color && value.isString()) {
        return renderColorValue(value.toString().toStdString());
    }

    if (unit.has_value() && *unit == api::Unit::UnitUnixTime) {
        if (const std::optional<std::string> formattedTimestamp = formatUnixTimestampValue(value); formattedTimestamp.has_value()) {
            return ftxui::text(*formattedTimestamp);
        }
    }

    std::string renderedValue = jsonValueToString(value);
    if (unit.has_value()) {
        const std::string renderedUnit = prettyUnit(*unit);
        if (!renderedUnit.empty() && !renderedValue.empty()) {
            renderedValue += " " + renderedUnit;
        }
    }

    return ftxui::text(renderedValue);
}

ftxui::Element renderTwoColumnRow(const std::string& name, ftxui::Element value, bool selected, bool focused)
{
    if (selected && focused) {
        value = renderActiveField(std::move(value), true, 14);
    }

    auto row = ftxui::hbox({
        ftxui::text(name) | ftxui::xflex,
        ftxui::text(" "),
        std::move(value),
    });

    if (selected) {
        row = row | ftxui::inverted | ftxui::bold;
    }
    if (selected && focused) {
        row = row | ftxui::color(ftxui::Color::CyanLight);
    }
    if (selected) {
        row = row | ftxui::focus;
    }

    return row;
}

std::optional<QJsonValue> parseActionInputValue(const std::string& input, api::BasicType type)
{
    const QString trimmed = QString::fromStdString(input).trimmed();

    switch (type) {
    case api::BasicType::Bool:
        if (trimmed.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || trimmed == QStringLiteral("1") || trimmed.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0
            || trimmed.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0) {
            return QJsonValue(true);
        }
        if (trimmed.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0 || trimmed == QStringLiteral("0") || trimmed.compare(QStringLiteral("no"), Qt::CaseInsensitive) == 0
            || trimmed.compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0) {
            return QJsonValue(false);
        }
        return std::nullopt;
    case api::BasicType::Int:
    case api::BasicType::Time: {
        bool ok = false;
        const qint64 value = trimmed.toLongLong(&ok);
        return ok ? std::optional<QJsonValue>(QJsonValue(value)) : std::nullopt;
    }
    case api::BasicType::Uint: {
        bool ok = false;
        const quint64 value = trimmed.toULongLong(&ok);
        return ok ? std::optional<QJsonValue>(QJsonValue(static_cast<qint64>(value))) : std::nullopt;
    }
    case api::BasicType::Double: {
        bool ok = false;
        const double value = trimmed.toDouble(&ok);
        return ok ? std::optional<QJsonValue>(QJsonValue(value)) : std::nullopt;
    }
    case api::BasicType::String:
    case api::BasicType::Color:
    case api::BasicType::Uuid:
        return QJsonValue(trimmed);
    case api::BasicType::StringList: {
        QJsonArray array;
        for (const QString& item : trimmed.split(',', Qt::SkipEmptyParts)) {
            array.append(item.trimmed());
        }
        return QJsonValue(array);
    }
    case api::BasicType::Object:
    case api::BasicType::Variant: {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError) {
            if (document.isObject()) {
                return QJsonValue(document.object());
            }
            if (document.isArray()) {
                return QJsonValue(document.array());
            }
        }
        return QJsonValue(trimmed);
    }
    }

    return QJsonValue(trimmed);
}

bool buildParamList(const std::vector<api::ParamType>& paramTypes, const std::vector<std::string>& rawValues, api::ParamList& params, std::string& errorMessage)
{
    params.clear();
    errorMessage.clear();

    for (int index = 0; index < static_cast<int>(paramTypes.size()) && index < static_cast<int>(rawValues.size()); ++index) {
        const api::ParamType& paramType = paramTypes.at(index);
        const std::string rawValue = normalizedActionSubmissionValue(paramType, rawValues.at(index));
        const std::optional<QJsonValue> parsedValue = parseActionInputValue(rawValue, paramType.type);
        if (!parsedValue.has_value()) {
            errorMessage = "Invalid value for " + firstNonEmpty({paramType.displayName.toStdString(), paramType.name.toStdString(), "param"}) + ".";
            return false;
        }

        api::Param param;
        param.paramTypeId = paramType.id;
        param.value = *parsedValue;
        params.append(param);
    }

    return true;
}

std::vector<QJsonValue> selectableValuesForParamType(const api::ParamType& paramType)
{
    if (paramType.allowedValues.has_value() && !paramType.allowedValues->empty()) {
        return {paramType.allowedValues->begin(), paramType.allowedValues->end()};
    }

    if (paramType.type == api::BasicType::Bool) {
        return {QJsonValue(false), QJsonValue(true)};
    }

    return {};
}

std::string normalizedActionDialogValue(const api::ParamType& paramType, const std::string& rawValue)
{
    if (const std::optional<NumericRangeInputSpec> rangeSpec = numericRangeInputSpec(paramType); rangeSpec.has_value()) {
        const std::optional<double> currentValue = currentNumericRangeValue(paramType, *rangeSpec, rawValue);
        if (currentValue.has_value()) {
            return formatNumericRangeValue(paramType, *rangeSpec, *currentValue);
        }
        if (!rawValue.empty()) {
            return rawValue;
        }
    }

    if (!rawValue.empty()) {
        return rawValue;
    }

    if (paramType.defaultValue.has_value()) {
        return jsonValueToString(*paramType.defaultValue);
    }

    const std::vector<QJsonValue> selectableValues = selectableValuesForParamType(paramType);
    if (!selectableValues.empty()) {
        return jsonValueToString(selectableValues.front());
    }

    if (const std::optional<NumericRangeInputSpec> rangeSpec = numericRangeInputSpec(paramType); rangeSpec.has_value()) {
        return formatNumericRangeValue(paramType, *rangeSpec, rangeSpec->minValue);
    }

    return {};
}

std::optional<int> selectableValueIndex(const api::ParamType& paramType, const std::string& rawValue)
{
    const std::vector<QJsonValue> selectableValues = selectableValuesForParamType(paramType);
    if (selectableValues.empty()) {
        return std::nullopt;
    }

    const std::string normalizedValue = normalizedActionDialogValue(paramType, rawValue);
    for (int index = 0; index < static_cast<int>(selectableValues.size()); ++index) {
        if (jsonValueToString(selectableValues.at(index)) == normalizedValue) {
            return index;
        }
    }

    return 0;
}

bool actionParamUsesSelector(const api::ParamType& paramType)
{
    return !selectableValuesForParamType(paramType).empty();
}

std::string cycleSelectableValue(const api::ParamType& paramType, const std::string& rawValue, int delta)
{
    const std::vector<QJsonValue> selectableValues = selectableValuesForParamType(paramType);
    if (selectableValues.empty()) {
        return rawValue;
    }

    const int currentIndex = selectableValueIndex(paramType, rawValue).value_or(0);
    const int nextIndex = (currentIndex + delta + static_cast<int>(selectableValues.size())) % static_cast<int>(selectableValues.size());
    return jsonValueToString(selectableValues.at(nextIndex));
}

ftxui::Element renderActionDialogValueCell(const api::ParamType& paramType, const std::string& rawValue)
{
    const std::string normalizedValue = normalizedActionDialogValue(paramType, rawValue);

    if (paramType.type == api::BasicType::Bool) {
        const std::optional<QJsonValue> parsedValue = parseActionInputValue(normalizedValue, api::BasicType::Bool);
        return renderBoolValue(parsedValue.has_value() ? parsedValue->toBool() : false);
    }

    if (actionParamUsesRangeInput(paramType)) {
        return renderRangeValueCell(paramType, rawValue);
    }

    if (actionParamUsesSelector(paramType)) {
        return ftxui::text("< " + normalizedValue + " >");
    }

    return ftxui::text(normalizedValue.empty() ? std::string("<empty>") : normalizedValue);
}

} // namespace
#endif
} // namespace
} // namespace nymea

#include "engineutils.cpp"

namespace nymea {

namespace {

std::string settingsViewLabel(int view)
{
    switch (view) {
    case 0:
        return "Server info";
    case 1:
        return "Timezone";
    case 2:
        return "Update";
    case 3:
        return "Shutdown";
    case 4:
        return "Restart";
    case 5:
        return "Reboot";
    }

    return "Settings";
}

std::string powerActionLabel(int action)
{
    switch (action) {
    case 0:
        return "Shutdown";
    case 1:
        return "Restart";
    case 2:
        return "Reboot";
    }

    return "Power action";
}

constexpr int timezoneSearchLineIndex = 4;
constexpr int timezoneListStartLineIndex = 7;

int nextTimezoneDetailsLineIndex(int currentIndex, int direction, int filteredCount)
{
    if (filteredCount <= 0) {
        return timezoneSearchLineIndex;
    }

    const int firstResultLineIndex = timezoneListStartLineIndex;
    const int lastResultLineIndex = firstResultLineIndex + filteredCount - 1;

    if (currentIndex == timezoneSearchLineIndex) {
        return direction > 0 ? firstResultLineIndex : lastResultLineIndex;
    }

    if (currentIndex < firstResultLineIndex) {
        return direction > 0 ? firstResultLineIndex : timezoneSearchLineIndex;
    }

    if (currentIndex <= lastResultLineIndex) {
        if (direction > 0) {
            return currentIndex == lastResultLineIndex ? timezoneSearchLineIndex : currentIndex + 1;
        }
        return currentIndex == firstResultLineIndex ? timezoneSearchLineIndex : currentIndex - 1;
    }

    return direction > 0 ? timezoneSearchLineIndex : lastResultLineIndex;
}

} // namespace

Engine::Engine(EngineOptions options)
    : m_options(std::move(options))
    , m_username(m_options.username)
    , m_password(m_options.password)
{
    m_client.setNotificationHandler([this](const QJsonObject& message) { enqueueUiTask([this, message]() { handleNotification(message); }); });
    m_client.setStateHandler([this](bool connected, bool encrypted, const QString& peerCertificateFingerprint, const QString& authToken, const QString& lastError) {
        enqueueUiTask([this, connected, encrypted, peerCertificateFingerprint, authToken, lastError]() {
            handleClientStateChanged(connected, encrypted, peerCertificateFingerprint, authToken, lastError);
        });
    });
    m_passwordInputOption.password = true;
    m_usernameInput = ftxui::Input(&m_username, "Username");
    m_passwordInput = ftxui::Input(&m_password, "Password", m_passwordInputOption);
    m_loginForm = ftxui::Container::Vertical({m_usernameInput, m_passwordInput});
}

int Engine::run()
{
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse();
    m_screen = &screen;
    auto ui = ftxui::Renderer([this] { return renderUi(); });
    auto withKeyHandler = ftxui::CatchEvent(ui, [&](ftxui::Event event) { return handleEvent(event, screen); });

    connectToServer();
    if (m_client.isConnected()) {
        runHandshakeAndLoadThings();
    }

    screen.Loop(withKeyHandler);
    m_screen = nullptr;
    return 0;
}

std::string Engine::endpoint() const
{
    return std::string(m_options.useSsl ? "ssl://" : "tcp://") + m_options.host.toStdString() + ":" + std::to_string(static_cast<unsigned>(m_options.port));
}

std::string Engine::connectionDisplayName() const
{
    if (!m_serverName.isEmpty()) {
        return m_serverName.toStdString();
    }

    if (m_savedConnection.has_value() && !m_savedConnection->name.isEmpty()) {
        return m_savedConnection->name.toStdString();
    }

    return endpoint();
}

SavedConnection Engine::currentConnection(bool allowFingerprintUpdate) const
{
    SavedConnection connection;
    connection.hostUuid = m_serverUuid;
    connection.name = m_serverName;
    connection.host = m_options.host;
    connection.port = m_options.port;
    connection.useSsl = m_options.useSsl;
    connection.certificateFingerprint = m_client.peerCertificateFingerprint();
    connection.token = m_client.authToken();

    if (!allowFingerprintUpdate && m_savedConnection.has_value() && m_savedConnection->hostUuid == m_serverUuid && !m_savedConnection->certificateFingerprint.isEmpty()
        && m_savedConnection->certificateFingerprint != connection.certificateFingerprint) {
        connection.certificateFingerprint = m_savedConnection->certificateFingerprint;
    }

    if (connection.token.isEmpty() && m_savedConnection.has_value() && m_savedConnection->hostUuid == m_serverUuid) {
        connection.token = m_savedConnection->token;
    }

    return connection;
}

std::optional<QUuid> Engine::currentTokenId() const
{
    const QUuid liveTokenId(m_client.authToken());
    if (!liveTokenId.isNull()) {
        return liveTokenId;
    }

    if (m_savedConnection.has_value()) {
        const QUuid savedTokenId(m_savedConnection->token);
        if (!savedTokenId.isNull()) {
            return savedTokenId;
        }
    }

    return std::nullopt;
}

#if 0
void Engine::clampThingSelection(const QUuid& preferredThingId)
{
    const std::vector<const api::Thing*> things = filteredThings();
    if (things.empty()) {
        m_selectedThingIndex = 0;
        m_showThingDetailInspector = false;
        if (m_focusArea == FocusArea::ThingDetails) {
            m_focusArea = FocusArea::ThingList;
        }
        return;
    }

    if (!preferredThingId.isNull()) {
        for (int index = 0; index < static_cast<int>(things.size()); ++index) {
            if (things.at(index)->id == preferredThingId) {
                m_selectedThingIndex = index;
                return;
            }
        }
    }

    if (m_selectedThingIndex < 0) {
        m_selectedThingIndex = 0;
    } else if (m_selectedThingIndex >= static_cast<int>(things.size())) {
        m_selectedThingIndex = static_cast<int>(things.size()) - 1;
    }
}

void Engine::clampConfigureThingClassSelection()
{
    const std::vector<api::ThingClass> thingClasses = filteredConfigThingClasses();
    if (thingClasses.empty()) {
        m_selectedConfigureThingClassIndex = 0;
        if (m_focusArea == FocusArea::ConfigureThingClassList) {
            m_focusArea = FocusArea::ConfigureThingClassSearch;
        }
        return;
    }

    if (m_selectedConfigureThingClassIndex < 0) {
        m_selectedConfigureThingClassIndex = 0;
    } else if (m_selectedConfigureThingClassIndex >= static_cast<int>(thingClasses.size())) {
        m_selectedConfigureThingClassIndex = static_cast<int>(thingClasses.size()) - 1;
    }
}

void Engine::clampConfigureThingSelection()
{
    if (m_thingManager.things().empty()) {
        m_selectedConfigureThingIndex = 0;
        if (m_focusArea == FocusArea::ConfigureThingSelection) {
            m_focusArea = FocusArea::ConfigureMenu;
        }
        return;
    }

    if (m_selectedConfigureThingIndex < 0) {
        m_selectedConfigureThingIndex = 0;
    } else if (m_selectedConfigureThingIndex >= static_cast<int>(m_thingManager.things().size())) {
        m_selectedConfigureThingIndex = static_cast<int>(m_thingManager.things().size()) - 1;
    }
}

int Engine::thingDetailEntryCount() const
{
    const api::Thing* thing = selectedThing();
    const api::ThingClass* thingClass = thing != nullptr ? m_thingManager.thingClassForThing(*thing) : nullptr;
    return static_cast<int>(buildThingDetailEntries(thing, thingClass).size());
}

void Engine::clampThingDetailSelection()
{
    const int entryCount = thingDetailEntryCount();
    if (entryCount <= 0) {
        m_selectedThingDetailIndex = 0;
        m_showThingDetailInspector = false;
        return;
    }

    if (m_selectedThingDetailIndex < 0) {
        m_selectedThingDetailIndex = 0;
    } else if (m_selectedThingDetailIndex >= entryCount) {
        m_selectedThingDetailIndex = entryCount - 1;
    }
}

void Engine::resetThingDetailSelection()
{
    m_selectedThingDetailIndex = 0;
    m_showThingDetailInspector = false;
    closeActionDialog();
    clampThingDetailSelection();
}

bool Engine::openSelectedActionDialog()
{
    const api::Thing* thing = selectedThing();
    if (thing == nullptr) {
        return false;
    }

    const api::ThingClass* thingClass = m_thingManager.thingClassForThing(*thing);
    const std::vector<ThingDetailEntry> detailEntries = buildThingDetailEntries(thing, thingClass);
    if (m_selectedThingDetailIndex < 0 || m_selectedThingDetailIndex >= static_cast<int>(detailEntries.size())) {
        return false;
    }

    const ThingDetailEntry& selectedEntry = detailEntries.at(m_selectedThingDetailIndex);
    if (selectedEntry.type != ThingDetailEntry::Type::Action) {
        return false;
    }

    const api::ActionType* actionType = m_thingManager.actionTypeForThing(*thing, selectedEntry.index);
    if (actionType == nullptr) {
        return false;
    }

    m_showThingDetailInspector = false;
    m_showActionDialog = true;
    m_focusArea = FocusArea::ActionDialog;
    m_actionDialogActionIndex = selectedEntry.index;
    m_actionDialogThingId = thing->id;
    m_actionDialogSelectedParamIndex = 0;
    m_actionDialogRangeEditIndex.reset();
    m_actionDialogActionName = firstNonEmpty({actionType->displayName.toStdString(), actionType->name.toStdString(), "Action"});
    m_actionDialogParamTypes.assign(actionType->paramTypes.begin(), actionType->paramTypes.end());
    m_actionDialogParamValues.clear();
    m_actionDialogParamValues.reserve(m_actionDialogParamTypes.size());
    for (const api::ParamType& paramType : m_actionDialogParamTypes) {
        m_actionDialogParamValues.push_back(normalizedActionDialogValue(paramType, {}));
    }
    m_actionExecutionPending = false;
    m_pendingActionRequestId = -1;
    m_pendingActionInvocation.clear();
    m_actionDialogStatus = "Edit action params and press Enter to execute.";
    m_lastActionExecutionStatus.clear();
    m_lastActionExecutionStatusWarning = false;
    return true;
}

void Engine::closeActionDialog()
{
    m_showActionDialog = false;
    m_actionDialogStatus.clear();
    m_actionDialogActionName.clear();
    m_actionDialogActionIndex = -1;
    m_actionDialogThingId = QUuid();
    m_actionDialogSelectedParamIndex = 0;
    m_actionDialogRangeEditIndex.reset();
    m_actionDialogParamTypes.clear();
    m_actionDialogParamValues.clear();
    m_actionExecutionPending = false;
    m_pendingActionRequestId = -1;
    m_pendingActionInvocation.clear();
    m_lastActionExecutionStatus.clear();
    m_lastActionExecutionStatusWarning = false;
    if (m_focusArea == FocusArea::ActionDialog) {
        m_focusArea = FocusArea::ThingDetails;
    }
}

std::vector<const api::Thing*> Engine::filteredThings() const
{
    const QString search = QString::fromStdString(m_thingSearch).trimmed();
    std::vector<const api::Thing*> things;
    things.reserve(m_thingManager.things().size());

    for (const api::Thing& thing : m_thingManager.things()) {
        const api::ThingClass* thingClass = m_thingManager.thingClassForThing(thing);
        const ThingCategory category = thingCategory(thing);
        if (m_thingCategoryFilter != ThingCategory::All && category != m_thingCategoryFilter) {
            continue;
        }

        if (!search.isEmpty()) {
            const bool matchesThing = thing.name.has_value() && caseInsensitiveContains(*thing.name, search);
            const bool matchesClass = thingClass != nullptr
                                      && (caseInsensitiveContains(thingClass->displayName, search) || caseInsensitiveContains(thingClass->name, search)
                                          || caseInsensitiveContains(thingClass->interfaces.join(QStringLiteral(" ")), search));
            if (!matchesThing && !matchesClass) {
                continue;
            }
        }

        things.push_back(&thing);
    }

    if (m_thingSortMode == ThingSortMode::Alphabetical) {
        std::stable_sort(things.begin(), things.end(), [](const api::Thing* left, const api::Thing* right) {
            return QString::compare(QString::fromStdString(thingLabel(left)), QString::fromStdString(thingLabel(right)), Qt::CaseInsensitive) < 0;
        });
    } else if (m_thingSortMode == ThingSortMode::Grouped) {
        std::stable_sort(things.begin(), things.end(), [this](const api::Thing* left, const api::Thing* right) {
            const ThingCategory leftCategory = thingCategory(*left);
            const ThingCategory rightCategory = thingCategory(*right);
            if (leftCategory != rightCategory) {
                return static_cast<int>(leftCategory) < static_cast<int>(rightCategory);
            }
            return QString::compare(QString::fromStdString(thingLabel(left)), QString::fromStdString(thingLabel(right)), Qt::CaseInsensitive) < 0;
        });
    }

    return things;
}

const api::Thing* Engine::selectedThing() const
{
    const std::vector<const api::Thing*> things = filteredThings();
    if (m_selectedThingIndex < 0 || m_selectedThingIndex >= static_cast<int>(things.size())) {
        return nullptr;
    }
    return things.at(m_selectedThingIndex);
}

QUuid Engine::selectedThingId() const
{
    const api::Thing* thing = selectedThing();
    return thing != nullptr ? thing->id : QUuid();
}

Engine::ThingCategory Engine::thingCategory(const api::Thing& thing) const
{
    const api::ThingClass* thingClass = m_thingManager.thingClassForThing(thing);
    if (thingClass == nullptr) {
        return ThingCategory::Other;
    }

    const QString interfaces = thingClass->interfaces.join(QStringLiteral(" "));
    if (caseInsensitiveContainsAny(interfaces, {"light", "lamp", "dimmer"})) {
        return ThingCategory::Lights;
    }
    if (caseInsensitiveContainsAny(interfaces, {"blind", "shading", "cover", "shutter"})) {
        return ThingCategory::Blinds;
    }
    if (caseInsensitiveContainsAny(interfaces, {"media", "multimedia", "audio", "video", "player"})) {
        return ThingCategory::Multimedia;
    }
    if (caseInsensitiveContainsAny(interfaces, {"powersocket", "power socket", "socket", "outlet", "plug"})) {
        return ThingCategory::PowerSockets;
    }
    if (caseInsensitiveContainsAny(interfaces, {"thermostat", "heating", "climate"})) {
        return ThingCategory::Thermostats;
    }
    if (caseInsensitiveContainsAny(interfaces, {"sensor"})) {
        return ThingCategory::Sensors;
    }
    if (caseInsensitiveContainsAny(interfaces, {"smartmeter", "smart meter", "energymeter", "energy meter", "meter"})) {
        return ThingCategory::SmartMeters;
    }
    if (caseInsensitiveContainsAny(interfaces, {"switch", "button"})) {
        return ThingCategory::Switches;
    }
    return ThingCategory::Other;
}

std::string Engine::thingCategoryLabel(ThingCategory category) const
{
    switch (category) {
    case ThingCategory::All:
        return "All";
    case ThingCategory::Lights:
        return "Lights";
    case ThingCategory::Blinds:
        return "Blinds";
    case ThingCategory::Multimedia:
        return "Multimedia";
    case ThingCategory::PowerSockets:
        return "Power sockets";
    case ThingCategory::Thermostats:
        return "Thermostats";
    case ThingCategory::Sensors:
        return "Sensors";
    case ThingCategory::SmartMeters:
        return "Smart meters";
    case ThingCategory::Switches:
        return "Switches";
    case ThingCategory::Other:
        return "Other";
    }
    return "Other";
}

std::string Engine::thingSortModeLabel() const
{
    switch (m_thingSortMode) {
    case ThingSortMode::Default:
        return "Default";
    case ThingSortMode::Alphabetical:
        return "A-Z";
    case ThingSortMode::Grouped:
        return "Grouped";
    }
    return "Default";
}

void Engine::cycleThingSortMode()
{
    const QUuid selectedId = selectedThingId();
    switch (m_thingSortMode) {
    case ThingSortMode::Default:
        m_thingSortMode = ThingSortMode::Alphabetical;
        break;
    case ThingSortMode::Alphabetical:
        m_thingSortMode = ThingSortMode::Grouped;
        break;
    case ThingSortMode::Grouped:
        m_thingSortMode = ThingSortMode::Default;
        break;
    }
    clampThingSelection(selectedId);
    resetThingDetailSelection();
}

void Engine::cycleThingCategoryFilter()
{
    const QUuid selectedId = selectedThingId();
    if (m_thingCategoryFilter == ThingCategory::Other) {
        m_thingCategoryFilter = ThingCategory::All;
    } else {
        m_thingCategoryFilter = static_cast<ThingCategory>(static_cast<int>(m_thingCategoryFilter) + 1);
    }
    clampThingSelection(selectedId);
    resetThingDetailSelection();
}

std::vector<api::ThingClass> Engine::filteredConfigThingClasses() const
{
    const QString search = QString::fromStdString(m_configureThingSearch).trimmed();
    std::vector<api::ThingClass> filtered;
    for (const api::ThingClass& thingClass : m_thingManager.thingClasses()) {
        if (!isThingClassAddable(thingClass)) {
            continue;
        }
        if (!search.isEmpty() && !caseInsensitiveContains(thingClass.displayName, search) && !caseInsensitiveContains(thingClass.name, search)) {
            continue;
        }
        filtered.push_back(thingClass);
    }
    return filtered;
}

const api::ThingClass* Engine::selectedConfigThingClass() const
{
    const std::vector<api::ThingClass> thingClasses = filteredConfigThingClasses();
    if (m_selectedConfigureThingClassIndex < 0 || m_selectedConfigureThingClassIndex >= static_cast<int>(thingClasses.size())) {
        return nullptr;
    }

    return m_thingManager.thingClassById(thingClasses.at(m_selectedConfigureThingClassIndex).id);
}

const api::Thing* Engine::selectedConfigureThing() const
{
    return m_thingManager.thingAt(m_selectedConfigureThingIndex);
}

#endif
} // namespace nymea

#include "enginebrowse.cpp"

namespace nymea {

void Engine::closeConfigureDialog()
{
    m_showConfigureDialog = false;
    m_configureDialogMode = ConfigureDialogMode::None;
    m_configureRequestPending = false;
    m_configureFlowComplete = false;
    m_configurePendingRequestId = -1;
    m_pendingConfigureInvocation.clear();
    m_lastConfigureExecutionStatus.clear();
    m_lastConfigureExecutionStatusWarning = false;
    m_configureDialogTitle.clear();
    m_configureDialogStatus.clear();
    m_configureThingClassId = QUuid();
    m_configureTargetThingId = QUuid();
    m_configureCreateMethodOptions.clear();
    m_configureCreateMethodIndex = 0;
    m_configureCreateMethod.reset();
    m_configureThingName.clear();
    m_configureParamTypes.clear();
    m_configureParamValues.clear();
    m_configureParamSelectionIndex = 0;
    m_configureRangeEditIndex.reset();
    m_configureThingDescriptors.clear();
    m_configureThingDescriptorIndex = 0;
    m_configureSetupMethod.reset();
    m_configurePairingTransactionId = QUuid();
    m_configurePairingDisplayMessage.clear();
    m_configurePairingOauthUrl.clear();
    m_configurePairingPin.clear();
    m_configurePairingUsername.clear();
    m_configurePairingSecret.clear();
    if (m_focusArea == FocusArea::ConfigureDialog) {
        if (m_mainView == MainView::ConfigureThings) {
            m_focusArea = m_configureThingsView == ConfigureThingsView::AddThing ? FocusArea::ConfigureThingClassList : FocusArea::ConfigureThingSelection;
        } else {
            m_focusArea = FocusArea::MainMenu;
        }
    }
}

void Engine::openHelpView()
{
    if (m_mainView == MainView::Help) {
        return;
    }

    m_previousMainView = m_mainView;
    m_previousFocusArea = m_focusArea;
    m_mainView = MainView::Help;
    m_helpLineIndex = 0;
    m_focusArea = FocusArea::MainMenu;
}

void Engine::closeHelpView()
{
    if (m_mainView != MainView::Help) {
        return;
    }

    m_mainView = m_previousMainView;
    m_focusArea = m_previousFocusArea;
    switch (m_mainView) {
    case MainView::Things:
        clampThingSelection();
        clampThingDetailSelection();
        break;
    case MainView::ConfigureThings:
        clampConfigureThingClassSelection();
        clampConfigureThingSelection();
        break;
    case MainView::ApiBrowser:
        clampApiBrowserSelection();
        clampApiBrowserReferenceSelection();
        clampApiBrowserJsonSelection();
        break;
    case MainView::Settings:
        clampSettingsDetailsSelection();
        break;
    case MainView::Logout:
        break;
    case MainView::About:
        break;
    case MainView::Help:
        break;
    }
}

void Engine::openAddThingDialog()
{
    const api::ThingClass* thingClass = selectedConfigThingClass();
    if (thingClass == nullptr) {
        return;
    }

    closeConfigureDialog();
    m_showConfigureDialog = true;
    m_focusArea = FocusArea::ConfigureDialog;
    m_configureThingClassId = thingClass->id;
    m_configureDialogTitle = "Add thing";
    m_configureThingName = thingClassLabel(*thingClass);
    m_configureCreateMethodOptions.clear();
    for (const api::CreateMethod createMethod : thingClass->createMethods) {
        if (createMethod == api::CreateMethod::CreateMethodUser || createMethod == api::CreateMethod::CreateMethodDiscovery) {
            m_configureCreateMethodOptions.push_back(createMethod);
        }
    }

    if (m_configureCreateMethodOptions.empty()) {
        m_configureDialogMode = ConfigureDialogMode::ReconfigureThingInfo;
        m_configureDialogStatus = "This thing class cannot be created manually from the CLI.";
        return;
    }

    if (m_configureCreateMethodOptions.size() == 1) {
        startAddThingFlow(m_configureCreateMethodOptions.front());
        return;
    }

    m_configureDialogMode = ConfigureDialogMode::AddChooseCreateMethod;
    m_configureDialogStatus = "Select how the thing should be created and press Enter.";
}

void Engine::openRemoveThingDialog()
{
    const api::Thing* thing = selectedConfigureThing();
    if (thing == nullptr) {
        return;
    }

    closeConfigureDialog();
    m_showConfigureDialog = true;
    m_focusArea = FocusArea::ConfigureDialog;
    m_configureDialogMode = ConfigureDialogMode::RemoveThingConfirm;
    m_configureDialogTitle = "Remove thing";
    m_configureTargetThingId = thing->id;
    m_configureDialogStatus = "Press Enter to remove " + thingLabel(thing) + ".";
}

void Engine::openRenameThingDialog()
{
    const api::Thing* thing = selectedConfigureThing();
    if (thing == nullptr) {
        return;
    }

    closeConfigureDialog();
    m_showConfigureDialog = true;
    m_focusArea = FocusArea::ConfigureDialog;
    m_configureDialogMode = ConfigureDialogMode::RenameThing;
    m_configureDialogTitle = "Rename thing";
    m_configureTargetThingId = thing->id;
    m_configureThingName = optionalQStringToStd(thing->name);
    m_configureDialogStatus = "Edit the thing name and press Enter.";
}

void Engine::openReconfigureThingDialog()
{
    const api::Thing* thing = selectedConfigureThing();
    if (thing == nullptr) {
        return;
    }

    closeConfigureDialog();
    m_showConfigureDialog = true;
    m_focusArea = FocusArea::ConfigureDialog;
    m_configureDialogMode = ConfigureDialogMode::ReconfigureThingInfo;
    m_configureTargetThingId = thing->id;
    m_configureDialogTitle = "Reconfigure thing";
    m_configureDialogStatus = "Reconfigure flow is not implemented yet in this CLI.";
}

void Engine::startAddThingFlow(api::CreateMethod createMethod)
{
    const api::ThingClass* thingClass = m_thingManager.thingClassById(m_configureThingClassId);
    if (thingClass == nullptr) {
        closeConfigureDialog();
        return;
    }

    m_showConfigureDialog = true;
    m_focusArea = FocusArea::ConfigureDialog;
    m_configureCreateMethod = createMethod;
    m_configureThingDescriptors.clear();
    m_configureThingDescriptorIndex = 0;
    m_configureSetupMethod.reset();
    m_configureFlowComplete = false;
    m_configurePairingTransactionId = QUuid();
    m_configurePairingDisplayMessage.clear();
    m_configurePairingOauthUrl.clear();
    m_configurePairingPin.clear();
    m_configurePairingUsername.clear();
    m_configurePairingSecret.clear();
    m_pendingConfigureInvocation.clear();
    m_lastConfigureExecutionStatus.clear();
    m_lastConfigureExecutionStatusWarning = false;

    if (createMethod == api::CreateMethod::CreateMethodDiscovery) {
        m_configureDialogMode = ConfigureDialogMode::AddDiscoveryParams;
        m_configureParamTypes.assign(thingClass->discoveryParamTypes.begin(), thingClass->discoveryParamTypes.end());
        m_configureParamValues.clear();
        m_configureParamValues.reserve(m_configureParamTypes.size());
        for (const api::ParamType& paramType : m_configureParamTypes) {
            m_configureParamValues.push_back(normalizedActionDialogValue(paramType, {}));
        }
        m_configureParamSelectionIndex = 0;
        m_configureRangeEditIndex.reset();
        m_configureDialogStatus = m_configureParamTypes.empty() ? "Press Enter to start discovery." : "Edit discovery params and press Enter to search.";
        return;
    }

    m_configureDialogMode = ConfigureDialogMode::AddManualParams;
    m_configureParamTypes.assign(thingClass->paramTypes.begin(), thingClass->paramTypes.end());
    m_configureParamValues.clear();
    m_configureParamValues.reserve(m_configureParamTypes.size());
    for (const api::ParamType& paramType : m_configureParamTypes) {
        m_configureParamValues.push_back(normalizedActionDialogValue(paramType, {}));
    }
    m_configureParamSelectionIndex = 0;
    m_configureRangeEditIndex.reset();
    if (thingClass->setupMethod == api::SetupMethod::SetupMethodJustAdd) {
        m_configureDialogStatus = "Edit the name and params, then press Enter to add the thing.";
    } else {
        m_configureDialogStatus = "Edit the optional name and params, then press Enter to start pairing.";
    }
}

bool Engine::submitConfigureDialog()
{
    if (!m_showConfigureDialog || m_configureRequestPending) {
        return false;
    }

    const api::ThingClass* thingClass = m_configureThingClassId.isNull() ? nullptr : m_thingManager.thingClassById(m_configureThingClassId);
    auto sendReply = [&](JsonRpcReply* reply, const std::string& invocation, const std::string& waitingStatus, auto&& handler) -> bool {
        if (reply == nullptr) {
            m_configureDialogStatus = "Failed to send request: " + m_client.lastError().toStdString();
            return false;
        }
        m_configureFlowComplete = false;
        m_configureRequestPending = true;
        m_configurePendingRequestId = reply->requestId();
        m_configurePendingStartedAt = std::chrono::steady_clock::now();
        m_pendingConfigureInvocation = invocation;
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(m_configurePendingRequestId,
                                                                     m_pendingConfigureInvocation,
                                                                     busyIndicator(m_configurePendingStartedAt) + " Processing");
        m_lastConfigureExecutionStatusWarning = false;
        m_configureDialogStatus = waitingStatus;
        observeReply(reply, std::forward<decltype(handler)>(handler));
        return true;
    };

    switch (m_configureDialogMode) {
    case ConfigureDialogMode::AddChooseCreateMethod:
        if (m_configureCreateMethodIndex < 0 || m_configureCreateMethodIndex >= static_cast<int>(m_configureCreateMethodOptions.size())) {
            return false;
        }
        startAddThingFlow(m_configureCreateMethodOptions.at(m_configureCreateMethodIndex));
        return true;
    case ConfigureDialogMode::AddManualParams: {
        if (thingClass == nullptr) {
            m_configureDialogStatus = "The selected thing class is no longer available.";
            return false;
        }

        api::ParamList params;
        std::string errorMessage;
        if (!buildParamList(m_configureParamTypes, m_configureParamValues, params, errorMessage)) {
            m_configureDialogStatus = errorMessage;
            return false;
        }

        const QString name = QString::fromStdString(m_configureThingName).trimmed();
        if (thingClass->setupMethod == api::SetupMethod::SetupMethodJustAdd) {
            if (name.isEmpty()) {
                m_configureDialogStatus = "A name is required.";
                return false;
            }

            api::IntegrationsAddThingParams request;
            request.name = name;
            request.thingClassId = thingClass->id;
            if (!params.empty()) {
                request.thingParams = params;
            }
            return sendReply(m_client.sendRequest(api::IntegrationsAddThingMethod::methodName(), request.toJson()),
                             "AddThing(name=" + name.toStdString() + ", thingClass=" + thingClassLabel(*thingClass) + ")",
                             "Adding thing...",
                             [this](const QJsonObject& message, const QString& transportError) { handleAddThingReply(message, transportError); });
        }

        api::IntegrationsPairThingParams request;
        if (!name.isEmpty()) {
            request.name = name;
        }
        request.thingClassId = thingClass->id;
        if (!params.empty()) {
            request.thingParams = params;
        }
        return sendReply(m_client.sendRequest(api::IntegrationsPairThingMethod::methodName(), request.toJson()),
                         "PairThing(thingClass=" + thingClassLabel(*thingClass) + (name.isEmpty() ? std::string() : ", name=" + name.toStdString()) + ")",
                         "Starting pairing...",
                         [this](const QJsonObject& message, const QString& transportError) { handlePairThingReply(message, transportError); });
    }
    case ConfigureDialogMode::AddDiscoveryParams: {
        if (thingClass == nullptr) {
            m_configureDialogStatus = "The selected thing class is no longer available.";
            return false;
        }

        api::ParamList discoveryParams;
        std::string errorMessage;
        if (!buildParamList(m_configureParamTypes, m_configureParamValues, discoveryParams, errorMessage)) {
            m_configureDialogStatus = errorMessage;
            return false;
        }

        api::IntegrationsDiscoverThingsParams request;
        request.thingClassId = thingClass->id;
        if (!discoveryParams.empty()) {
            request.discoveryParams = discoveryParams;
        }

        return sendReply(m_client.sendRequest(api::IntegrationsDiscoverThingsMethod::methodName(), request.toJson()),
                         "DiscoverThings(thingClass=" + thingClassLabel(*thingClass) + ")",
                         "Discovering things...",
                         [this](const QJsonObject& message, const QString& transportError) { handleDiscoverThingsReply(message, transportError); });
    }
    case ConfigureDialogMode::AddDiscoveryResults: {
        if (thingClass == nullptr) {
            m_configureDialogStatus = "The selected thing class is no longer available.";
            return false;
        }
        if (m_configureThingDescriptorIndex < 0 || m_configureThingDescriptorIndex >= static_cast<int>(m_configureThingDescriptors.size())) {
            return false;
        }

        const api::ThingDescriptor& descriptor = m_configureThingDescriptors.at(m_configureThingDescriptorIndex);
        if (thingClass->setupMethod == api::SetupMethod::SetupMethodJustAdd) {
            api::IntegrationsAddThingParams request;
            request.name = !descriptor.title.isEmpty() ? descriptor.title : QString::fromStdString(thingClassLabel(*thingClass));
            request.thingDescriptorId = descriptor.id;
            return sendReply(m_client.sendRequest(api::IntegrationsAddThingMethod::methodName(), request.toJson()),
                             "AddThing(descriptor=" + descriptorLabel(descriptor) + ")",
                             "Adding discovered thing...",
                             [this](const QJsonObject& message, const QString& transportError) { handleAddThingReply(message, transportError); });
        }

        api::IntegrationsPairThingParams request;
        if (!descriptor.title.isEmpty()) {
            request.name = descriptor.title;
        }
        request.thingDescriptorId = descriptor.id;
        return sendReply(m_client.sendRequest(api::IntegrationsPairThingMethod::methodName(), request.toJson()),
                         "PairThing(descriptor=" + descriptorLabel(descriptor) + ")",
                         "Starting pairing...",
                         [this](const QJsonObject& message, const QString& transportError) { handlePairThingReply(message, transportError); });
    }
    case ConfigureDialogMode::AddPairingConfirmation: {
        if (m_configurePairingTransactionId.isNull()) {
            m_configureDialogStatus = "Missing pairing transaction id.";
            return false;
        }

        api::IntegrationsConfirmPairingParams request;
        request.pairingTransactionId = m_configurePairingTransactionId;
        if (m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword) {
            const QString username = QString::fromStdString(m_configurePairingUsername).trimmed();
            const QString secret = QString::fromStdString(m_configurePairingSecret);
            if (username.isEmpty() || secret.isEmpty()) {
                m_configureDialogStatus = "Username and password are required.";
                return false;
            }
            request.username = username;
            request.secret = secret;
        } else if (m_configureSetupMethod == api::SetupMethod::SetupMethodEnterPin || m_configureSetupMethod == api::SetupMethod::SetupMethodDisplayPin
                   || m_configureSetupMethod == api::SetupMethod::SetupMethodOAuth) {
            const QString secret = QString::fromStdString(m_configurePairingSecret).trimmed();
            if (secret.isEmpty()) {
                m_configureDialogStatus = "A confirmation value is required.";
                return false;
            }
            request.secret = secret;
        }

        return sendReply(m_client.sendRequest(api::IntegrationsConfirmPairingMethod::methodName(), request.toJson()),
                         "ConfirmPairing(setup=" + setupMethodLabel(m_configureSetupMethod.value_or(api::SetupMethod::SetupMethodPushButton)) + ")",
                         "Confirming pairing...",
                         [this](const QJsonObject& message, const QString& transportError) { handleConfirmPairingReply(message, transportError); });
    }
    case ConfigureDialogMode::RemoveThingConfirm: {
        if (m_configureTargetThingId.isNull()) {
            return false;
        }

        api::IntegrationsRemoveThingParams request;
        request.thingId = m_configureTargetThingId;
        return sendReply(m_client.sendRequest(api::IntegrationsRemoveThingMethod::methodName(), request.toJson()),
                         "RemoveThing(thing=" + thingLabel(m_thingManager.thingById(m_configureTargetThingId)) + ")",
                         "Removing thing...",
                         [this](const QJsonObject& message, const QString& transportError) { handleRemoveThingReply(message, transportError); });
    }
    case ConfigureDialogMode::RenameThing: {
        if (m_configureTargetThingId.isNull()) {
            return false;
        }

        const QString name = QString::fromStdString(m_configureThingName).trimmed();
        if (name.isEmpty()) {
            m_configureDialogStatus = "A name is required.";
            return false;
        }

        api::IntegrationsEditThingParams request;
        request.thingId = m_configureTargetThingId;
        request.name = name;
        return sendReply(m_client.sendRequest(api::IntegrationsEditThingMethod::methodName(), request.toJson()),
                         "RenameThing(name=" + name.toStdString() + ")",
                         "Renaming thing...",
                         [this](const QJsonObject& message, const QString& transportError) { handleRenameThingReply(message, transportError); });
    }
    case ConfigureDialogMode::ReconfigureThingInfo:
        closeConfigureDialog();
        return true;
    case ConfigureDialogMode::None:
        break;
    }

    return false;
}

bool Engine::executeCurrentAction()
{
    const api::Thing* thing = m_thingManager.thingById(m_actionDialogThingId);
    if (!m_showActionDialog || thing == nullptr || m_actionDialogActionIndex < 0 || m_actionExecutionPending) {
        if (m_showActionDialog && thing == nullptr) {
            m_actionDialogStatus = "Selected thing is no longer available.";
        }
        return false;
    }

    const api::ActionType* actionType = m_thingManager.actionTypeForThing(*thing, m_actionDialogActionIndex);
    if (actionType == nullptr) {
        m_actionDialogStatus = "Selected action is no longer available.";
        return false;
    }

    api::ParamList params;
    for (int index = 0; index < static_cast<int>(m_actionDialogParamTypes.size()); ++index) {
        const api::ParamType& paramType = m_actionDialogParamTypes.at(index);
        const std::string rawValue = normalizedActionSubmissionValue(paramType, m_actionDialogParamValues.at(index));
        std::optional<QJsonValue> parsedValue = parseActionInputValue(rawValue, paramType.type);
        if (!parsedValue.has_value()) {
            m_actionDialogStatus = "Invalid value for " + firstNonEmpty({paramType.displayName.toStdString(), paramType.name.toStdString(), "param"}) + ".";
            return false;
        }

        api::Param param;
        param.paramTypeId = paramType.id;
        param.value = *parsedValue;
        params.append(param);
    }

    api::IntegrationsExecuteActionParams request;
    request.thingId = thing->id;
    request.actionTypeId = actionType->id;
    if (!params.empty()) {
        request.params = params;
    }

    JsonRpcReply* reply = m_client.sendRequest(QStringLiteral("Integrations.ExecuteAction"), request.toJson());
    if (reply == nullptr) {
        m_actionDialogStatus = "Failed to send action request: " + m_client.lastError().toStdString();
        return false;
    }

    const int requestId = reply->requestId();
    m_actionExecutionPending = true;
    m_pendingActionRequestId = requestId;
    m_pendingActionInvocation = formatActionInvocation(m_actionDialogActionName, m_actionDialogParamTypes, m_actionDialogParamValues);
    m_pendingActionStartedAt = std::chrono::steady_clock::now();
    m_actionDialogStatus = "Action sent. Waiting for response...";
    m_lastActionExecutionStatus = formatActionExecutionStatus(requestId, m_pendingActionInvocation, busyIndicator(m_pendingActionStartedAt) + " Executing");
    m_lastActionExecutionStatusWarning = false;
    observeReply(reply, [this](const QJsonObject& message, const QString& transportError) { handleActionExecutionReply(message, transportError); });
    return true;
}

void Engine::observeReply(JsonRpcReply* reply, std::function<void(const QJsonObject&, const QString&)> handler)
{
    if (reply == nullptr) {
        enqueueUiTask([handler = std::move(handler)]() mutable { handler(QJsonObject{}, QStringLiteral("Failed to create JSON-RPC reply.")); });
        return;
    }

    auto dispatch = [this, reply, handler = std::move(handler)]() mutable {
        const QJsonObject message = reply->message();
        const QString transportError = reply->transportError();
        enqueueUiTask([reply, handler = std::move(handler), message, transportError]() mutable {
            handler(message, transportError);
            QMetaObject::invokeMethod(reply, "deleteLater", Qt::QueuedConnection);
        });
    };

    if (reply->isFinished()) {
        dispatch();
        return;
    }

    QObject::connect(reply, &JsonRpcReply::finished, [dispatch = std::move(dispatch)]() mutable { dispatch(); });
}

void Engine::enterAuthenticationRequiredState(const std::string& statusMessage, bool startPushButtonAuth)
{
    m_isAuthenticationRequired = true;
    m_isAuthenticated = false;
    m_notificationsEnabled = false;
    m_showLoginForm = true;
    m_loginSelectedInputIndex = 0;
    m_focusArea = FocusArea::LoginForm;
    m_authenticationPending = false;
    m_pushButtonAuthPending = false;
    m_pushButtonAuthTransactionId = -1;

    if (m_pushButtonAuthAvailable) {
        m_authStatus = statusMessage;
        if (startPushButtonAuth) {
            requestPushButtonAuth();
        }
    } else {
        m_authStatus = statusMessage;
    }
}

void Engine::completeAuthentication(const QString& token, const std::optional<QString>& username, const std::string& statusMessage)
{
    if (username.has_value() && !username->isEmpty()) {
        m_username = username->toStdString();
    }

    m_client.setAuthToken(token);
    m_isAuthenticated = true;
    m_isAuthenticationRequired = false;
    m_notificationsEnabled = false;
    m_showLoginForm = false;
    m_focusArea = FocusArea::MainMenu;
    m_authStatus = statusMessage;
    m_ignoreStoredToken = false;
    m_logoutStatus.clear();
    m_pushButtonAuthPending = false;
    m_pushButtonAuthTransactionId = -1;
    saveCurrentConnection(true);
    m_thingManager.setStatus("Authentication succeeded.");
    if (m_mainView == MainView::ApiBrowser) {
        ensureApiBrowserLoaded();
    }
    enableNotifications(true);
}

void Engine::requestPushButtonAuth()
{
    if (!m_client.isConnected() || !m_isAuthenticationRequired || m_isAuthenticated || !m_pushButtonAuthAvailable) {
        return;
    }
    if (m_pushButtonAuthPending) {
        return;
    }

    m_pushButtonAuthPending = true;
    m_pushButtonAuthTransactionId = -1;
    m_authStatus.clear();

    QJsonObject params;
    params.insert(QStringLiteral("deviceName"), QStringLiteral("nymea-cli"));
    observeReply(m_client.sendRequest(api::JSONRPCRequestPushButtonAuthMethod::methodName(), params),
                 [this](const QJsonObject& message, const QString& transportError) { handleRequestPushButtonAuthReply(message, transportError); });
}

void Engine::handleRequestPushButtonAuthReply(const QJsonObject& message, const QString& transportError)
{
    if (!m_pushButtonAuthPending) {
        return;
    }

    if (!transportError.isEmpty()) {
        m_pushButtonAuthPending = false;
        m_pushButtonAuthTransactionId = -1;
        enterAuthenticationRequiredState("No reply for push-button authentication request: " + transportError.toStdString(), false);
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("error") || status == QStringLiteral("unauthorized")) {
        m_pushButtonAuthPending = false;
        m_pushButtonAuthTransactionId = -1;
        enterAuthenticationRequiredState("Push-button authentication could not be started.", false);
        return;
    }

    const api::JSONRPCRequestPushButtonAuthResponse response = api::JSONRPCRequestPushButtonAuthResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (!response.success) {
        m_pushButtonAuthPending = false;
        m_pushButtonAuthTransactionId = -1;
        enterAuthenticationRequiredState("Push-button authentication could not be started.", false);
        return;
    }

    m_pushButtonAuthTransactionId = response.transactionId;
    m_authStatus.clear();
}

void Engine::handlePushButtonAuthFinished(const QJsonObject& message)
{
    const api::JSONRPCPushButtonAuthFinishedNotificationParams notification = api::JSONRPCPushButtonAuthFinishedNotificationParams::fromJson(
        message.value(QStringLiteral("params")).toObject());

    if (m_pushButtonAuthPending && m_pushButtonAuthTransactionId >= 0 && notification.transactionId != m_pushButtonAuthTransactionId) {
        return;
    }

    m_pushButtonAuthPending = false;
    m_pushButtonAuthTransactionId = -1;

    if (!notification.success || !notification.token.has_value() || notification.token->isEmpty()) {
        enterAuthenticationRequiredState("Push-button authentication failed. Please press the push button again.", false);
        return;
    }

    completeAuthentication(*notification.token, std::nullopt, "Authenticated using push-button auth.");
}

void Engine::handleActionExecutionReply(const QJsonObject& message, const QString& transportError)
{
    if (!transportError.isEmpty()) {
        m_actionDialogStatus = "Action execution failed: " + transportError.toStdString();
        m_lastActionExecutionStatus = formatActionExecutionStatus(m_pendingActionRequestId, m_pendingActionInvocation, transportError.toStdString());
        m_lastActionExecutionStatusWarning = true;
    } else {
        const QString status = message.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("unauthorized")) {
            clearStoredToken();
            m_client.clearAuthToken();
            m_isAuthenticationRequired = true;
            m_isAuthenticated = false;
            m_notificationsEnabled = false;
            m_showLoginForm = true;
            m_loginSelectedInputIndex = 0;
            m_focusArea = FocusArea::LoginForm;
            m_authStatus = "Authentication required. Please login.";
            m_actionDialogStatus = "Action execution unauthorized.";
            m_lastActionExecutionStatus = formatActionExecutionStatus(m_pendingActionRequestId, m_pendingActionInvocation, "Unauthorized");
            m_lastActionExecutionStatusWarning = true;
        } else if (status == QStringLiteral("error")) {
            const QString errorText = message.value(QStringLiteral("error")).toString();
            const std::string renderedError = errorText.isEmpty() ? "Transport error" : errorText.toStdString();
            m_actionDialogStatus = "Action execution returned JSON-RPC error.";
            m_lastActionExecutionStatus = formatActionExecutionStatus(m_pendingActionRequestId, m_pendingActionInvocation, renderedError);
            m_lastActionExecutionStatusWarning = true;
        } else {
            const api::IntegrationsExecuteActionResponse response = api::IntegrationsExecuteActionResponse::fromJson(message.value(QStringLiteral("params")).toObject());
            if (response.thingError != api::ThingError::ThingErrorNoError) {
                m_actionDialogStatus = "Action failed: " + api::toString(response.thingError).toStdString();
                if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
                    m_actionDialogStatus += " - " + response.displayMessage->toStdString();
                }
                std::string result = thingErrorLabel(response.thingError);
                if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
                    result += " - " + response.displayMessage->toStdString();
                }
                m_lastActionExecutionStatus = formatActionExecutionStatus(m_pendingActionRequestId, m_pendingActionInvocation, result);
                m_lastActionExecutionStatusWarning = true;
            } else {
                m_thingManager.setStatus("Executed action " + m_actionDialogActionName + " (request id " + std::to_string(m_pendingActionRequestId) + ").");
                std::string result = "NoError";
                if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
                    result += " - " + response.displayMessage->toStdString();
                }
                m_lastActionExecutionStatus = formatActionExecutionStatus(m_pendingActionRequestId, m_pendingActionInvocation, result);
                m_lastActionExecutionStatusWarning = false;
                m_actionDialogStatus = "Action executed. Press Enter to execute again or Esc to close.";
            }
        }
    }

    m_actionExecutionPending = false;
    m_pendingActionRequestId = -1;
    m_pendingActionInvocation.clear();
}

void Engine::handleFetchAllThingClassesReply(const QJsonObject& message, const QString& transportError)
{
    m_fetchAllThingClassesPending = false;

    if (!transportError.isEmpty()) {
        m_settingsWarning = "Thing class catalog unavailable: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_settingsWarning = "Thing class catalog request was unauthorized.";
        return;
    }

    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Thing class catalog request returned an error.";
        return;
    }

    std::string errorMessage;
    if (!m_thingManager.updateThingClassesFromReply(message, errorMessage)) {
        m_settingsWarning = errorMessage;
        return;
    }

    m_haveAllThingClasses = true;
    clampConfigureThingClassSelection();
    m_settingsWarning.clear();
}

void Engine::handleDiscoverThingsReply(const QJsonObject& message, const QString& transportError)
{
    const int requestId = m_configurePendingRequestId;
    const std::string invocation = m_pendingConfigureInvocation;
    m_configureRequestPending = false;
    m_configurePendingRequestId = -1;
    m_pendingConfigureInvocation.clear();

    if (!transportError.isEmpty()) {
        m_configureDialogStatus = "Discovery failed: " + transportError.toStdString();
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, transportError.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_configureDialogStatus = "Discovery unauthorized.";
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, "Unauthorized");
        m_lastConfigureExecutionStatusWarning = true;
        return;
    }

    if (status == QStringLiteral("error")) {
        m_configureDialogStatus = "Discovery returned a JSON-RPC error.";
        const QString errorText = message.value(QStringLiteral("error")).toString();
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, errorText.isEmpty() ? "JSON-RPC error" : errorText.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        return;
    }

    const api::IntegrationsDiscoverThingsResponse response = api::IntegrationsDiscoverThingsResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.thingError != api::ThingError::ThingErrorNoError) {
        m_configureDialogStatus = "Discovery failed: " + thingErrorLabel(response.thingError);
        if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
            m_configureDialogStatus += " - " + response.displayMessage->toStdString();
        }
        std::string result = thingErrorLabel(response.thingError);
        if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
            result += " - " + response.displayMessage->toStdString();
        }
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, result);
        m_lastConfigureExecutionStatusWarning = true;
        return;
    }

    m_configureThingDescriptors.clear();
    if (response.thingDescriptors.has_value()) {
        m_configureThingDescriptors.assign(response.thingDescriptors->begin(), response.thingDescriptors->end());
    }
    m_configureThingDescriptorIndex = 0;

    if (m_configureThingDescriptors.empty()) {
        m_configureDialogStatus = response.displayMessage.has_value() && !response.displayMessage->isEmpty() ? response.displayMessage->toStdString()
                                                                                                             : "No discovery results found.";
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, "No results");
        m_lastConfigureExecutionStatusWarning = true;
        return;
    }

    m_configureDialogMode = ConfigureDialogMode::AddDiscoveryResults;
    m_configureDialogStatus = "Select a discovery result and press Enter to continue.";
    m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, "NoError - " + std::to_string(m_configureThingDescriptors.size()) + " result(s)");
    m_lastConfigureExecutionStatusWarning = false;
}

void Engine::handleAddThingReply(const QJsonObject& message, const QString& transportError)
{
    const int requestId = m_configurePendingRequestId;
    const std::string invocation = m_pendingConfigureInvocation;
    m_configureRequestPending = false;
    m_configurePendingRequestId = -1;
    m_pendingConfigureInvocation.clear();
    m_configureFlowComplete = true;

    if (!transportError.isEmpty()) {
        m_configureDialogStatus = "Add thing failed: " + transportError.toStdString();
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, transportError.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Add thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_configureDialogStatus = "Add thing unauthorized.";
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, "Unauthorized");
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Add thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    if (status == QStringLiteral("error")) {
        const QString errorText = message.value(QStringLiteral("error")).toString();
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, errorText.isEmpty() ? "JSON-RPC error" : errorText.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Add thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    const api::IntegrationsAddThingResponse response = api::IntegrationsAddThingResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.thingError != api::ThingError::ThingErrorNoError) {
        std::string result = thingErrorLabel(response.thingError);
        if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
            result += " - " + response.displayMessage->toStdString();
        }
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, result);
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Add thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    std::string result = "NoError";
    if (response.thingId.has_value()) {
        result += " - " + uuidToStd(*response.thingId);
    }
    if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
        result += " - " + response.displayMessage->toStdString();
    }
    m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, result);
    m_lastConfigureExecutionStatusWarning = false;
    m_configureDialogStatus = "Add thing finished. Press Enter or Esc to close.";
    m_thingManager.setStatus("Added thing" + std::string(response.thingId.has_value() ? " " + uuidToStd(*response.thingId) : "."));
    fetchThings();
}

void Engine::handlePairThingReply(const QJsonObject& message, const QString& transportError)
{
    const int requestId = m_configurePendingRequestId;
    const std::string invocation = m_pendingConfigureInvocation;
    m_configureRequestPending = false;
    m_configurePendingRequestId = -1;
    m_pendingConfigureInvocation.clear();

    if (!transportError.isEmpty()) {
        m_configureDialogStatus = "Pairing failed: " + transportError.toStdString();
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, transportError.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_configureDialogStatus = "Pairing unauthorized.";
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, "Unauthorized");
        m_lastConfigureExecutionStatusWarning = true;
        return;
    }

    if (status == QStringLiteral("error")) {
        const QString errorText = message.value(QStringLiteral("error")).toString();
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, errorText.isEmpty() ? "JSON-RPC error" : errorText.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Pairing returned a JSON-RPC error.";
        return;
    }

    const api::IntegrationsPairThingResponse response = api::IntegrationsPairThingResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.thingError != api::ThingError::ThingErrorNoError) {
        std::string result = thingErrorLabel(response.thingError);
        if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
            result += " - " + response.displayMessage->toStdString();
        }
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, result);
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Pairing failed: " + result;
        return;
    }

    if (!response.pairingTransactionId.has_value()) {
        m_configureDialogStatus = "Pairing started but no transaction id was returned.";
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, "Missing pairing transaction id");
        m_lastConfigureExecutionStatusWarning = true;
        return;
    }

    m_configurePairingTransactionId = *response.pairingTransactionId;
    m_configureSetupMethod = response.setupMethod;
    m_configurePairingDisplayMessage = response.displayMessage.has_value() ? response.displayMessage->toStdString() : std::string();
    m_configurePairingOauthUrl = response.oAuthUrl.has_value() ? response.oAuthUrl->toStdString() : std::string();
    m_configurePairingPin = response.pin.has_value() ? response.pin->toStdString() : std::string();
    m_configurePairingUsername.clear();
    m_configurePairingSecret.clear();
    m_configureParamSelectionIndex = 0;
    if (m_configureSetupMethod == api::SetupMethod::SetupMethodEnterPin && !m_configurePairingPin.empty()) {
        m_configurePairingSecret = m_configurePairingPin;
    }
    std::string result = "NoError - " + setupMethodLabel(m_configureSetupMethod.value_or(api::SetupMethod::SetupMethodPushButton));
    if (!m_configurePairingDisplayMessage.empty()) {
        result += " - " + m_configurePairingDisplayMessage;
    }
    m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, result);
    m_lastConfigureExecutionStatusWarning = false;

    m_configureDialogMode = ConfigureDialogMode::AddPairingConfirmation;
    switch (m_configureSetupMethod.value_or(api::SetupMethod::SetupMethodPushButton)) {
    case api::SetupMethod::SetupMethodDisplayPin:
        m_configureDialogStatus = "Enter the PIN shown on the device and press Enter.";
        break;
    case api::SetupMethod::SetupMethodEnterPin:
        m_configureDialogStatus = "Show the PIN on the target device, then press Enter to confirm.";
        break;
    case api::SetupMethod::SetupMethodPushButton:
        m_configureDialogStatus = "Follow the pairing instructions and press Enter when ready.";
        break;
    case api::SetupMethod::SetupMethodUserAndPassword:
        m_configureDialogStatus = "Enter the username and password, then press Enter.";
        break;
    case api::SetupMethod::SetupMethodOAuth:
        m_configureDialogStatus = "Open the OAuth URL, complete login, then paste the redirect URL and press Enter.";
        break;
    case api::SetupMethod::SetupMethodJustAdd:
        m_configureDialogStatus = "Press Enter to confirm.";
        break;
    }
}

void Engine::handleConfirmPairingReply(const QJsonObject& message, const QString& transportError)
{
    const int requestId = m_configurePendingRequestId;
    const std::string invocation = m_pendingConfigureInvocation;
    m_configureRequestPending = false;
    m_configurePendingRequestId = -1;
    m_pendingConfigureInvocation.clear();
    m_configureFlowComplete = true;

    if (!transportError.isEmpty()) {
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, transportError.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Setup finished with an error. Press Enter or Esc to close.";
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, "Unauthorized");
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Setup finished with an error. Press Enter or Esc to close.";
        return;
    }

    if (status == QStringLiteral("error")) {
        const QString errorText = message.value(QStringLiteral("error")).toString();
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, errorText.isEmpty() ? "JSON-RPC error" : errorText.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Setup finished with an error. Press Enter or Esc to close.";
        return;
    }

    const api::IntegrationsConfirmPairingResponse response = api::IntegrationsConfirmPairingResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.thingError != api::ThingError::ThingErrorNoError) {
        std::string result = thingErrorLabel(response.thingError);
        if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
            result += " - " + response.displayMessage->toStdString();
        }
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, result);
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Setup finished with an error. Press Enter or Esc to close.";
        return;
    }

    std::string result = "NoError";
    if (response.thingId.has_value()) {
        result += " - " + uuidToStd(*response.thingId);
    }
    if (response.displayMessage.has_value() && !response.displayMessage->isEmpty()) {
        result += " - " + response.displayMessage->toStdString();
    }
    m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, result);
    m_lastConfigureExecutionStatusWarning = false;
    m_configureDialogStatus = "Setup finished. Press Enter or Esc to close.";
    m_thingManager.setStatus("Pairing completed successfully.");
    fetchThings();
}

void Engine::handleRemoveThingReply(const QJsonObject& message, const QString& transportError)
{
    const int requestId = m_configurePendingRequestId;
    const std::string invocation = m_pendingConfigureInvocation;
    m_configureRequestPending = false;
    m_configurePendingRequestId = -1;
    m_pendingConfigureInvocation.clear();
    m_configureFlowComplete = true;

    if (!transportError.isEmpty()) {
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, transportError.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Remove thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("error") || status == QStringLiteral("unauthorized")) {
        const QString errorText = message.value(QStringLiteral("error")).toString();
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId,
                                                                     invocation,
                                                                     status == QStringLiteral("unauthorized") ? "Unauthorized"
                                                                                                              : (errorText.isEmpty() ? "JSON-RPC error" : errorText.toStdString()));
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Remove thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    const api::IntegrationsRemoveThingResponse response = api::IntegrationsRemoveThingResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.thingError != api::ThingError::ThingErrorNoError) {
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, thingErrorLabel(response.thingError));
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Remove thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, "NoError");
    m_lastConfigureExecutionStatusWarning = false;
    m_configureDialogStatus = "Remove thing finished. Press Enter or Esc to close.";
    m_thingManager.setStatus("Removed thing " + uuidToStd(m_configureTargetThingId) + ".");
    fetchThings();
}

void Engine::handleRenameThingReply(const QJsonObject& message, const QString& transportError)
{
    const int requestId = m_configurePendingRequestId;
    const std::string invocation = m_pendingConfigureInvocation;
    m_configureRequestPending = false;
    m_configurePendingRequestId = -1;
    m_pendingConfigureInvocation.clear();
    m_configureFlowComplete = true;

    if (!transportError.isEmpty()) {
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, transportError.toStdString());
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Rename thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("error") || status == QStringLiteral("unauthorized")) {
        const QString errorText = message.value(QStringLiteral("error")).toString();
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId,
                                                                     invocation,
                                                                     status == QStringLiteral("unauthorized") ? "Unauthorized"
                                                                                                              : (errorText.isEmpty() ? "JSON-RPC error" : errorText.toStdString()));
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Rename thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    const api::IntegrationsEditThingResponse response = api::IntegrationsEditThingResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.thingError != api::ThingError::ThingErrorNoError) {
        m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, thingErrorLabel(response.thingError));
        m_lastConfigureExecutionStatusWarning = true;
        m_configureDialogStatus = "Rename thing finished with an error. Press Enter or Esc to close.";
        return;
    }

    m_lastConfigureExecutionStatus = formatActionExecutionStatus(requestId, invocation, "NoError");
    m_lastConfigureExecutionStatusWarning = false;
    m_configureDialogStatus = "Rename thing finished. Press Enter or Esc to close.";
    m_thingManager.setStatus("Renamed thing " + uuidToStd(m_configureTargetThingId) + ".");
    fetchThings();
}

void Engine::handleNotification(const QJsonObject& message)
{
    const QString notificationName = message.value(QStringLiteral("notification")).toString();
    const QJsonObject params = message.value(QStringLiteral("params")).toObject();
    const QUuid selectedId = selectedThingId();

    if (notificationName == api::JSONRPCPushButtonAuthFinishedNotification::notificationName()
        || notificationName == api::UsersPushButtonAuthFinishedNotification::notificationName()) {
        handlePushButtonAuthFinished(message);
        return;
    }

    if (notificationName == api::IntegrationsStateChangedNotification::notificationName()) {
        const api::IntegrationsStateChangedNotificationParams notification = api::IntegrationsStateChangedNotificationParams::fromJson(params);
        if (m_thingManager
                .updateThingState(notification.thingId, notification.stateTypeId, notification.value, notification.minValue, notification.maxValue, notification.possibleValues)) {
            clampThingSelection(selectedId);
            clampThingDetailSelection();
            m_thingManager.setStatus("Live update: state changed on " + thingLabel(m_thingManager.thingById(notification.thingId)) + ".");
        }
        return;
    }

    if (notificationName == api::IntegrationsThingAddedNotification::notificationName()) {
        const api::IntegrationsThingAddedNotificationParams notification = api::IntegrationsThingAddedNotificationParams::fromJson(params);
        m_thingManager.upsertThing(notification.thing);
        clampThingSelection(selectedId);
        clampThingDetailSelection();
        clampConfigureThingSelection();

        std::string status = "Live update: added " + thingLabel(m_thingManager.thingById(notification.thing.id)) + ".";
        if (!m_thingManager.hasThingClass(notification.thing.thingClassId)) {
            status += " Thing class metadata is not loaded yet.";
            fetchThingClasses();
        }
        m_thingManager.setStatus(status);
        return;
    }

    if (notificationName == api::IntegrationsThingChangedNotification::notificationName()) {
        const api::IntegrationsThingChangedNotificationParams notification = api::IntegrationsThingChangedNotificationParams::fromJson(params);
        m_thingManager.upsertThing(notification.thing);
        clampThingSelection(selectedId);
        clampThingDetailSelection();
        clampConfigureThingSelection();

        std::string status = "Live update: updated " + thingLabel(m_thingManager.thingById(notification.thing.id)) + ".";
        if (!m_thingManager.hasThingClass(notification.thing.thingClassId)) {
            status += " Thing class metadata is not loaded yet.";
            fetchThingClasses();
        }
        m_thingManager.setStatus(status);
        return;
    }

    if (notificationName == api::IntegrationsThingRemovedNotification::notificationName()) {
        const api::IntegrationsThingRemovedNotificationParams notification = api::IntegrationsThingRemovedNotificationParams::fromJson(params);
        if (m_thingManager.removeThing(notification.thingId)) {
            if (m_showActionDialog && m_actionDialogThingId == notification.thingId) {
                m_actionDialogStatus = "Thing was removed. Press Esc to close this dialog.";
            }
            clampThingSelection(selectedId);
            clampThingDetailSelection();
            clampConfigureThingSelection();
            m_thingManager.setStatus("Live update: removed thing " + uuidToStd(notification.thingId) + ".");
        }
        return;
    }

    if (notificationName == api::IntegrationsThingSettingChangedNotification::notificationName()) {
        const api::IntegrationsThingSettingChangedNotificationParams notification = api::IntegrationsThingSettingChangedNotificationParams::fromJson(params);
        if (m_thingManager.updateThingSetting(notification.thingId, notification.paramTypeId, notification.value)) {
            m_thingManager.setStatus("Live update: setting changed on " + thingLabel(m_thingManager.thingById(notification.thingId)) + ".");
        }
        return;
    }

    if (notificationName.startsWith(QStringLiteral("Integrations."))) {
        m_thingManager.setStatus("Live update: " + notificationName.toStdString() + ".");
    }
}

void Engine::enqueueUiTask(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_uiTaskMutex);
        m_uiTasks.push_back(std::move(task));
    }

    if (m_screen != nullptr) {
        m_screen->PostEvent(ftxui::Event::Custom);
    }
}

void Engine::drainUiTasks()
{
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock(m_uiTaskMutex);
        tasks.swap(m_uiTasks);
    }

    for (auto& task : tasks) {
        task();
    }
}

void Engine::handleClientStateChanged(bool connected, bool encrypted, const QString& peerCertificateFingerprint, const QString& authToken, const QString& lastError)
{
    Q_UNUSED(encrypted)
    Q_UNUSED(peerCertificateFingerprint)
    Q_UNUSED(authToken)
    Q_UNUSED(lastError)

    if (!connected) {
        m_connectionLost = true;
        m_connectionStatus = "Connection lost, try to reconnect.";
        m_authStatus = m_connectionStatus;
        m_thingManager.setStatus(m_connectionStatus);
        m_showLogoutConfirm = false;
        m_logoutRequestPending = false;
        m_logoutStatus.clear();
        closePowerActionConfirmDialog();
        closeActionDialog();
        closeConfigureDialog();
        m_showThingDetailInspector = false;
        m_helloPending = false;
        m_authenticationPending = false;
        m_pushButtonAuthAvailable = false;
        m_pushButtonAuthPending = false;
        m_pushButtonAuthTransactionId = -1;
        m_notificationSetupPending = false;
        m_fetchThingsPending = false;
        m_fetchThingClassesPending = false;
        m_actionExecutionPending = false;
        m_pendingActionRequestId = -1;
        m_pendingActionInvocation.clear();
    } else {
        m_connectionStatus = std::string(m_client.isEncrypted() ? "SSL connected to " : "TCP connected to ") + endpoint();
        m_thingManager.setStatus(m_connectionStatus);
        updateCertificateWarning();
    }
}

bool Engine::connectToServer(bool shouldLoadSavedConnection)
{
    m_client.clearAuthToken();
    if (shouldLoadSavedConnection) {
        loadSavedConnection();
    }
    m_helloPending = false;
    m_authenticationPending = false;
    m_notificationSetupPending = false;
    m_fetchThingsPending = false;
    m_fetchThingClassesPending = false;
    m_fetchAllThingClassesPending = false;
    m_actionExecutionPending = false;
    m_pendingActionRequestId = -1;
    m_pendingActionInvocation.clear();
    m_notificationsEnabled = false;
    m_haveAllThingClasses = false;
    m_pushButtonAuthAvailable = false;
    m_pushButtonAuthPending = false;
    m_pushButtonAuthTransactionId = -1;
    m_securityWarning.clear();
    m_settingsWarning.clear();
    m_showLogoutConfirm = false;
    m_logoutRequestPending = false;
    m_logoutStatus.clear();
    m_showSystemActionConfirm = false;
    m_systemActionRequestPending = false;
    m_systemActionStatus.clear();
    m_systemCapabilitiesLoaded = false;
    m_systemCapabilitiesPending = false;
    m_systemCapabilities = api::SystemGetCapabilitiesResponse{};
    m_systemTimeLoaded = false;
    m_systemTimePending = false;
    m_systemTime = api::SystemGetTimeResponse{};
    m_systemUpdateStatusLoaded = false;
    m_systemUpdateStatusPending = false;
    m_systemUpdateStatus = api::SystemGetUpdateStatusResponse{};
    m_systemPackagesLoaded = false;
    m_systemPackagesPending = false;
    m_systemPackages.clear();
    m_systemTimeZonesLoaded = false;
    m_systemTimeZonesPending = false;
    m_systemTimeZones.clear();
    m_systemTimeZoneSearch.clear();
    m_apiBrowserLoaded = false;
    m_apiBrowserPending = false;
    m_apiBrowserIntrospection = QJsonObject();
    m_apiBrowserHistory.clear();
    m_apiBrowserSelectedSection.clear();
    m_apiBrowserSelectedName.clear();
    m_apiBrowserSearch.clear();
    m_apiBrowserSelectedReferenceIndex = 0;
    m_apiBrowserStatus.clear();
    m_serverVersion = "n/a";
    m_serverApiVersion = "n/a";
    m_serverUuid = QUuid();
    m_serverName.clear();
    closeConfigureDialog();

    const NymeaJsonRpcClient::TransportSecurity security = m_options.useSsl ? NymeaJsonRpcClient::TransportSecurity::SslTls : NymeaJsonRpcClient::TransportSecurity::PlainTcp;
    if (m_client.connectToHost(m_options.host, static_cast<quint16>(m_options.port), security, m_options.timeoutMs)) {
        m_connectionStatus = std::string(m_client.isEncrypted() ? "SSL connected to " : "TCP connected to ") + endpoint();
        updateCertificateWarning();
        return true;
    }

    m_connectionLost = true;
    m_connectionStatus = "Connection lost, try to reconnect.";
    m_isAuthenticationRequired = false;
    m_isAuthenticated = false;
    m_pushButtonAuthAvailable = false;
    m_pushButtonAuthPending = false;
    m_pushButtonAuthTransactionId = -1;
    m_showLoginForm = false;
    m_authStatus = m_connectionStatus;
    m_thingManager.clear();
    m_thingManager.setStatus(m_connectionStatus);
    return false;
}

void Engine::handleHelloReply(const QJsonObject& message, const QString& transportError)
{
    m_helloPending = false;

    if (!transportError.isEmpty()) {
        m_connectionLost = true;
        m_connectionStatus = "Connection lost, try to reconnect.";
        m_thingManager.setStatus("No reply for JSONRPC.Hello: " + transportError.toStdString());
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    const int requestId = message.value(QStringLiteral("id")).toInt(-1);
    m_connectionLost = false;
    const QJsonObject helloParams = message.value(QStringLiteral("params")).toObject();
    const QString serverNameValue = helloParams.value(QStringLiteral("name")).toString();
    if (!serverNameValue.isEmpty()) {
        m_serverName = serverNameValue;
    }

    const QUuid serverUuidValue = QUuid(helloParams.value(QStringLiteral("uuid")).toString());
    if (!serverUuidValue.isNull()) {
        m_serverUuid = serverUuidValue;
    }

    const QString serverVersionValue = helloParams.value(QStringLiteral("version")).toString();
    if (!serverVersionValue.isEmpty()) {
        m_serverVersion = serverVersionValue.toStdString();
    }

    QString serverApiVersionValue = helloParams.value(QStringLiteral("protocol version")).toString();
    if (serverApiVersionValue.isEmpty()) {
        serverApiVersionValue = helloParams.value(QStringLiteral("protocolVersion")).toString();
    }
    if (serverApiVersionValue.isEmpty()) {
        serverApiVersionValue = message.value(QStringLiteral("jsonrpc")).toString();
    }
    if (!serverApiVersionValue.isEmpty()) {
        m_serverApiVersion = serverApiVersionValue.toStdString();
    }

    if (!m_serverUuid.isNull()) {
        if (const auto savedConnection = m_connectionSettings.loadConnectionByUuid(m_serverUuid); savedConnection.has_value()) {
            m_savedConnection = savedConnection;
        } else if (!m_savedConnection.has_value() || m_savedConnection->hostUuid != m_serverUuid) {
            m_savedConnection.reset();
        }
    }

    updateCertificateWarning();

    m_isAuthenticationRequired = helloParams.value(QStringLiteral("authenticationRequired")).toBool();
    m_pushButtonAuthAvailable = helloParams.value(QStringLiteral("pushButtonAuthAvailable")).toBool();
    const bool helloAuthenticated = helloParams.value(QStringLiteral("authenticated")).toBool();
    const bool allowAutoAuthenticate = !m_skipNextAutoAuthenticate;
    m_skipNextAutoAuthenticate = false;

    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_apiBrowserLoaded = false;
        m_apiBrowserPending = false;
        m_apiBrowserIntrospection = QJsonObject();
        m_apiBrowserHistory.clear();
        m_apiBrowserSelectedSection.clear();
        m_apiBrowserSelectedName.clear();
        m_apiBrowserSelectedReferenceIndex = 0;
        m_apiBrowserStatus = "Stored token was rejected. Please login.";
        enterAuthenticationRequiredState(m_pushButtonAuthAvailable ? std::string() : "Stored token was rejected. Please login.", true);
        m_thingManager.setStatus("JSONRPC.Hello unauthorized (request id " + std::to_string(requestId) + ").");
        return;
    }

    if (status == QStringLiteral("error")) {
        m_connectionLost = true;
        m_connectionStatus = "Connection lost, try to reconnect.";
        m_thingManager.setStatus("JSONRPC.Hello returned error (request id " + std::to_string(requestId) + ").");
        return;
    }

    m_isAuthenticated = helloAuthenticated || !m_isAuthenticationRequired;
    m_showLoginForm = m_isAuthenticationRequired && !m_isAuthenticated;

    if (!m_isAuthenticationRequired) {
        m_authStatus = "Server does not require authentication.";
    } else if (m_isAuthenticated) {
        m_authStatus = m_client.authToken().isEmpty() ? "Authenticated." : "Authenticated using stored token.";
    } else {
        m_authStatus = "Authentication required.";
    }
    if (m_isAuthenticated || !m_isAuthenticationRequired) {
        m_ignoreStoredToken = false;
        m_logoutStatus.clear();
    }

    saveCurrentConnection(false);
    m_thingManager.setStatus("JSONRPC.Hello succeeded (request id " + std::to_string(requestId) + ").");

    if (m_isAuthenticationRequired && !m_isAuthenticated) {
        if (m_pushButtonAuthAvailable) {
            enterAuthenticationRequiredState("", true);
        } else if (allowAutoAuthenticate && !m_username.empty() && !m_password.empty()) {
            authenticate(m_username, m_password);
        } else {
            enterAuthenticationRequiredState("Authentication required. Enter username/password and press Enter.", false);
        }
        return;
    }

    if (m_focusArea == FocusArea::LoginForm) {
        m_focusArea = FocusArea::MainMenu;
    }

    if (m_mainView == MainView::ApiBrowser) {
        ensureApiBrowserLoaded();
    }

    enableNotifications(true);
}

void Engine::sendHello()
{
    if (!m_client.isConnected()) {
        m_thingManager.setStatus("Cannot send JSONRPC.Hello while disconnected.");
        return;
    }
    if (m_helloPending) {
        return;
    }

    m_helloPending = true;

    QJsonObject params;
    params.insert(QStringLiteral("locale"), QLocale().name());

    observeReply(m_client.sendRequest(QStringLiteral("JSONRPC.Hello"), params),
                 [this](const QJsonObject& message, const QString& transportError) { handleHelloReply(message, transportError); });
}

void Engine::handleAuthenticateReply(const QJsonObject& message, const QString& transportError)
{
    m_authenticationPending = false;

    if (!transportError.isEmpty()) {
        m_authStatus = "No reply for authenticate request: " + transportError.toStdString();
        enterAuthenticationRequiredState(m_authStatus, m_pushButtonAuthAvailable);
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    const int requestId = message.value(QStringLiteral("id")).toInt(-1);
    if (status == QStringLiteral("error") || status == QStringLiteral("unauthorized")) {
        if (status == QStringLiteral("unauthorized")) {
            clearStoredToken();
        }
        m_client.clearAuthToken();
        m_apiBrowserLoaded = false;
        m_apiBrowserPending = false;
        m_apiBrowserIntrospection = QJsonObject();
        m_apiBrowserHistory.clear();
        m_apiBrowserSelectedSection.clear();
        m_apiBrowserSelectedName.clear();
        m_apiBrowserSearch.clear();
        m_apiBrowserSelectedReferenceIndex = 0;
        m_apiBrowserStatus = "Authentication rejected by server.";
        enterAuthenticationRequiredState("Authentication rejected by server.", true);
        return;
    }

    const api::JSONRPCAuthenticateResponse response = api::JSONRPCAuthenticateResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (!response.success) {
        m_client.clearAuthToken();
        enterAuthenticationRequiredState("Authentication failed.", true);
        return;
    }

    const QString token = response.token.value_or(QString());
    if (token.isEmpty()) {
        m_client.clearAuthToken();
        enterAuthenticationRequiredState("Authentication succeeded but no token was returned.", true);
        return;
    }

    completeAuthentication(token,
                           response.username,
                           response.username.has_value() && !response.username->isEmpty() ? "Authenticated as " + response.username->toStdString() + "." : "Authenticated.");
    m_thingManager.setStatus("Authentication succeeded (request id " + std::to_string(requestId) + ").");
}

void Engine::authenticate(const std::string& username, const std::string& password)
{
    if (!m_client.isConnected()) {
        m_authStatus = "Cannot authenticate while disconnected.";
        return;
    }

    if (username.empty() || password.empty()) {
        m_authStatus = "Username and password are required.";
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        return;
    }

    if (m_authenticationPending) {
        return;
    }
    m_authenticationPending = true;

    QJsonObject params;
    params.insert(QStringLiteral("username"), QString::fromStdString(username));
    params.insert(QStringLiteral("password"), QString::fromStdString(password));
    params.insert(QStringLiteral("deviceName"), QStringLiteral("nymea-cli"));

    m_authStatus = "Authenticating...";
    observeReply(m_client.sendRequest(QStringLiteral("JSONRPC.Authenticate"), params),
                 [this](const QJsonObject& message, const QString& transportError) { handleAuthenticateReply(message, transportError); });
}

void Engine::logout()
{
    if (!m_client.isConnected() || !m_isAuthenticated) {
        m_authStatus = "Logout is only available after authentication.";
        return;
    }

    m_showLogoutConfirm = true;
    m_logoutStatus = "Logout revokes the current token on the server, clears the saved token locally, and reconnects to the same server.";
}

void Engine::finalizeLogout()
{
    m_showLogoutConfirm = false;
    m_logoutRequestPending = false;
    m_skipNextAutoAuthenticate = true;
    m_ignoreStoredToken = true;
    m_mainView = MainView::Things;
    m_selectedMainMenuEntry = MainMenuEntry::Things;
    const bool storedTokenCleared = clearStoredToken();
    if (!storedTokenCleared && !m_settingsWarning.empty() && !m_logoutStatus.empty()) {
        m_logoutStatus += " " + m_settingsWarning;
    }
    if (!m_logoutStatus.empty()) {
        m_authStatus = m_logoutStatus;
    } else if (!m_settingsWarning.empty()) {
        m_authStatus = m_settingsWarning;
    } else {
        m_authStatus = "Logging out...";
    }
    m_client.clearAuthToken();
    m_isAuthenticationRequired = false;
    m_isAuthenticated = false;
    m_notificationsEnabled = false;
    m_showLoginForm = true;
    m_loginSelectedInputIndex = 0;
    m_focusArea = FocusArea::LoginForm;
    closeActionDialog();
    closeConfigureDialog();
    m_showThingDetailInspector = false;

    m_client.disconnectFromHost();
    if (!connectToServer(storedTokenCleared)) {
        m_authStatus = m_connectionStatus;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        return;
    }

    runHandshakeAndLoadThings();
}

void Engine::revokeCurrentTokenAndLogout()
{
    if (m_logoutRequestPending) {
        return;
    }

    const std::optional<QUuid> tokenId = currentTokenId();
    if (!tokenId.has_value()) {
        m_logoutStatus = "Cannot revoke this token because its id is unavailable.";
        finalizeLogout();
        return;
    }

    if (!m_client.isConnected()) {
        m_logoutStatus = "Cannot revoke the token while disconnected.";
        finalizeLogout();
        return;
    }

    m_logoutRequestPending = true;
    m_logoutStatus = "Revoking token...";

    api::UsersRemoveTokenParams params;
    params.tokenId = *tokenId;
    observeReply(m_client.sendRequest(api::UsersRemoveTokenMethod::methodName(), params.toJson()), [this](const QJsonObject& message, const QString& transportError) {
        m_logoutRequestPending = false;

        if (!transportError.isEmpty()) {
            m_logoutStatus = "Token revocation failed: " + transportError.toStdString() + ". Logging out locally.";
            finalizeLogout();
            return;
        }

        const QString status = message.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("error") || status == QStringLiteral("unauthorized")) {
            m_logoutStatus = "Token revocation was rejected by the server. Logging out locally.";
            finalizeLogout();
            return;
        }

        const api::UsersRemoveTokenResponse response = api::UsersRemoveTokenResponse::fromJson(message.value(QStringLiteral("params")).toObject());
        if (response.error != api::UserError::UserErrorNoError) {
            m_logoutStatus = "Token revocation returned " + api::toString(response.error).toStdString() + ". Logging out locally.";
        } else {
            m_logoutStatus = "Token revoked. Logging out...";
        }
        finalizeLogout();
    });
}

void Engine::openPowerActionConfirmDialog(PowerAction action)
{
    m_systemAction = action;
    m_showSystemActionConfirm = true;
    m_systemActionStatus = "Press Enter to confirm or Esc to cancel.";
}

void Engine::closePowerActionConfirmDialog()
{
    m_showSystemActionConfirm = false;
    m_systemActionRequestPending = false;
}

void Engine::executePowerAction()
{
    if (!m_client.isConnected() || m_systemActionRequestPending) {
        return;
    }

    m_systemActionRequestPending = true;
    m_systemActionStatus = "Sending " + powerActionLabel(static_cast<int>(m_systemAction)) + " request...";

    auto send = [&](const QString& method, auto params, auto handler) { observeReply(m_client.sendRequest(method, params.toJson()), handler); };

    switch (m_systemAction) {
    case PowerAction::Shutdown: {
        api::SystemShutdownParams params;
        send(api::SystemShutdownMethod::methodName(), params, [this](const QJsonObject& message, const QString& transportError) {
            handlePowerActionReply(message, transportError, PowerAction::Shutdown);
        });
        break;
    }
    case PowerAction::Restart: {
        api::SystemRestartParams params;
        send(api::SystemRestartMethod::methodName(), params, [this](const QJsonObject& message, const QString& transportError) {
            handlePowerActionReply(message, transportError, PowerAction::Restart);
        });
        break;
    }
    case PowerAction::Reboot: {
        api::SystemRebootParams params;
        send(api::SystemRebootMethod::methodName(), params, [this](const QJsonObject& message, const QString& transportError) {
            handlePowerActionReply(message, transportError, PowerAction::Reboot);
        });
        break;
    }
    }
}

void Engine::handleEnableNotificationsReply(const QJsonObject& message, const QString& transportError, bool fetchThingsAfterReply)
{
    m_notificationSetupPending = false;

    if (!transportError.isEmpty()) {
        m_notificationsEnabled = false;
        m_thingManager.setStatus("Failed to enable notifications: " + transportError.toStdString());
        if (fetchThingsAfterReply) {
            fetchThings();
        }
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_thingManager.setStatus("Notification setup unauthorized.");
        return;
    }

    if (status == QStringLiteral("error")) {
        m_notificationsEnabled = false;
        m_thingManager.setStatus("Notification setup returned error.");
        if (fetchThingsAfterReply) {
            fetchThings();
        }
        return;
    }

    const api::JSONRPCSetNotificationStatusResponse response = api::JSONRPCSetNotificationStatusResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_notificationsEnabled = response.enabled || response.namespaces.contains(QStringLiteral("Integrations"));
    if (!m_notificationsEnabled) {
        m_thingManager.setStatus("Server did not confirm Integrations notifications.");
    } else {
        m_thingManager.setStatus(m_thingManager.status() + " Live updates enabled for Integrations.");
    }

    if (fetchThingsAfterReply) {
        fetchThings();
    }
}

void Engine::enableNotifications(bool fetchThingsAfterReply)
{
    if (m_notificationsEnabled || m_notificationSetupPending) {
        if (fetchThingsAfterReply) {
            fetchThings();
        }
        return;
    }

    if (!m_client.isConnected()) {
        m_thingManager.setStatus("Cannot enable notifications while disconnected.");
        if (fetchThingsAfterReply) {
            fetchThings();
        }
        return;
    }

    if (m_isAuthenticationRequired && !m_isAuthenticated) {
        m_thingManager.setStatus("Authentication required before enabling notifications.");
        return;
    }

    m_notificationSetupPending = true;

    api::JSONRPCSetNotificationStatusParams request;
    request.enabled = true;
    request.namespaces = QStringList{QStringLiteral("Integrations")};

    observeReply(m_client.sendRequest(api::JSONRPCSetNotificationStatusMethod::methodName(), request.toJson()),
                 [this, fetchThingsAfterReply](const QJsonObject& message, const QString& transportError) {
                     handleEnableNotificationsReply(message, transportError, fetchThingsAfterReply);
                 });
}

void Engine::handleFetchThingsReply(const QJsonObject& message, const QString& transportError)
{
    m_fetchThingsPending = false;
    const QUuid selectedId = m_preferredThingSelectionId;
    m_preferredThingSelectionId = QUuid();

    if (!transportError.isEmpty()) {
        m_thingManager.setStatus("No reply for Integrations.GetThings: " + transportError.toStdString());
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    const int requestId = message.value(QStringLiteral("id")).toInt(-1);
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_thingManager.setStatus("Integrations.GetThings unauthorized.");
        return;
    }

    if (status == QStringLiteral("error")) {
        m_thingManager.setStatus("Integrations.GetThings returned error.");
        return;
    }

    std::string errorMessage;
    if (!m_thingManager.updateFromReply(message, errorMessage)) {
        m_thingManager.setStatus(errorMessage);
        return;
    }

    clampThingSelection(selectedId);
    clampThingDetailSelection();
    clampConfigureThingSelection();
    m_thingManager.setStatus("Loaded " + std::to_string(m_thingManager.things().size()) + " thing(s) (request id " + std::to_string(requestId) + ").");
    fetchThingClasses();
}

void Engine::fetchThings()
{
    if (m_fetchThingsPending) {
        return;
    }

    m_preferredThingSelectionId = selectedThingId();
    m_thingManager.clear();
    m_haveAllThingClasses = false;

    if (!m_client.isConnected()) {
        m_thingManager.setStatus("Cannot fetch things while disconnected.");
        return;
    }

    if (m_isAuthenticationRequired && !m_isAuthenticated) {
        m_thingManager.setStatus("Authentication required before fetching things.");
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        return;
    }

    m_fetchThingsPending = true;
    m_thingManager.setStatus("Loading things...");

    observeReply(m_client.sendRequest(QStringLiteral("Integrations.GetThings"), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchThingsReply(message, transportError); });
}

void Engine::handleFetchThingClassesReply(const QJsonObject& message, const QString& transportError)
{
    m_fetchThingClassesPending = false;
    const QUuid selectedId = selectedThingId();

    if (!transportError.isEmpty()) {
        m_thingManager.setStatus("Thing list loaded, but no reply for Integrations.GetThingClasses: " + transportError.toStdString());
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    const int requestId = message.value(QStringLiteral("id")).toInt(-1);
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_thingManager.setStatus("Integrations.GetThingClasses unauthorized.");
        return;
    }

    if (status == QStringLiteral("error")) {
        m_thingManager.setStatus("Thing list loaded, but Integrations.GetThingClasses returned error.");
        return;
    }

    std::string errorMessage;
    if (!m_thingManager.updateThingClassesFromReply(message, errorMessage)) {
        m_thingManager.setStatus("Thing list loaded, but thing class metadata is unavailable: " + errorMessage);
        return;
    }

    clampThingSelection(selectedId);
    clampThingDetailSelection();
    m_thingManager.setStatus("Loaded " + std::to_string(m_thingManager.things().size()) + " thing(s) and enriched them with type metadata (request id " + std::to_string(requestId)
                             + ").");
    if (!m_haveAllThingClasses) {
        fetchAllThingClasses();
    }
}

void Engine::fetchThingClasses()
{
    if (m_fetchThingClassesPending) {
        return;
    }

    const std::vector<std::string> thingClassIds = m_thingManager.thingClassIds();
    if (thingClassIds.empty()) {
        return;
    }

    m_fetchThingClassesPending = true;

    api::IntegrationsGetThingClassesParams params;
    QList<QUuid> ids;
    for (const std::string& thingClassId : thingClassIds) {
        ids.append(QUuid(QString::fromStdString(thingClassId)));
    }
    params.thingClassIds = ids;

    observeReply(m_client.sendRequest(QStringLiteral("Integrations.GetThingClasses"), params.toJson()),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchThingClassesReply(message, transportError); });
}

void Engine::fetchAllThingClasses()
{
    if (m_fetchAllThingClassesPending || m_haveAllThingClasses) {
        return;
    }

    if (!m_client.isConnected()) {
        return;
    }

    if (m_isAuthenticationRequired && !m_isAuthenticated) {
        return;
    }

    m_fetchAllThingClassesPending = true;
    observeReply(m_client.sendRequest(api::IntegrationsGetThingClassesMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchAllThingClassesReply(message, transportError); });
}

void Engine::ensureSystemCapabilitiesLoaded()
{
    if (m_systemCapabilitiesLoaded || m_systemCapabilitiesPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemCapabilitiesPending = true;
    observeReply(m_client.sendRequest(api::SystemGetCapabilitiesMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemCapabilitiesReply(message, transportError); });
}

void Engine::ensureSystemTimeLoaded()
{
    if (m_systemTimeLoaded || m_systemTimePending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemTimePending = true;
    observeReply(m_client.sendRequest(api::SystemGetTimeMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemTimeReply(message, transportError); });
}

void Engine::ensureSystemUpdateStatusLoaded()
{
    if (m_systemUpdateStatusLoaded || m_systemUpdateStatusPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemUpdateStatusPending = true;
    observeReply(m_client.sendRequest(api::SystemGetUpdateStatusMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemUpdateStatusReply(message, transportError); });
}

void Engine::ensureSystemPackagesLoaded()
{
    if (m_systemPackagesLoaded || m_systemPackagesPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemPackagesPending = true;
    observeReply(m_client.sendRequest(api::SystemGetPackagesMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemPackagesReply(message, transportError); });
}

void Engine::ensureSystemTimeZonesLoaded()
{
    if (m_systemTimeZonesLoaded || m_systemTimeZonesPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemTimeZonesPending = true;
    observeReply(m_client.sendRequest(api::SystemGetTimeZonesMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemTimeZonesReply(message, transportError); });
}

QStringList Engine::filteredSystemTimeZones() const
{
    if (m_systemTimeZoneSearch.empty()) {
        return m_systemTimeZones;
    }

    QStringList filtered;
    const QString search = QString::fromStdString(m_systemTimeZoneSearch);
    for (const QString& timeZone : m_systemTimeZones) {
        if (caseInsensitiveContains(timeZone, search)) {
            filtered.append(timeZone);
        }
    }
    return filtered;
}

void Engine::handleFetchSystemCapabilitiesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemCapabilitiesPending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load system capabilities: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_settingsWarning = "Settings warning: system capabilities request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: system capabilities request returned an error.";
        return;
    }

    m_systemCapabilities = api::SystemGetCapabilitiesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_systemCapabilitiesLoaded = true;
    m_settingsWarning.clear();
}

void Engine::handleFetchSystemTimeReply(const QJsonObject& message, const QString& transportError)
{
    m_systemTimePending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load system time: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_settingsWarning = "Settings warning: system time request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: system time request returned an error.";
        return;
    }

    m_systemTime = api::SystemGetTimeResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_systemTimeLoaded = true;
    m_settingsWarning.clear();
}

void Engine::handleFetchSystemUpdateStatusReply(const QJsonObject& message, const QString& transportError)
{
    m_systemUpdateStatusPending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load update status: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_settingsWarning = "Settings warning: update status request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: update status request returned an error.";
        return;
    }

    m_systemUpdateStatus = api::SystemGetUpdateStatusResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_systemUpdateStatusLoaded = true;
    m_settingsWarning.clear();
}

void Engine::handleFetchSystemPackagesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemPackagesPending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load packages: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_settingsWarning = "Settings warning: package request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: package request returned an error.";
        return;
    }

    const api::SystemGetPackagesResponse response = api::SystemGetPackagesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_systemPackages.clear();
    m_systemPackages.reserve(response.packages.size());
    for (const api::Package& package : response.packages) {
        m_systemPackages.push_back(package);
    }
    m_systemPackagesLoaded = true;
    m_settingsWarning.clear();
}

void Engine::handleFetchSystemTimeZonesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemTimeZonesPending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load time zones: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_settingsWarning = "Settings warning: time zone request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: time zone request returned an error.";
        return;
    }

    const api::SystemGetTimeZonesResponse response = api::SystemGetTimeZonesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_systemTimeZones = response.timeZones;
    m_systemTimeZonesLoaded = true;
    m_settingsWarning.clear();
}

void Engine::handleCheckForUpdatesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemActionRequestPending = false;
    if (!transportError.isEmpty()) {
        m_systemActionStatus = "Check for updates failed: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_systemActionStatus = "Check for updates was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_systemActionStatus = "Check for updates returned an error.";
        return;
    }

    const api::SystemCheckForUpdatesResponse response = api::SystemCheckForUpdatesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (!response.success) {
        m_systemActionStatus = "Check for updates failed.";
        return;
    }

    m_systemActionStatus = "Update check started.";
    m_settingsWarning.clear();
    m_systemUpdateStatusLoaded = false;
    m_systemPackagesLoaded = false;
    ensureSystemUpdateStatusLoaded();
    ensureSystemPackagesLoaded();
}

void Engine::handleSetTimeZoneReply(const QJsonObject& message, const QString& transportError)
{
    m_systemActionRequestPending = false;
    if (!transportError.isEmpty()) {
        m_systemActionStatus = "Failed to set time zone: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_systemActionStatus = "Setting the time zone was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_systemActionStatus = "Setting the time zone returned an error.";
        return;
    }

    const api::ConfigurationSetTimeZoneResponse response = api::ConfigurationSetTimeZoneResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.configurationError != api::ConfigurationError::ConfigurationErrorNoError) {
        m_systemActionStatus = "Setting the time zone failed: " + api::toString(response.configurationError).toStdString();
        return;
    }

    m_systemActionStatus = "Time zone updated.";
    m_settingsWarning.clear();
    m_systemTimeLoaded = false;
    ensureSystemTimeLoaded();
}

void Engine::handleUpdatePackagesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemActionRequestPending = false;
    if (!transportError.isEmpty()) {
        m_systemActionStatus = "Update request failed: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_systemActionStatus = "Updating packages was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_systemActionStatus = "Update request returned an error.";
        return;
    }

    const api::SystemUpdatePackagesResponse response = api::SystemUpdatePackagesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (!response.success) {
        m_systemActionStatus = "Update request failed.";
        return;
    }

    m_systemActionStatus = "Update started.";
    m_settingsWarning.clear();
    m_systemUpdateStatusLoaded = false;
    m_systemPackagesLoaded = false;
    ensureSystemUpdateStatusLoaded();
    ensureSystemPackagesLoaded();
}

void Engine::handlePowerActionReply(const QJsonObject& message, const QString& transportError, PowerAction action)
{
    m_systemActionRequestPending = false;
    if (!transportError.isEmpty()) {
        m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " failed: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " returned an error.";
        return;
    }

    bool success = false;
    switch (action) {
    case PowerAction::Shutdown:
        success = api::SystemShutdownResponse::fromJson(message.value(QStringLiteral("params")).toObject()).success;
        break;
    case PowerAction::Restart:
        success = api::SystemRestartResponse::fromJson(message.value(QStringLiteral("params")).toObject()).success;
        break;
    case PowerAction::Reboot:
        success = api::SystemRebootResponse::fromJson(message.value(QStringLiteral("params")).toObject()).success;
        break;
    }

    if (!success) {
        m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " request was rejected.";
        return;
    }

    m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " requested.";
    m_settingsWarning.clear();
    closePowerActionConfirmDialog();
}

void Engine::loadSavedConnection()
{
    m_savedConnection = m_connectionSettings.loadConnectionByEndpoint(m_options.host, m_options.port, m_options.useSsl);
    if (!m_savedConnection.has_value()) {
        return;
    }

    if (!m_savedConnection->token.isEmpty()) {
        if (m_ignoreStoredToken) {
            m_savedConnection->token.clear();
            return;
        }
        m_client.setAuthToken(m_savedConnection->token);
        m_authStatus = "Using stored token for " + connectionDisplayName() + ".";
    }
}

void Engine::updateCertificateWarning()
{
    m_securityWarning.clear();

    if (!m_options.useSsl || !m_savedConnection.has_value()) {
        return;
    }

    if (m_savedConnection->certificateFingerprint.isEmpty()) {
        return;
    }

    const QString currentFingerprint = m_client.peerCertificateFingerprint();
    if (currentFingerprint.isEmpty() || currentFingerprint == m_savedConnection->certificateFingerprint) {
        return;
    }

    m_securityWarning = "TLS certificate fingerprint changed for " + connectionDisplayName() + ". Stored: " + m_savedConnection->certificateFingerprint.toStdString()
                        + " Current: " + currentFingerprint.toStdString() + ".";
}

bool Engine::clearStoredToken()
{
    QUuid hostUuid = m_serverUuid;
    if (hostUuid.isNull() && m_savedConnection.has_value()) {
        hostUuid = m_savedConnection->hostUuid;
    }

    QString errorMessage;
    if (!m_connectionSettings.clearToken(hostUuid, errorMessage)) {
        m_settingsWarning = "Settings warning: " + errorMessage.toStdString();
        if (m_savedConnection.has_value() && m_savedConnection->hostUuid == hostUuid) {
            m_savedConnection->token.clear();
        }
        return false;
    }

    if (m_savedConnection.has_value() && m_savedConnection->hostUuid == hostUuid) {
        m_savedConnection->token.clear();
    }
    return true;
}

void Engine::saveCurrentConnection(bool allowFingerprintUpdate)
{
    if (m_serverUuid.isNull()) {
        return;
    }

    const SavedConnection connection = currentConnection(allowFingerprintUpdate);
    QString errorMessage;
    if (!m_connectionSettings.saveConnection(connection, errorMessage)) {
        m_settingsWarning = "Settings warning: " + errorMessage.toStdString();
        return;
    }

    m_savedConnection = connection;
    m_settingsWarning.clear();
}

void Engine::runHandshakeAndLoadThings()
{
    sendHello();
}

ftxui::Element Engine::renderMainMenu() const
{
    constexpr std::array<const char*, 6> menuItems = {"Things", "Configure things", "API browser", "Settings", "Logout", "About"};
    const bool apiBrowserEnabled = m_client.isConnected() && (!m_isAuthenticationRequired || m_isAuthenticated);
    const bool logoutEnabled = m_client.isConnected() && m_isAuthenticated && !m_client.authToken().isEmpty();

    ftxui::Elements entries;
    for (int index = 0; index < static_cast<int>(menuItems.size()); ++index) {
        const bool selected = (m_selectedMainMenuEntry == MainMenuEntry::Things && index == 0) || (m_selectedMainMenuEntry == MainMenuEntry::ConfigureThings && index == 1)
                              || (m_selectedMainMenuEntry == MainMenuEntry::ApiBrowser && index == 2) || (m_selectedMainMenuEntry == MainMenuEntry::Settings && index == 3)
                              || (m_selectedMainMenuEntry == MainMenuEntry::Logout && index == 4) || (m_selectedMainMenuEntry == MainMenuEntry::About && index == 5);
        auto entry = ftxui::text(std::string(" ") + menuItems.at(index) + " ");
        if ((index == 2 && !apiBrowserEnabled) || (index == 4 && !logoutEnabled)) {
            entry = entry | ftxui::dim;
        }
        if (selected && ((index == 2 && apiBrowserEnabled) || (index == 4 && logoutEnabled) || (index != 2 && index != 4))) {
            entry = entry | ftxui::bold | ftxui::inverted;
        }
        if (m_focusArea == FocusArea::MainMenu && selected && ((index == 2 && apiBrowserEnabled) || (index == 4 && logoutEnabled) || (index != 2 && index != 4))) {
            entry = entry | ftxui::color(ftxui::Color::CyanLight);
        }
        if (selected && ((index == 2 && apiBrowserEnabled) || (index == 4 && logoutEnabled) || (index != 2 && index != 4))) {
            entry = entry | ftxui::focus;
        }
        entries.push_back(entry);
    }

    return renderFocusedWindow(ftxui::text("Menu"), ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::frame, m_focusArea == FocusArea::MainMenu)
           | ftxui::reflect(m_mainMenuBox);
}

ftxui::Element Engine::renderThingList() const
{
    ftxui::Elements lines;
    lines.push_back(ftxui::text(m_thingManager.status()));
    auto search = ftxui::text("Search: " + (m_thingSearch.empty() ? std::string("<type to filter>") : m_thingSearch));
    if (m_focusArea == FocusArea::ThingSearch) {
        search = renderActiveField(std::move(search) | ftxui::inverted | ftxui::bold | ftxui::color(ftxui::Color::CyanLight), true, 28);
    }
    lines.push_back(search);
    lines.push_back(ftxui::text("Sort: " + thingSortModeLabel() + "  Filter: " + thingCategoryLabel(m_thingCategoryFilter)));
    lines.push_back(ftxui::separator());

    const std::vector<const api::Thing*> things = filteredThings();
    if (m_thingManager.things().empty()) {
        lines.push_back(ftxui::text("No things found."));
    } else if (things.empty()) {
        lines.push_back(ftxui::text("No things match the current filter."));
    } else {
        ThingCategory lastCategory = ThingCategory::All;
        for (int index = 0; index < static_cast<int>(things.size()); ++index) {
            const api::Thing* thing = things.at(index);
            const ThingCategory category = thingCategory(*thing);
            if (m_thingSortMode == ThingSortMode::Grouped && category != lastCategory) {
                lines.push_back(ftxui::text(thingCategoryLabel(category)) | ftxui::bold);
                lastCategory = category;
            }

            auto entry = ftxui::text(" " + thingLabel(thing) + " ");
            if (index == m_selectedThingIndex) {
                entry = entry | ftxui::bold | ftxui::inverted;
            }
            if (m_focusArea == FocusArea::ThingList && index == m_selectedThingIndex) {
                entry = entry | ftxui::color(ftxui::Color::CyanLight);
            }
            if (index == m_selectedThingIndex) {
                entry = entry | ftxui::focus;
            }
            lines.push_back(entry);
        }
    }

    return renderFocusedWindow(ftxui::text("Things"),
                               ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame,
                               m_focusArea == FocusArea::ThingSearch || m_focusArea == FocusArea::ThingList)
           | ftxui::reflect(m_thingListBox);
}

ftxui::Element Engine::renderThingDetails() const
{
    const api::Thing* thing = selectedThing();
    if (thing == nullptr) {
        return ftxui::text("No thing selected.");
    }

    const api::ThingClass* thingClass = m_thingManager.thingClassForThing(*thing);
    const std::vector<ThingDetailEntry> detailEntries = buildThingDetailEntries(thing, thingClass);
    const ThingDetailEntry* selectedEntry = detailEntries.empty() || m_selectedThingDetailIndex < 0 || m_selectedThingDetailIndex >= static_cast<int>(detailEntries.size())
                                                ? nullptr
                                                : &detailEntries.at(m_selectedThingDetailIndex);
    const std::string thingClassLabel = [&] {
        if (thingClass == nullptr) {
            return firstNonEmpty({uuidToStd(thing->thingClassId), "<unknown thing class>"});
        }

        const std::string displayName = firstNonEmpty({thingClass->displayName.toStdString(), thingClass->name.toStdString()});
        if (displayName.empty()) {
            return firstNonEmpty({uuidToStd(thing->thingClassId), "<unknown thing class>"});
        }
        if (thingClass->name.isEmpty() || thingClass->name.toStdString() == displayName) {
            return displayName;
        }
        return displayName + " (" + thingClass->name.toStdString() + ")";
    }();

    ftxui::Elements metadata;
    metadata.push_back(ftxui::text("Name: " + firstNonEmpty({optionalQStringToStd(thing->name), "<unnamed>"})));
    metadata.push_back(ftxui::text("Id: " + uuidToStd(thing->id)));
    metadata.push_back(ftxui::text("Thing class: " + thingClassLabel));
    metadata.push_back(ftxui::text("Thing class id: " + uuidToStd(thing->thingClassId)));
    metadata.push_back(ftxui::text("Setup status: " + api::toString(thing->setupStatus).toStdString()));
    metadata.push_back(ftxui::text("Setup error: " + api::toString(thing->setupError).toStdString()));
    metadata.push_back(ftxui::text("Setup complete: " + std::string(thing->setupComplete ? "true" : "false")));
    if (thing->parentId.has_value()) {
        metadata.push_back(ftxui::text("Parent id: " + uuidToStd(*thing->parentId)));
    }
    if (thing->setupDisplayMessage.has_value()) {
        metadata.push_back(ftxui::paragraph("Setup message: " + thing->setupDisplayMessage->toStdString()));
    }

    ftxui::Elements browserRows;
    browserRows.push_back(ftxui::text("Params") | ftxui::bold);
    if (thing->params.empty()) {
        browserRows.push_back(ftxui::text("No params."));
    } else {
        for (int index = 0; index < thing->params.size(); ++index) {
            const api::Param& param = thing->params.at(index);
            const api::ParamType* paramType = m_thingManager.paramTypeForThing(*thing, param);
            const std::string label = [&] {
                if (paramType != nullptr) {
                    return firstNonEmpty({paramType->displayName.toStdString(), paramType->name.toStdString()});
                }
                if (param.paramTypeId.has_value()) {
                    return uuidToStd(*param.paramTypeId);
                }
                return std::string("<unknown param>");
            }();
            const bool isSelected = selectedEntry != nullptr && selectedEntry->type == ThingDetailEntry::Type::Param && selectedEntry->index == index;
            const std::optional<api::BasicType> basicType = paramType != nullptr ? std::optional<api::BasicType>(paramType->type) : std::nullopt;
            const std::optional<api::Unit> unit = paramType != nullptr ? paramType->unit : std::nullopt;
            browserRows.push_back(renderTwoColumnRow(label, renderValueCell(param.value, basicType, unit), isSelected, m_focusArea == FocusArea::ThingDetails));
        }
    }

    browserRows.push_back(ftxui::separator());
    browserRows.push_back(ftxui::text("States") | ftxui::bold);
    if (thing->states.empty()) {
        browserRows.push_back(ftxui::text("No states."));
    } else {
        for (int index = 0; index < thing->states.size(); ++index) {
            const api::State& state = thing->states.at(index);
            const api::StateType* stateType = m_thingManager.stateTypeForThing(*thing, state);
            const std::string label = [&] {
                if (stateType != nullptr) {
                    return firstNonEmpty({stateType->displayName.toStdString(), stateType->name.toStdString()});
                }
                return firstNonEmpty({uuidToStd(state.stateTypeId), "<unknown state>"});
            }();
            const bool isSelected = selectedEntry != nullptr && selectedEntry->type == ThingDetailEntry::Type::State && selectedEntry->index == index;
            const std::optional<api::BasicType> basicType = stateType != nullptr ? std::optional<api::BasicType>(stateType->type) : std::nullopt;
            const std::optional<api::Unit> unit = stateType != nullptr ? stateType->unit : std::nullopt;
            browserRows.push_back(renderTwoColumnRow(label, renderValueCell(state.value, basicType, unit), isSelected, m_focusArea == FocusArea::ThingDetails));
        }
    }

    browserRows.push_back(ftxui::separator());
    browserRows.push_back(ftxui::text("Actions") | ftxui::bold);
    if (thingClass == nullptr || thingClass->actionTypes.empty()) {
        browserRows.push_back(ftxui::text("No actions."));
    } else {
        for (int index = 0; index < thingClass->actionTypes.size(); ++index) {
            const api::ActionType& actionType = thingClass->actionTypes.at(index);
            const std::string label = firstNonEmpty({actionType.displayName.toStdString(), actionType.name.toStdString(), "<unnamed action>"});
            const bool isSelected = selectedEntry != nullptr && selectedEntry->type == ThingDetailEntry::Type::Action && selectedEntry->index == index;
            browserRows.push_back(
                renderTwoColumnRow(label, ftxui::text(std::to_string(actionType.paramTypes.size()) + " param(s)"), isSelected, m_focusArea == FocusArea::ThingDetails));
        }
    }

    auto detailBrowser = renderFocusedWindow(ftxui::text("Values"),
                                             ftxui::vbox(std::move(browserRows)) | ftxui::vscroll_indicator | ftxui::frame,
                                             m_focusArea == FocusArea::ThingDetails)
                         | ftxui::flex;

    ftxui::Element inspector = ftxui::text("");
    if (m_showThingDetailInspector && selectedEntry != nullptr) {
        ftxui::Elements lines;
        if (selectedEntry->type == ThingDetailEntry::Type::Param) {
            const api::Param& param = thing->params.at(selectedEntry->index);
            const api::ParamType* paramType = m_thingManager.paramTypeForThing(*thing, param);
            lines.push_back(ftxui::text("Kind: Param"));
            if (paramType != nullptr) {
                lines.push_back(ftxui::text("Name: " + paramType->name.toStdString()));
                lines.push_back(ftxui::text("Display name: " + paramType->displayName.toStdString()));
                lines.push_back(ftxui::text("Data type: " + api::toString(paramType->type).toStdString()));
                lines.push_back(ftxui::text("Param type id: " + uuidToStd(paramType->id)));
                lines.push_back(ftxui::text("Unit: " + firstNonEmpty({paramType->unit.has_value() ? prettyUnit(*paramType->unit) : std::string(), "n/a"})));
                lines.push_back(ftxui::text("Input type: " + (paramType->inputType.has_value() ? api::toString(*paramType->inputType).toStdString() : std::string("n/a"))));
                lines.push_back(ftxui::text("Read only: " + firstNonEmpty({optionalBoolToString(paramType->readOnly), "n/a"})));
                lines.push_back(ftxui::text("Default value: " + optionalJsonValueToString(paramType->defaultValue)));
                lines.push_back(ftxui::text("Min value: " + optionalJsonValueToString(paramType->minValue)));
                lines.push_back(ftxui::text("Max value: " + optionalJsonValueToString(paramType->maxValue)));
                lines.push_back(ftxui::text("Step size: " + firstNonEmpty({optionalDoubleToString(paramType->stepSize), "n/a"})));
                lines.push_back(ftxui::paragraph("Allowed values: " + firstNonEmpty({optionalJsonValuesToString(paramType->allowedValues), "n/a"})));
            } else if (param.paramTypeId.has_value()) {
                lines.push_back(ftxui::text("Param type id: " + uuidToStd(*param.paramTypeId)));
            }
            lines.push_back(ftxui::separator());
            lines.push_back(ftxui::text("Current value:"));
            lines.push_back(renderValueCell(param.value,
                                            paramType != nullptr ? std::optional<api::BasicType>(paramType->type) : std::nullopt,
                                            paramType != nullptr ? paramType->unit : std::nullopt));
        } else if (selectedEntry->type == ThingDetailEntry::Type::State) {
            const api::State& state = thing->states.at(selectedEntry->index);
            const api::StateType* stateType = m_thingManager.stateTypeForThing(*thing, state);
            lines.push_back(ftxui::text("Kind: State"));
            if (stateType != nullptr) {
                lines.push_back(ftxui::text("Name: " + stateType->name.toStdString()));
                lines.push_back(ftxui::text("Display name: " + stateType->displayName.toStdString()));
                lines.push_back(ftxui::text("Data type: " + api::toString(stateType->type).toStdString()));
                lines.push_back(ftxui::text("State type id: " + uuidToStd(stateType->id)));
                lines.push_back(ftxui::text("Unit: " + firstNonEmpty({stateType->unit.has_value() ? prettyUnit(*stateType->unit) : std::string(), "n/a"})));
                lines.push_back(ftxui::text("IO type: " + (stateType->ioType.has_value() ? api::toString(*stateType->ioType).toStdString() : std::string("n/a"))));
                lines.push_back(ftxui::text("Default value: " + jsonValueToString(stateType->defaultValue)));
                lines.push_back(ftxui::text("Min value: " + optionalJsonValueToString(stateType->minValue)));
                lines.push_back(ftxui::text("Max value: " + optionalJsonValueToString(stateType->maxValue)));
                lines.push_back(ftxui::text("Step size: " + firstNonEmpty({optionalDoubleToString(stateType->stepSize), "n/a"})));
                lines.push_back(ftxui::paragraph("Possible values: " + firstNonEmpty({optionalJsonValuesToString(stateType->possibleValues), "n/a"})));
                lines.push_back(ftxui::paragraph("Possible value names: " + firstNonEmpty({optionalStringListToString(stateType->possibleValuesDisplayNames), "n/a"})));
            }
            lines.push_back(ftxui::text("Filter: " + api::toString(state.filter).toStdString()));
            lines.push_back(ftxui::text("Runtime min: " + firstNonEmpty({optionalJsonValueToString(state.minValue), "n/a"})));
            lines.push_back(ftxui::text("Runtime max: " + firstNonEmpty({optionalJsonValueToString(state.maxValue), "n/a"})));
            lines.push_back(ftxui::paragraph("Runtime values: " + firstNonEmpty({optionalJsonValuesToString(state.possibleValues), "n/a"})));
            lines.push_back(ftxui::separator());
            lines.push_back(ftxui::text("Current value:"));
            lines.push_back(renderValueCell(state.value,
                                            stateType != nullptr ? std::optional<api::BasicType>(stateType->type) : std::nullopt,
                                            stateType != nullptr ? stateType->unit : std::nullopt));
        } else if (selectedEntry->type == ThingDetailEntry::Type::Action) {
            const api::ActionType* actionType = thingClass != nullptr ? m_thingManager.actionTypeForThing(*thing, selectedEntry->index) : nullptr;
            lines.push_back(ftxui::text("Kind: Action"));
            if (actionType != nullptr) {
                lines.push_back(ftxui::text("Name: " + actionType->name.toStdString()));
                lines.push_back(ftxui::text("Display name: " + actionType->displayName.toStdString()));
                lines.push_back(ftxui::text("Action type id: " + uuidToStd(actionType->id)));
                lines.push_back(ftxui::text("Param count: " + std::to_string(actionType->paramTypes.size())));
                lines.push_back(ftxui::separator());
                for (int index = 0; index < actionType->paramTypes.size(); ++index) {
                    const api::ParamType* paramType = m_thingManager.paramTypeForAction(*actionType, index);
                    if (paramType == nullptr) {
                        continue;
                    }
                    lines.push_back(ftxui::text(firstNonEmpty({paramType->displayName.toStdString(), paramType->name.toStdString(), "<param>"}) + ": "
                                                + api::toString(paramType->type).toStdString()));
                    lines.push_back(ftxui::text("  id: " + uuidToStd(paramType->id)) | ftxui::dim);
                    std::vector<std::string> paramFields;
                    appendField(paramFields, "unit", paramType->unit.has_value() ? prettyUnit(*paramType->unit) : std::string());
                    appendField(paramFields, "input", paramType->inputType.has_value() ? api::toString(*paramType->inputType).toStdString() : std::string());
                    appendField(paramFields, "default", optionalJsonValueToString(paramType->defaultValue));
                    appendField(paramFields, "min", optionalJsonValueToString(paramType->minValue));
                    appendField(paramFields, "max", optionalJsonValueToString(paramType->maxValue));
                    appendField(paramFields, "allowed", optionalJsonValuesToString(paramType->allowedValues));
                    if (!paramFields.empty()) {
                        lines.push_back(ftxui::paragraph("  " + joinFields(paramFields)) | ftxui::dim);
                    }
                }
            }
            lines.push_back(ftxui::separator());
            lines.push_back(ftxui::text("Enter opens execution dialog") | ftxui::dim);
        }
        lines.push_back(ftxui::separator());
        lines.push_back(ftxui::text("Space closes inspector") | ftxui::dim);
        inspector = ftxui::window(ftxui::text("Inspector"), ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 42);
    }

    ftxui::Elements detailContent;
    detailContent.push_back(ftxui::window(ftxui::text("Overview"), ftxui::vbox(std::move(metadata))));
    if (m_showThingDetailInspector && selectedEntry != nullptr) {
        detailContent.push_back(ftxui::hbox({
                                    detailBrowser,
                                    inspector,
                                })
                                | ftxui::flex);
    } else {
        detailContent.push_back(detailBrowser);
    }

    return ftxui::vbox(std::move(detailContent)) | ftxui::reflect(m_thingDetailsBox) | ftxui::flex;
}

ftxui::Element Engine::renderConfigureMenu() const
{
    constexpr std::array<const char*, 4> menuItems = {"Add thing", "Remove thing", "Reconfigure thing", "Rename thing"};

    ftxui::Elements entries;
    for (int index = 0; index < static_cast<int>(menuItems.size()); ++index) {
        const bool selected = (m_configureThingsView == ConfigureThingsView::AddThing && index == 0) || (m_configureThingsView == ConfigureThingsView::RemoveThing && index == 1)
                              || (m_configureThingsView == ConfigureThingsView::ReconfigureThing && index == 2)
                              || (m_configureThingsView == ConfigureThingsView::RenameThing && index == 3);
        auto entry = ftxui::text(std::string(" ") + menuItems.at(index) + " ");
        if (selected) {
            entry = entry | ftxui::bold | ftxui::inverted;
        }
        if (m_focusArea == FocusArea::ConfigureMenu && selected) {
            entry = entry | ftxui::color(ftxui::Color::CyanLight);
        }
        if (selected) {
            entry = entry | ftxui::focus;
        }
        entries.push_back(entry);
    }

    return renderFocusedWindow(ftxui::text("Configure"), ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::frame, m_focusArea == FocusArea::ConfigureMenu)
           | ftxui::reflect(m_configureMenuBox);
}

ftxui::Element Engine::renderConfigureDetails() const
{
    if (m_configureThingsView == ConfigureThingsView::AddThing) {
        ftxui::Elements content;

        auto search = ftxui::text("Search: " + (m_configureThingSearch.empty() ? std::string("<type to filter>") : m_configureThingSearch));
        if (m_focusArea == FocusArea::ConfigureThingClassSearch) {
            search = renderActiveField(std::move(search) | ftxui::inverted | ftxui::bold | ftxui::color(ftxui::Color::CyanLight), true, 28);
        }
        content.push_back(search);
        content.push_back(ftxui::separator());

        if (m_fetchAllThingClassesPending && !m_haveAllThingClasses) {
            content.push_back(ftxui::text("Loading thing classes..."));
        } else {
            const std::vector<api::ThingClass> thingClasses = filteredConfigThingClasses();
            if (thingClasses.empty()) {
                content.push_back(ftxui::text("No addable thing classes match the current filter."));
            } else {
                for (int index = 0; index < static_cast<int>(thingClasses.size()); ++index) {
                    const api::ThingClass& thingClass = thingClasses.at(index);
                    auto row = ftxui::text(" " + thingClassLabel(thingClass) + " ");
                    if (index == m_selectedConfigureThingClassIndex) {
                        row = row | ftxui::bold | ftxui::inverted;
                    }
                    if (m_focusArea == FocusArea::ConfigureThingClassList && index == m_selectedConfigureThingClassIndex) {
                        row = row | ftxui::color(ftxui::Color::CyanLight);
                    }
                    if (index == m_selectedConfigureThingClassIndex) {
                        row = row | ftxui::focus;
                    }
                    content.push_back(row);
                }
            }

            if (const api::ThingClass* thingClass = selectedConfigThingClass(); thingClass != nullptr) {
                content.push_back(ftxui::separator());
                content.push_back(ftxui::text("Selected class") | ftxui::bold);
                content.push_back(ftxui::text("Setup: " + setupMethodLabel(thingClass->setupMethod)));

                std::vector<std::string> createMethods;
                for (const api::CreateMethod method : thingClass->createMethods) {
                    createMethods.push_back(createMethodLabel(method));
                }
                content.push_back(ftxui::text("Create methods: " + joinCommaSeparated(createMethods)));
                content.push_back(ftxui::text("Thing params: " + std::to_string(thingClass->paramTypes.size())));
                content.push_back(ftxui::text("Discovery params: " + std::to_string(thingClass->discoveryParamTypes.size())));
                if (!thingClass->interfaces.isEmpty()) {
                    content.push_back(ftxui::paragraph("Interfaces: " + thingClass->interfaces.join(QStringLiteral(", ")).toStdString()));
                }
            }
        }

        content.push_back(ftxui::separator());
        content.push_back(ftxui::text("Enter opens the setup flow.") | ftxui::dim);
        return renderFocusedWindow(ftxui::text("Add thing"),
                                   ftxui::vbox(std::move(content)) | ftxui::vscroll_indicator | ftxui::frame,
                                   m_focusArea == FocusArea::ConfigureThingClassSearch || m_focusArea == FocusArea::ConfigureThingClassList);
    }

    ftxui::Elements content;
    if (m_thingManager.things().empty()) {
        content.push_back(ftxui::text("No configured things available."));
    } else {
        for (int index = 0; index < static_cast<int>(m_thingManager.things().size()); ++index) {
            const api::Thing& thing = m_thingManager.things().at(index);
            auto row = ftxui::text(" " + thingLabel(&thing) + " ");
            if (index == m_selectedConfigureThingIndex) {
                row = row | ftxui::bold | ftxui::inverted;
            }
            if (m_focusArea == FocusArea::ConfigureThingSelection && index == m_selectedConfigureThingIndex) {
                row = row | ftxui::color(ftxui::Color::CyanLight);
            }
            if (index == m_selectedConfigureThingIndex) {
                row = row | ftxui::focus;
            }
            content.push_back(row);
        }
    }

    content.push_back(ftxui::separator());
    switch (m_configureThingsView) {
    case ConfigureThingsView::RemoveThing:
        content.push_back(ftxui::text("Enter opens a remove confirmation.") | ftxui::dim);
        break;
    case ConfigureThingsView::ReconfigureThing:
        content.push_back(ftxui::text("Enter shows the current reconfigure status.") | ftxui::dim);
        break;
    case ConfigureThingsView::RenameThing:
        content.push_back(ftxui::text("Enter opens the rename dialog.") | ftxui::dim);
        break;
    case ConfigureThingsView::AddThing:
        break;
    }

    const char* title = m_configureThingsView == ConfigureThingsView::RemoveThing
                            ? "Remove thing"
                            : (m_configureThingsView == ConfigureThingsView::ReconfigureThing ? "Reconfigure thing" : "Rename thing");
    return renderFocusedWindow(ftxui::text(title), ftxui::vbox(std::move(content)) | ftxui::vscroll_indicator | ftxui::frame, m_focusArea == FocusArea::ConfigureThingSelection)
           | ftxui::reflect(m_configureDetailsBox);
}

ftxui::Element Engine::renderSettingsMenu() const
{
    constexpr std::array<const char*, 6> menuItems = {"Server info", "Timezone", "Update", "Shutdown", "Restart", "Reboot"};

    ftxui::Elements entries;
    for (int index = 0; index < static_cast<int>(menuItems.size()); ++index) {
        const bool selected = static_cast<int>(m_settingsView) == index;
        auto entry = ftxui::text(std::string(" ") + menuItems.at(index) + " ");
        if (selected) {
            entry = entry | ftxui::bold | ftxui::inverted;
        }
        if (m_focusArea == FocusArea::SettingsMenu && selected) {
            entry = entry | ftxui::color(ftxui::Color::CyanLight);
        } else if (m_focusArea == FocusArea::SettingsDetails && selected) {
            entry = entry | ftxui::color(ftxui::Color::CyanLight);
        }
        if (selected) {
            entry = entry | ftxui::focus;
        }
        entries.push_back(entry);
    }

    return renderFocusedWindow(ftxui::text("Settings"), ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::frame, m_focusArea == FocusArea::SettingsMenu)
           | ftxui::reflect(m_settingsMenuBox);
}

ftxui::Element Engine::renderSettingsDetails() const
{
    ftxui::Elements lines;
    int lineIndex = 0;
    auto pushLine = [&](ftxui::Element line) {
        if (m_focusArea == FocusArea::SettingsDetails && lineIndex == m_settingsDetailsLineIndex) {
            line = line | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight) | ftxui::focus;
        }
        lines.push_back(std::move(line));
        ++lineIndex;
    };
    auto pushSelectableLine = [&](ftxui::Element line, int minimumWidth = 0) {
        const bool selected = m_focusArea == FocusArea::SettingsDetails && lineIndex == m_settingsDetailsLineIndex;
        pushLine(renderActiveField(std::move(line), selected, minimumWidth));
    };

    if (m_settingsView == SettingsView::ServerInfo) {
        const QString fingerprint = m_client.peerCertificateFingerprint();
        pushLine(ftxui::text("Connection: " + endpoint()));
        pushLine(ftxui::text("Display name: " + connectionDisplayName()));
        pushLine(ftxui::text("Settings path: " + m_connectionSettings.settingsPath().toStdString()));
        pushLine(ftxui::text("Transport: " + std::string(m_options.useSsl ? "SSL/TLS" : "Plain TCP")));
        pushLine(ftxui::text("Server uuid: " + m_serverUuid.toString(QUuid::WithoutBraces).toStdString()));
        pushLine(ftxui::text("Authentication: " + m_authStatus));
        pushLine(ftxui::text("Stored token: " + std::string(m_savedConnection.has_value() && !m_savedConnection->token.isEmpty() ? "available" : "none")));
        pushLine(ftxui::text("TLS fingerprint: " + (fingerprint.isEmpty() ? std::string("n/a") : fingerprint.toStdString())));
    } else if (m_settingsView == SettingsView::Timezone) {
        const QStringList filteredTimeZones = filteredSystemTimeZones();
        pushLine(ftxui::text("Current time zone: " + (m_systemTimeLoaded ? m_systemTime.timeZone.toStdString() : std::string("<loading>"))));
        pushLine(ftxui::text("Automatic time: " + (m_systemTimeLoaded ? std::string(m_systemTime.automaticTime ? "enabled" : "disabled") : std::string("n/a"))));
        pushLine(ftxui::text("Automatic time available: " + (m_systemTimeLoaded ? std::string(m_systemTime.automaticTimeAvailable ? "yes" : "no") : std::string("n/a"))));
        pushLine(ftxui::separator());
        auto search = ftxui::text("Search: " + (m_systemTimeZoneSearch.empty() ? std::string("<type to filter>") : m_systemTimeZoneSearch));
        if (m_focusArea == FocusArea::SettingsDetails && m_settingsDetailsLineIndex == timezoneSearchLineIndex) {
            search = renderActiveField(std::move(search) | ftxui::inverted | ftxui::bold | ftxui::color(ftxui::Color::CyanLight), true, 32);
        }
        pushLine(std::move(search));
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Available time zones") | ftxui::bold);
        if (!m_systemTimeZonesLoaded) {
            pushLine(ftxui::text("Loading time zones..."));
        } else {
            if (filteredTimeZones.isEmpty()) {
                pushLine(ftxui::text("No time zones match the current filter."));
            } else {
                for (const QString& timeZone : filteredTimeZones) {
                    pushSelectableLine(ftxui::text(" " + timeZone.toStdString() + " "), 24);
                }
            }
        }
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Type to search, Up/Down move, Enter applies the selected time zone.") | ftxui::dim);
    } else if (m_settingsView == SettingsView::Update) {
        const std::string capabilityText = m_systemCapabilitiesLoaded ? std::string("Update management: ") + (m_systemCapabilities.updateManagement ? "available" : "unavailable")
                                                                            + " (" + api::toString(m_systemCapabilities.updateManagementType).toStdString() + ")"
                                                                      : std::string("Update management: loading...");
        pushLine(ftxui::text(capabilityText));
        if (m_systemUpdateStatusLoaded) {
            std::string statusText = "Busy: ";
            statusText += m_systemUpdateStatus.busy ? "yes" : "no";
            statusText += " | Update running: ";
            statusText += m_systemUpdateStatus.updateRunning ? "yes" : "no";
            if (m_systemUpdateStatus.updateProgress.has_value()) {
                statusText += " | Progress: " + std::to_string(*m_systemUpdateStatus.updateProgress) + "%";
            }
            pushLine(ftxui::text("Status: " + statusText));
        } else {
            pushLine(ftxui::text("Status: loading..."));
        }
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Actions") | ftxui::bold);
        pushSelectableLine(ftxui::text(" Check for updates "), 24);
        const bool hasInstallablePackages = std::any_of(m_systemPackages.begin(), m_systemPackages.end(), [](const api::Package& package) {
            return package.updateAvailable || package.installedVersion.isEmpty();
        });
        auto installRow = ftxui::text(hasInstallablePackages ? " Install available updates " : " Install available updates (none) ");
        if (!hasInstallablePackages) {
            installRow = installRow | ftxui::dim;
        }
        pushSelectableLine(std::move(installRow), 28);
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Packages") | ftxui::bold);
        if (!m_systemPackagesLoaded) {
            pushLine(ftxui::text("Loading packages..."));
        } else if (m_systemPackages.empty()) {
            pushLine(ftxui::text("No packages returned."));
        } else {
            for (const api::Package& package : m_systemPackages) {
                std::string label = package.displayName.toStdString();
                if (!package.installedVersion.isEmpty() || !package.candidateVersion.isEmpty()) {
                    label += " (" + package.installedVersion.toStdString();
                    if (!package.candidateVersion.isEmpty()) {
                        label += " -> " + package.candidateVersion.toStdString();
                    }
                    label += ")";
                }
                if (package.updateAvailable) {
                    label += " [update available]";
                }
                if (!package.summary.isEmpty()) {
                    label += " - " + package.summary.toStdString();
                }
                pushLine(ftxui::paragraph(label));
            }
        }
        pushLine(ftxui::separator());
        pushLine(ftxui::text(m_systemUpdateStatusPending ? "Loading update status..." : "Enter checks updates or installs all available updates.") | ftxui::dim);
    } else {
        pushLine(ftxui::text("Warning") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
        pushLine(ftxui::separator());
        const std::string action = powerActionLabel(static_cast<int>(m_systemAction));
        pushLine(ftxui::paragraph("This will request a " + action + " on the server."));
        pushLine(ftxui::paragraph("Enter opens the confirmation dialog, and Left or Esc returns to the settings menu."));
        if (!m_systemActionStatus.empty()) {
            pushLine(ftxui::separator());
            pushLine(ftxui::text(m_systemActionStatus));
        }
    }

    return renderFocusedWindow(ftxui::text(settingsViewLabel(static_cast<int>(m_settingsView))),
                               ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame,
                               m_focusArea == FocusArea::SettingsDetails)
           | ftxui::reflect(m_settingsDetailsBox);
}

ftxui::Element Engine::renderLogout() const
{
    ftxui::Elements lines;
    lines.push_back(ftxui::text("Logout"));
    lines.push_back(ftxui::separator());
    lines.push_back(ftxui::paragraph("Logout revokes the current token on the server, clears the saved token locally, and reconnects to the same server."));
    lines.push_back(ftxui::separator());
    lines.push_back(ftxui::text("Only execution option: Logout") | ftxui::bold);
    lines.push_back(ftxui::text("Press Enter to logout, or Left to return to the menu.") | ftxui::dim);

    return renderFocusedWindow(ftxui::text("Logout"), ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame, m_mainView == MainView::Logout) | ftxui::flex;
}

int Engine::settingsDetailsLineCount() const
{
    switch (m_settingsView) {
    case SettingsView::ServerInfo:
        return 8;
    case SettingsView::Timezone:
        return m_systemTimeZonesLoaded ? 9 + std::max(1, static_cast<int>(filteredSystemTimeZones().size())) : 10;
    case SettingsView::Update:
        return m_systemPackagesLoaded ? 11 + static_cast<int>(m_systemPackages.size()) : 11;
    case SettingsView::Shutdown:
    case SettingsView::Restart:
    case SettingsView::Reboot:
        return 0;
    }
    return 0;
}

void Engine::clampSettingsDetailsSelection()
{
    const int lineCount = settingsDetailsLineCount();
    if (lineCount <= 0) {
        m_settingsDetailsLineIndex = 0;
        return;
    }

    if (m_settingsView == SettingsView::Timezone) {
        const int filteredCount = m_systemTimeZonesLoaded ? static_cast<int>(filteredSystemTimeZones().size()) : 0;
        const int firstResultLineIndex = timezoneListStartLineIndex;
        if (filteredCount <= 0) {
            m_settingsDetailsLineIndex = timezoneSearchLineIndex;
            return;
        }

        const int lastResultLineIndex = firstResultLineIndex + filteredCount - 1;
        if (m_settingsDetailsLineIndex == timezoneSearchLineIndex) {
            return;
        }
        if (m_settingsDetailsLineIndex < firstResultLineIndex) {
            m_settingsDetailsLineIndex = timezoneSearchLineIndex;
            return;
        }
        if (m_settingsDetailsLineIndex > lastResultLineIndex) {
            m_settingsDetailsLineIndex = lastResultLineIndex;
            return;
        }
    }

    if (m_settingsDetailsLineIndex < 0) {
        m_settingsDetailsLineIndex = 0;
    } else if (m_settingsDetailsLineIndex >= lineCount) {
        m_settingsDetailsLineIndex = lineCount - 1;
    }
}

ftxui::Element Engine::renderAbout() const
{
    ftxui::Elements lines;
    lines.push_back(ftxui::text("nymea-cli"));
    lines.push_back(ftxui::separator());
    lines.push_back(ftxui::text("Application version: " + m_options.appVersion));
    lines.push_back(ftxui::text("Project license: " APP_LICENSE_SPDX));
    lines.push_back(ftxui::text("Server version: " + m_serverVersion));
    lines.push_back(ftxui::text("Server API version: " + m_serverApiVersion));
    lines.push_back(ftxui::text("Purpose: terminal client for nymead"));
    lines.push_back(ftxui::text("Navigation: Up/Down move, Left/Right switch panels"));
    lines.push_back(ftxui::separator());
    lines.push_back(ftxui::text("Open source components"));
    lines.push_back(ftxui::text("Qt Core + Network version: " + std::string(qVersion())));
    lines.push_back(ftxui::paragraph("Qt licensing reference: https://www.qt.io/licensing/"));
    lines.push_back(ftxui::paragraph("Qt project reference: https://www.qt.io/"));
    lines.push_back(ftxui::text("FTXUI version: " FTXUI_VERSION));
    lines.push_back(ftxui::text("FTXUI license: MIT"));
    lines.push_back(ftxui::paragraph("FTXUI project reference: https://github.com/ArthurSonzogni/FTXUI"));
    lines.push_back(ftxui::paragraph("FTXUI license reference: https://github.com/ArthurSonzogni/FTXUI/blob/" FTXUI_VERSION "/LICENSE"));

    return renderFocusedWindow(ftxui::text("About"), ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame, m_mainView == MainView::About) | ftxui::flex;
}

ftxui::Element Engine::renderConnectionLost() const
{
    return ftxui::vbox({
               ftxui::filler(),
               ftxui::hbox({
                   ftxui::filler(),
                   ftxui::window(ftxui::text("Connection lost"),
                                 ftxui::vbox({
                                     ftxui::text("Connection lost, try to reconnect."),
                                     ftxui::separator(),
                                     ftxui::text("Press c to reconnect, q or Esc to quit.") | ftxui::dim,
                                 }))
                       | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 44),
                   ftxui::filler(),
               }),
               ftxui::filler(),
           })
           | ftxui::flex;
}

ftxui::Element Engine::renderThings() const
{
    return ftxui::hbox({
               renderThingList() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 36),
               renderThingDetails() | ftxui::flex,
           })
           | ftxui::flex;
}

ftxui::Element Engine::renderUi()
{
    drainUiTasks();
    if (m_connectionLost) {
        return renderConnectionLost();
    }

    if (m_mainView == MainView::Things) {
        clampThingDetailSelection();
    } else if (m_mainView == MainView::ConfigureThings) {
        clampConfigureThingClassSelection();
        clampConfigureThingSelection();
    } else if (m_mainView == MainView::Settings) {
        clampSettingsDetailsSelection();
    } else if (m_mainView == MainView::ApiBrowser) {
        clampApiBrowserSelection();
        clampApiBrowserReferenceSelection();
        clampApiBrowserJsonSelection();
    } else if (m_mainView == MainView::Help) {
        clampHelpSelection();
    }
    if (m_actionExecutionPending || m_configureRequestPending) {
        if (ftxui::ScreenInteractive* screen = ftxui::ScreenInteractive::Active(); screen != nullptr) {
            screen->RequestAnimationFrame();
        }
    }

    if (m_showLoginForm) {
        ftxui::Elements sections;
        sections.push_back(ftxui::hbox({
                               ftxui::text(" nymea-cli (" + m_options.appVersion + ")"),
                               ftxui::filler(),
                               ftxui::text(endpoint() + " " + m_serverName.toStdString() + " | " + m_serverVersion + " | API " + m_serverApiVersion),
                           })
                           | ftxui::border);
        sections.push_back(ftxui::filler());
        ftxui::Elements loginBody;
        if (m_pushButtonAuthAvailable) {
            loginBody.push_back(ftxui::text("Please press the push button for authentication.") | ftxui::bold);
            loginBody.push_back(ftxui::separator());
            if (!m_authStatus.empty()) {
                loginBody.push_back(ftxui::text(m_authStatus));
            }
            if (m_pushButtonAuthPending) {
                loginBody.push_back(ftxui::text("Waiting for push-button authentication to finish...") | ftxui::dim);
            }
            loginBody.push_back(ftxui::text("Enter requests push-button authentication again, Esc quits.") | ftxui::dim);
        } else {
            loginBody.push_back(ftxui::hbox({
                ftxui::text("Username: "),
                renderActiveField(m_usernameInput->Render() | ftxui::xflex, m_loginSelectedInputIndex == 0, 26),
            }));
            loginBody.push_back(ftxui::hbox({
                ftxui::text("Password: "),
                renderActiveField(m_passwordInput->Render() | ftxui::xflex, m_loginSelectedInputIndex == 1, 26),
            }));
            loginBody.push_back(ftxui::separator());
            if (!m_authStatus.empty()) {
                loginBody.push_back(ftxui::text(m_authStatus));
            }
            if (!m_logoutStatus.empty()) {
                loginBody.push_back(ftxui::paragraph(m_logoutStatus) | ftxui::dim);
            }
            loginBody.push_back(ftxui::text("Enter submits credentials, Esc quits.") | ftxui::dim);
        }
        sections.push_back(ftxui::hbox({
            ftxui::filler(),
            renderFocusedWindow(ftxui::text("Authentication required"), ftxui::vbox(std::move(loginBody)) | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 56), m_showLoginForm),
            ftxui::filler(),
        }));
        sections.push_back(ftxui::filler());
        return ftxui::vbox(std::move(sections)) | ftxui::flex;
    }

    ftxui::Elements sections;
    if (!m_securityWarning.empty()) {
        sections.push_back(ftxui::window(ftxui::text("INSECURE WARNING"),
                                         ftxui::vbox({
                                             ftxui::text("TLS certificate fingerprint changed") | ftxui::bold | ftxui::color(ftxui::Color::Red),
                                             ftxui::separator(),
                                             ftxui::paragraph(m_securityWarning),
                                         }))
                           | ftxui::color(ftxui::Color::Red));
    }

    sections.push_back(ftxui::hbox({
                           ftxui::text(" nymea-cli (" + m_options.appVersion + ")"),
                           ftxui::filler(),
                           ftxui::text(endpoint() + " " + m_serverName.toStdString() + " | " + m_serverVersion + " | API " + m_serverApiVersion),
                       })
                       | ftxui::border);
    if (!m_settingsWarning.empty()) {
        sections.push_back(ftxui::text(m_settingsWarning) | ftxui::color(ftxui::Color::Yellow));
    }
    if (m_mainView == MainView::Help) {
        sections.push_back(renderHelp() | ftxui::flex);
        return ftxui::vbox(std::move(sections)) | ftxui::border | ftxui::flex;
    }
    std::string keyHintLine
        = "Keys: Up/Down navigate, Left/Right switch panels, s sort, f filter, Space inspector, c reconnect, t refresh things, ?/h help, Enter opens actions/setup, q/Esc quit";
    if (m_mainView == MainView::ApiBrowser) {
        keyHintLine
            = "Keys: Up/Down navigate, Left back, Right switch browser panes, Enter follows a reference, type to filter, c reconnect, t refresh things, ?/h help, q/Esc quit";
    } else if (m_mainView == MainView::Settings) {
        keyHintLine = "Keys: Up/Down select settings sections, Right/Enter open the details panel, Enter applies the selected action or time zone, type to search time zones, Left "
                      "returns to the menu, ?/h help, q/Esc quit";
    } else if (m_mainView == MainView::Logout) {
        keyHintLine = "Keys: Enter logs out, Left returns to the menu, ?/h help, q/Esc quit";
    } else if (m_mainView == MainView::About) {
        keyHintLine = "Keys: Left/Right switch panels, ?/h help, q/Esc quit";
    }
    sections.push_back(ftxui::text(keyHintLine) | ftxui::dim);
    sections.push_back(ftxui::separator());

    ftxui::Element rightPanel;
    if (m_mainView == MainView::Things) {
        rightPanel = renderThings() | ftxui::flex;
    } else if (m_mainView == MainView::ConfigureThings) {
        rightPanel = ftxui::hbox({
                         renderConfigureMenu() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 28),
                         renderConfigureDetails() | ftxui::flex,
                     })
                     | ftxui::flex;
    } else if (m_mainView == MainView::ApiBrowser) {
        rightPanel = renderApiBrowser() | ftxui::flex;
    } else if (m_mainView == MainView::Logout) {
        rightPanel = renderLogout() | ftxui::flex;
    } else if (m_mainView == MainView::About) {
        rightPanel = renderAbout() | ftxui::flex;
    } else {
        rightPanel = ftxui::hbox({
                         renderSettingsMenu() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 28),
                         renderSettingsDetails() | ftxui::flex,
                     })
                     | ftxui::flex;
    }

    sections.push_back(ftxui::hbox({
                           renderMainMenu() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 24),
                           rightPanel | ftxui::flex,
                       })
                       | ftxui::flex);

    if (m_showLogoutConfirm) {
        ftxui::Elements dialogBody;
        dialogBody.push_back(ftxui::text("Warning") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
        dialogBody.push_back(ftxui::separator());
        dialogBody.push_back(ftxui::paragraph("This will revoke the current token on the server, forget the saved token locally, and reconnect to the same endpoint."));
        if (!m_logoutStatus.empty()) {
            dialogBody.push_back(ftxui::separator());
            dialogBody.push_back(ftxui::text(m_logoutStatus));
            dialogBody.push_back(ftxui::text("You will be returned to the login form if authentication is required.") | ftxui::dim);
        }
        dialogBody.push_back(ftxui::separator());
        dialogBody.push_back(ftxui::text(m_logoutRequestPending ? "Revoking token and reconnecting..." : "Enter confirms logout, Esc cancels.") | ftxui::dim);
        sections.push_back(ftxui::separator());
        sections.push_back(renderFocusedWindow(ftxui::text("Confirm logout"), ftxui::vbox(std::move(dialogBody)), m_showLogoutConfirm));
    }

    if (m_showSystemActionConfirm) {
        ftxui::Elements dialogBody;
        dialogBody.push_back(ftxui::text("Warning") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
        dialogBody.push_back(ftxui::separator());
        dialogBody.push_back(ftxui::paragraph("This will request a " + powerActionLabel(static_cast<int>(m_systemAction)) + " on the server."));
        if (!m_systemActionStatus.empty()) {
            dialogBody.push_back(ftxui::separator());
            dialogBody.push_back(ftxui::text(m_systemActionStatus));
        }
        dialogBody.push_back(ftxui::separator());
        dialogBody.push_back(ftxui::text(m_systemActionRequestPending ? "Sending request..." : "Enter confirms, Esc or Left cancels.") | ftxui::dim);
        sections.push_back(ftxui::separator());
        sections.push_back(
            renderFocusedWindow(ftxui::text(powerActionLabel(static_cast<int>(m_systemAction)) + " confirmation"), ftxui::vbox(std::move(dialogBody)), m_showSystemActionConfirm));
    }

    if (m_showActionDialog) {
        ftxui::Elements dialogBody;
        dialogBody.push_back(ftxui::text("Action: " + m_actionDialogActionName));
        dialogBody.push_back(ftxui::separator());

        if (m_actionDialogParamTypes.empty()) {
            dialogBody.push_back(ftxui::text("This action has no params."));
        } else {
            for (int index = 0; index < static_cast<int>(m_actionDialogParamTypes.size()); ++index) {
                const api::ParamType& paramType = m_actionDialogParamTypes.at(index);
                const std::string label = firstNonEmpty({paramType.displayName.toStdString(), paramType.name.toStdString(), "<param>"});
                dialogBody.push_back(renderTwoColumnRow(label,
                                                        renderActionDialogValueCell(paramType, m_actionDialogParamValues.at(index)),
                                                        index == m_actionDialogSelectedParamIndex,
                                                        m_focusArea == FocusArea::ActionDialog));
            }
        }

        ftxui::Elements dialogFooter;
        dialogFooter.push_back(ftxui::text(m_actionDialogStatus));
        if (!m_actionDialogParamTypes.empty() && m_actionDialogSelectedParamIndex >= 0 && m_actionDialogSelectedParamIndex < static_cast<int>(m_actionDialogParamTypes.size())) {
            const api::ParamType& selectedParamType = m_actionDialogParamTypes.at(m_actionDialogSelectedParamIndex);
            if (actionParamUsesSelector(selectedParamType)) {
                dialogFooter.push_back(ftxui::text("Up/Down select param, Left/Right/Space choose value, Enter execute, Esc close.") | ftxui::dim);
            } else if (actionParamUsesRangeInput(selectedParamType)) {
                dialogFooter.push_back(ftxui::text(rangeInputSummary(selectedParamType)));
                if (const std::optional<NumericRangeInputSpec> rangeSpec = numericRangeInputSpec(selectedParamType);
                    rangeSpec.has_value()
                    && !::nymea::currentNumericRangeValueFromRaw(selectedParamType, *rangeSpec, m_actionDialogParamValues.at(m_actionDialogSelectedParamIndex)).has_value()
                    && !m_actionDialogParamValues.at(m_actionDialogSelectedParamIndex).empty()) {
                    dialogFooter.push_back(ftxui::text("Current value is incomplete or invalid. Type a number or use Left/Right to snap into range.")
                                           | ftxui::color(ftxui::Color::YellowLight));
                }
                dialogFooter.push_back(ftxui::text("Type a number, Left/Right adjust the bar, Up/Down select param, Enter execute, Esc close.") | ftxui::dim);
            } else {
                dialogFooter.push_back(ftxui::text("Type to edit, Up/Down select param, Enter execute, Esc close.") | ftxui::dim);
            }
        } else {
            dialogFooter.push_back(ftxui::text("Enter executes, Esc closes.") | ftxui::dim);
        }

        if (!m_lastActionExecutionStatus.empty()) {
            dialogFooter.push_back(ftxui::separator());
            dialogFooter.push_back(
                ftxui::hbox({
                    ftxui::text(" " + m_lastActionExecutionStatus),
                    ftxui::filler(),
                })
                | ftxui::bold | ftxui::color(ftxui::Color::Black)
                | ftxui::bgcolor(m_actionExecutionPending ? ftxui::Color::CyanLight : (m_lastActionExecutionStatusWarning ? ftxui::Color::YellowLight : ftxui::Color::GreenLight)));
        }

        sections.push_back(ftxui::separator());
        sections.push_back(renderFocusedWindow(ftxui::text("Execute action"),
                                               ftxui::vbox({
                                                   ftxui::vbox(std::move(dialogBody)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::flex,
                                                   ftxui::separator(),
                                                   ftxui::vbox(std::move(dialogFooter)),
                                               }) | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 80),
                                               m_showActionDialog && m_focusArea == FocusArea::ActionDialog));
    }

    if (m_showConfigureDialog) {
        ftxui::Elements dialogBody;
        if (const api::ThingClass* thingClass = m_configureThingClassId.isNull() ? nullptr : m_thingManager.thingClassById(m_configureThingClassId); thingClass != nullptr) {
            dialogBody.push_back(ftxui::text("Thing class: " + thingClassLabel(*thingClass)));
            dialogBody.push_back(ftxui::separator());
        }

        switch (m_configureDialogMode) {
        case ConfigureDialogMode::AddChooseCreateMethod:
            for (int index = 0; index < static_cast<int>(m_configureCreateMethodOptions.size()); ++index) {
                const bool selected = index == m_configureCreateMethodIndex;
                auto row = ftxui::text(" " + createMethodLabel(m_configureCreateMethodOptions.at(index)) + " ");
                if (selected) {
                    row = row | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight) | ftxui::focus;
                }
                dialogBody.push_back(row);
            }
            break;
        case ConfigureDialogMode::AddManualParams: {
            dialogBody.push_back(renderTwoColumnRow("Name",
                                                    ftxui::text(m_configureThingName.empty() ? std::string("<empty>") : m_configureThingName),
                                                    m_configureParamSelectionIndex == 0,
                                                    m_focusArea == FocusArea::ConfigureDialog));
            for (int index = 0; index < static_cast<int>(m_configureParamTypes.size()); ++index) {
                const api::ParamType& paramType = m_configureParamTypes.at(index);
                dialogBody.push_back(renderTwoColumnRow(firstNonEmpty({paramType.displayName.toStdString(), paramType.name.toStdString(), "<param>"}),
                                                        renderActionDialogValueCell(paramType, m_configureParamValues.at(index)),
                                                        m_configureParamSelectionIndex == index + 1,
                                                        m_focusArea == FocusArea::ConfigureDialog));
            }
            break;
        }
        case ConfigureDialogMode::AddDiscoveryParams:
            if (m_configureParamTypes.empty()) {
                dialogBody.push_back(ftxui::text("No discovery params required."));
            } else {
                for (int index = 0; index < static_cast<int>(m_configureParamTypes.size()); ++index) {
                    const api::ParamType& paramType = m_configureParamTypes.at(index);
                    dialogBody.push_back(renderTwoColumnRow(firstNonEmpty({paramType.displayName.toStdString(), paramType.name.toStdString(), "<param>"}),
                                                            renderActionDialogValueCell(paramType, m_configureParamValues.at(index)),
                                                            m_configureParamSelectionIndex == index,
                                                            m_focusArea == FocusArea::ConfigureDialog));
                }
            }
            break;
        case ConfigureDialogMode::AddDiscoveryResults:
            for (int index = 0; index < static_cast<int>(m_configureThingDescriptors.size()); ++index) {
                const api::ThingDescriptor& descriptor = m_configureThingDescriptors.at(index);
                auto row = ftxui::text(" " + descriptorLabel(descriptor) + " ");
                if (index == m_configureThingDescriptorIndex) {
                    row = row | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight) | ftxui::focus;
                }
                dialogBody.push_back(row);
                if (!descriptor.description.isEmpty()) {
                    dialogBody.push_back(ftxui::paragraph("  " + descriptor.description.toStdString()) | ftxui::dim);
                }
            }
            break;
        case ConfigureDialogMode::AddPairingConfirmation: {
            if (!m_configurePairingDisplayMessage.empty()) {
                dialogBody.push_back(ftxui::paragraph(m_configurePairingDisplayMessage));
                dialogBody.push_back(ftxui::separator());
            }
            if (!m_configurePairingPin.empty()) {
                dialogBody.push_back(ftxui::text("PIN: " + m_configurePairingPin));
            }
            if (!m_configurePairingOauthUrl.empty()) {
                dialogBody.push_back(ftxui::paragraph("OAuth URL: " + m_configurePairingOauthUrl));
            }

            int fieldIndex = 0;
            if (m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword) {
                dialogBody.push_back(renderTwoColumnRow("Username",
                                                        ftxui::text(m_configurePairingUsername.empty() ? std::string("<empty>") : m_configurePairingUsername),
                                                        m_configureParamSelectionIndex == fieldIndex++,
                                                        m_focusArea == FocusArea::ConfigureDialog));
            }
            if (m_configureSetupMethod == api::SetupMethod::SetupMethodDisplayPin || m_configureSetupMethod == api::SetupMethod::SetupMethodEnterPin
                || m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword || m_configureSetupMethod == api::SetupMethod::SetupMethodOAuth) {
                const std::string label = m_configureSetupMethod == api::SetupMethod::SetupMethodOAuth
                                              ? "Redirect URL"
                                              : (m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword ? "Password" : "Secret");
                dialogBody.push_back(renderTwoColumnRow(label,
                                                        ftxui::text(m_configurePairingSecret.empty() ? std::string("<empty>") : m_configurePairingSecret),
                                                        m_configureParamSelectionIndex == fieldIndex,
                                                        m_focusArea == FocusArea::ConfigureDialog));
            } else if (m_configureSetupMethod == api::SetupMethod::SetupMethodPushButton) {
                dialogBody.push_back(ftxui::text("No additional input required."));
            }
            break;
        }
        case ConfigureDialogMode::RemoveThingConfirm:
            dialogBody.push_back(ftxui::paragraph("Remove thing " + uuidToStd(m_configureTargetThingId) + "?"));
            break;
        case ConfigureDialogMode::RenameThing:
            dialogBody.push_back(renderTwoColumnRow("Name",
                                                    ftxui::text(m_configureThingName.empty() ? std::string("<empty>") : m_configureThingName),
                                                    true,
                                                    m_focusArea == FocusArea::ConfigureDialog));
            break;
        case ConfigureDialogMode::ReconfigureThingInfo:
            dialogBody.push_back(ftxui::paragraph(m_configureDialogStatus));
            break;
        case ConfigureDialogMode::None:
            break;
        }

        ftxui::Elements dialogFooter;
        const std::string status = m_configureRequestPending ? busyIndicator(m_configurePendingStartedAt) + " " + m_configureDialogStatus : m_configureDialogStatus;
        if (!status.empty()) {
            dialogFooter.push_back(ftxui::text(status));
        }
        if (m_configureFlowComplete) {
            dialogFooter.push_back(ftxui::text("Enter or Esc closes this wizard.") | ftxui::dim);
        } else {
            dialogFooter.push_back(ftxui::text(m_configureRequestPending ? "Waiting for server reply..." : "Enter confirms, Esc closes.") | ftxui::dim);
        }

        if (!m_lastConfigureExecutionStatus.empty()) {
            dialogFooter.push_back(ftxui::separator());
            dialogFooter.push_back(ftxui::hbox({
                                       ftxui::text(" " + m_lastConfigureExecutionStatus),
                                       ftxui::filler(),
                                   })
                                   | ftxui::bold | ftxui::color(ftxui::Color::Black)
                                   | ftxui::bgcolor(m_configureRequestPending ? ftxui::Color::CyanLight
                                                                              : (m_lastConfigureExecutionStatusWarning ? ftxui::Color::YellowLight : ftxui::Color::GreenLight)));
        }

        sections.push_back(ftxui::separator());
        sections.push_back(renderFocusedWindow(ftxui::text(m_configureDialogTitle.empty() ? "Configure thing" : m_configureDialogTitle),
                                               ftxui::vbox({
                                                   ftxui::vbox(std::move(dialogBody)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::flex,
                                                   ftxui::separator(),
                                                   ftxui::vbox(std::move(dialogFooter)),
                                               }) | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 80),
                                               m_showConfigureDialog && m_focusArea == FocusArea::ConfigureDialog));
    }

    return ftxui::vbox(std::move(sections)) | ftxui::border | ftxui::flex;
}

bool Engine::handleEvent(const ftxui::Event& event, ftxui::ScreenInteractive& screen)
{
    auto applyMainMenuSelection = [this](MainMenuEntry entry) {
        m_selectedMainMenuEntry = entry;
        switch (entry) {
        case MainMenuEntry::Things:
            m_mainView = MainView::Things;
            break;
        case MainMenuEntry::ConfigureThings:
            m_mainView = MainView::ConfigureThings;
            if (!m_haveAllThingClasses && !m_fetchAllThingClassesPending) {
                fetchAllThingClasses();
            }
            break;
        case MainMenuEntry::ApiBrowser:
            m_mainView = MainView::ApiBrowser;
            m_apiBrowserJsonLineIndex = 0;
            ensureApiBrowserLoaded();
            clampApiBrowserSelection();
            clampApiBrowserReferenceSelection();
            break;
        case MainMenuEntry::Settings:
            m_mainView = MainView::Settings;
            m_settingsDetailsLineIndex = 0;
            ensureSystemCapabilitiesLoaded();
            ensureSystemTimeLoaded();
            ensureSystemUpdateStatusLoaded();
            if (m_settingsView == SettingsView::Timezone) {
                ensureSystemTimeZonesLoaded();
            } else if (m_settingsView == SettingsView::Update) {
                ensureSystemPackagesLoaded();
            }
            break;
        case MainMenuEntry::Logout:
            m_mainView = MainView::Logout;
            break;
        case MainMenuEntry::About:
            m_mainView = MainView::About;
            break;
        }
    };
    auto syncMainMenuSelectionToCurrentView = [this]() {
        switch (m_mainView) {
        case MainView::Things:
            m_selectedMainMenuEntry = MainMenuEntry::Things;
            break;
        case MainView::ConfigureThings:
            m_selectedMainMenuEntry = MainMenuEntry::ConfigureThings;
            break;
        case MainView::ApiBrowser:
            m_selectedMainMenuEntry = MainMenuEntry::ApiBrowser;
            break;
        case MainView::Settings:
            m_selectedMainMenuEntry = MainMenuEntry::Settings;
            break;
        case MainView::Logout:
            m_selectedMainMenuEntry = MainMenuEntry::Logout;
            break;
        case MainView::About:
            m_selectedMainMenuEntry = MainMenuEntry::About;
            break;
        case MainView::Help:
            break;
        }
    };

    if (event == ftxui::Event::Custom) {
        drainUiTasks();
        return true;
    }

    if (handleMouseWheel(event)) {
        return true;
    }

    if (m_connectionLost) {
        if (event == ftxui::Event::Character("c")) {
            connectToServer();
            if (m_client.isConnected()) {
                runHandshakeAndLoadThings();
            }
            return true;
        }
        if (event == ftxui::Event::Character("q") || event == ftxui::Event::Escape) {
            m_client.disconnectFromHost();
            screen.ExitLoopClosure()();
            return true;
        }
        return true;
    }

    if (m_showActionDialog) {
        if (m_actionExecutionPending) {
            return true;
        }
        if (event == ftxui::Event::Escape) {
            closeActionDialog();
            return true;
        }
        if (event == ftxui::Event::Return) {
            return executeCurrentAction();
        }
        if (event == ftxui::Event::ArrowUp && !m_actionDialogParamTypes.empty()) {
            m_actionDialogSelectedParamIndex = (m_actionDialogSelectedParamIndex + static_cast<int>(m_actionDialogParamTypes.size()) - 1)
                                               % static_cast<int>(m_actionDialogParamTypes.size());
            m_actionDialogRangeEditIndex.reset();
            return true;
        }
        if (event == ftxui::Event::ArrowDown && !m_actionDialogParamTypes.empty()) {
            m_actionDialogSelectedParamIndex = (m_actionDialogSelectedParamIndex + 1) % static_cast<int>(m_actionDialogParamTypes.size());
            m_actionDialogRangeEditIndex.reset();
            return true;
        }
        if (!m_actionDialogParamTypes.empty() && m_actionDialogSelectedParamIndex >= 0 && m_actionDialogSelectedParamIndex < static_cast<int>(m_actionDialogParamValues.size())) {
            const api::ParamType& currentParamType = m_actionDialogParamTypes.at(m_actionDialogSelectedParamIndex);
            std::string& currentValue = m_actionDialogParamValues.at(m_actionDialogSelectedParamIndex);
            bool isRangeTextEditing = m_actionDialogRangeEditIndex.has_value() && *m_actionDialogRangeEditIndex == m_actionDialogSelectedParamIndex;
            if (handleParamValueEditEvent(event, currentParamType, currentValue, isRangeTextEditing)) {
                if (actionParamUsesRangeInput(currentParamType) && isRangeTextEditing) {
                    m_actionDialogRangeEditIndex = m_actionDialogSelectedParamIndex;
                } else {
                    m_actionDialogRangeEditIndex.reset();
                }
                return true;
            }
        }

        return true;
    }

    if (m_showConfigureDialog) {
        if (m_configureRequestPending) {
            return true;
        }

        if (event == ftxui::Event::Escape) {
            closeConfigureDialog();
            return true;
        }

        if (m_configureFlowComplete) {
            if (event == ftxui::Event::Return) {
                closeConfigureDialog();
            }
            return true;
        }

        if (event == ftxui::Event::Return) {
            return submitConfigureDialog();
        }

        auto editText = [&](std::string& value) {
            if (event == ftxui::Event::Backspace && !value.empty()) {
                value.pop_back();
                return true;
            }
            if (event.is_character()) {
                value += event.character();
                return true;
            }
            return false;
        };

        switch (m_configureDialogMode) {
        case ConfigureDialogMode::AddChooseCreateMethod:
            if (event == ftxui::Event::ArrowUp && !m_configureCreateMethodOptions.empty()) {
                m_configureCreateMethodIndex = (m_configureCreateMethodIndex + static_cast<int>(m_configureCreateMethodOptions.size()) - 1)
                                               % static_cast<int>(m_configureCreateMethodOptions.size());
                return true;
            }
            if (event == ftxui::Event::ArrowDown && !m_configureCreateMethodOptions.empty()) {
                m_configureCreateMethodIndex = (m_configureCreateMethodIndex + 1) % static_cast<int>(m_configureCreateMethodOptions.size());
                return true;
            }
            break;
        case ConfigureDialogMode::AddManualParams: {
            const int rowCount = 1 + static_cast<int>(m_configureParamTypes.size());
            if (event == ftxui::Event::ArrowUp && rowCount > 0) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + rowCount - 1) % rowCount;
                m_configureRangeEditIndex.reset();
                return true;
            }
            if (event == ftxui::Event::ArrowDown && rowCount > 0) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + 1) % rowCount;
                m_configureRangeEditIndex.reset();
                return true;
            }
            if (m_configureParamSelectionIndex == 0) {
                m_configureRangeEditIndex.reset();
                return editText(m_configureThingName);
            }

            if (m_configureParamSelectionIndex - 1 >= 0 && m_configureParamSelectionIndex - 1 < static_cast<int>(m_configureParamTypes.size())) {
                const api::ParamType& currentParamType = m_configureParamTypes.at(m_configureParamSelectionIndex - 1);
                std::string& currentValue = m_configureParamValues.at(m_configureParamSelectionIndex - 1);
                const int currentParamIndex = m_configureParamSelectionIndex - 1;
                bool isRangeTextEditing = m_configureRangeEditIndex.has_value() && *m_configureRangeEditIndex == currentParamIndex;
                if (handleParamValueEditEvent(event, currentParamType, currentValue, isRangeTextEditing)) {
                    if (actionParamUsesRangeInput(currentParamType) && isRangeTextEditing) {
                        m_configureRangeEditIndex = currentParamIndex;
                    } else {
                        m_configureRangeEditIndex.reset();
                    }
                    return true;
                }
            }
            return true;
        }
        case ConfigureDialogMode::AddDiscoveryParams:
            if (event == ftxui::Event::ArrowUp && !m_configureParamTypes.empty()) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + static_cast<int>(m_configureParamTypes.size()) - 1)
                                                 % static_cast<int>(m_configureParamTypes.size());
                m_configureRangeEditIndex.reset();
                return true;
            }
            if (event == ftxui::Event::ArrowDown && !m_configureParamTypes.empty()) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + 1) % static_cast<int>(m_configureParamTypes.size());
                m_configureRangeEditIndex.reset();
                return true;
            }
            if (!m_configureParamTypes.empty()) {
                const api::ParamType& currentParamType = m_configureParamTypes.at(m_configureParamSelectionIndex);
                std::string& currentValue = m_configureParamValues.at(m_configureParamSelectionIndex);
                bool isRangeTextEditing = m_configureRangeEditIndex.has_value() && *m_configureRangeEditIndex == m_configureParamSelectionIndex;
                if (handleParamValueEditEvent(event, currentParamType, currentValue, isRangeTextEditing)) {
                    if (actionParamUsesRangeInput(currentParamType) && isRangeTextEditing) {
                        m_configureRangeEditIndex = m_configureParamSelectionIndex;
                    } else {
                        m_configureRangeEditIndex.reset();
                    }
                    return true;
                }
            }
            return true;
        case ConfigureDialogMode::AddDiscoveryResults:
            if (event == ftxui::Event::ArrowUp && !m_configureThingDescriptors.empty()) {
                m_configureThingDescriptorIndex = (m_configureThingDescriptorIndex + static_cast<int>(m_configureThingDescriptors.size()) - 1)
                                                  % static_cast<int>(m_configureThingDescriptors.size());
                return true;
            }
            if (event == ftxui::Event::ArrowDown && !m_configureThingDescriptors.empty()) {
                m_configureThingDescriptorIndex = (m_configureThingDescriptorIndex + 1) % static_cast<int>(m_configureThingDescriptors.size());
                return true;
            }
            return true;
        case ConfigureDialogMode::AddPairingConfirmation: {
            int fieldCount = 0;
            const bool needsUsername = m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword;
            const bool needsSecret = m_configureSetupMethod == api::SetupMethod::SetupMethodDisplayPin || m_configureSetupMethod == api::SetupMethod::SetupMethodEnterPin
                                     || m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword || m_configureSetupMethod == api::SetupMethod::SetupMethodOAuth;
            if (needsUsername) {
                ++fieldCount;
            }
            if (needsSecret) {
                ++fieldCount;
            }
            if (fieldCount > 0 && event == ftxui::Event::ArrowUp) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + fieldCount - 1) % fieldCount;
                return true;
            }
            if (fieldCount > 0 && event == ftxui::Event::ArrowDown) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + 1) % fieldCount;
                return true;
            }
            if (needsUsername && m_configureParamSelectionIndex == 0) {
                return editText(m_configurePairingUsername);
            }
            if (needsSecret && ((!needsUsername && m_configureParamSelectionIndex == 0) || (needsUsername && m_configureParamSelectionIndex == 1))) {
                return editText(m_configurePairingSecret);
            }
            return true;
        }
        case ConfigureDialogMode::RenameThing:
            return editText(m_configureThingName);
        case ConfigureDialogMode::RemoveThingConfirm:
        case ConfigureDialogMode::ReconfigureThingInfo:
        case ConfigureDialogMode::None:
            return true;
        }
    }

    if (event == ftxui::Event::Escape && m_focusArea == FocusArea::ThingSearch && !m_thingSearch.empty()) {
        const QUuid selectedId = selectedThingId();
        m_thingSearch.clear();
        clampThingSelection(selectedId);
        resetThingDetailSelection();
        return true;
    }

    if (m_showLogoutConfirm) {
        if (m_logoutRequestPending) {
            return true;
        }
        if (event == ftxui::Event::Escape) {
            m_showLogoutConfirm = false;
            m_logoutStatus.clear();
            return true;
        }
        if (event == ftxui::Event::Return) {
            revokeCurrentTokenAndLogout();
            return true;
        }
        return true;
    }

    if (m_showSystemActionConfirm) {
        if (m_systemActionRequestPending) {
            return true;
        }
        if (event == ftxui::Event::Escape || event == ftxui::Event::ArrowLeft) {
            closePowerActionConfirmDialog();
            m_systemActionStatus.clear();
            return true;
        }
        if (event == ftxui::Event::Return) {
            executePowerAction();
            return true;
        }
        return true;
    }

    if (m_mainView == MainView::Settings && m_systemActionRequestPending) {
        return true;
    }

    if (m_mainView == MainView::Help) {
        if (event == ftxui::Event::Escape) {
            closeHelpView();
            return true;
        }
        if (event == ftxui::Event::ArrowUp) {
            const int lineCount = helpLineCount();
            if (lineCount > 0) {
                m_helpLineIndex = (m_helpLineIndex + lineCount - 1) % lineCount;
            }
            return true;
        }
        if (event == ftxui::Event::ArrowDown) {
            const int lineCount = helpLineCount();
            if (lineCount > 0) {
                m_helpLineIndex = (m_helpLineIndex + 1) % lineCount;
            }
            return true;
        }
        if (event == ftxui::Event::Character("q")) {
            m_client.disconnectFromHost();
            screen.ExitLoopClosure()();
            return true;
        }
        return true;
    }

    if (event == ftxui::Event::Character("q") || event == ftxui::Event::Escape) {
        m_client.disconnectFromHost();
        screen.ExitLoopClosure()();
        return true;
    }

    if (m_showLoginForm) {
        if (event == ftxui::Event::Return) {
            if (m_pushButtonAuthAvailable) {
                requestPushButtonAuth();
            } else {
                authenticate(m_username, m_password);
            }
            return true;
        }
        if (!m_pushButtonAuthAvailable) {
            if (event == ftxui::Event::ArrowUp) {
                m_loginSelectedInputIndex = std::max(0, m_loginSelectedInputIndex - 1);
            } else if (event == ftxui::Event::ArrowDown) {
                m_loginSelectedInputIndex = std::min(1, m_loginSelectedInputIndex + 1);
            }
        }
        if (m_pushButtonAuthAvailable) {
            return true;
        }
        if (m_loginForm->OnEvent(event)) {
            return true;
        }
        return true;
    }

    if (m_mainView == MainView::ApiBrowser) {
        const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
        const std::vector<ApiBrowserItem> filteredItems = filterApiBrowserItems(items, QString::fromStdString(m_apiBrowserSearch));
        const int selectedVisibleIndex = apiBrowserItemIndex(filteredItems, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
        const int currentIndex = apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
        const ApiBrowserItem* currentItem = currentIndex >= 0 ? &items.at(currentIndex) : nullptr;

        auto followSelectedReference = [&]() {
            if (currentItem == nullptr || currentItem->references.empty()) {
                return true;
            }
            if (m_apiBrowserSelectedReferenceIndex < 0 || m_apiBrowserSelectedReferenceIndex >= static_cast<int>(currentItem->references.size())) {
                return true;
            }

            const QString referenceName = currentItem->references.at(m_apiBrowserSelectedReferenceIndex).second;
            const ApiBrowserItem* targetItem = nullptr;
            for (const ApiBrowserItem& item : items) {
                if ((item.kind == ApiBrowserKind::Type || item.kind == ApiBrowserKind::Enum) && item.name == referenceName) {
                    targetItem = &item;
                    break;
                }
            }
            if (targetItem == nullptr) {
                for (const ApiBrowserItem& item : items) {
                    if (item.name == referenceName) {
                        targetItem = &item;
                        break;
                    }
                }
            }

            if (targetItem == nullptr) {
                m_apiBrowserStatus = "Referenced item not found: " + referenceName.toStdString();
                return true;
            }

            selectApiBrowserItem(targetItem->section, targetItem->name, true);
            clampApiBrowserReferenceSelection();
            return true;
        };

        if (event == ftxui::Event::Return) {
            if (m_focusArea == FocusArea::ApiBrowserSearch) {
                m_focusArea = FocusArea::ApiBrowserList;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences) {
                return followSelectedReference();
            }
            return true;
        }

        if (event == ftxui::Event::ArrowRight) {
            if (m_focusArea == FocusArea::ApiBrowserSearch) {
                m_focusArea = FocusArea::ApiBrowserList;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserList) {
                m_focusArea = FocusArea::ApiBrowserReferences;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences) {
                m_focusArea = FocusArea::ApiBrowserSearch;
                return true;
            }
        }

        if (event == ftxui::Event::ArrowLeft) {
            if (!m_apiBrowserHistory.empty()) {
                apiBrowserGoBack();
                clampApiBrowserReferenceSelection();
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences) {
                m_focusArea = FocusArea::ApiBrowserList;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserList) {
                m_focusArea = FocusArea::ApiBrowserSearch;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserSearch) {
                syncMainMenuSelectionToCurrentView();
                m_focusArea = FocusArea::MainMenu;
                return true;
            }
        }

        if (event == ftxui::Event::ArrowUp) {
            if (m_focusArea == FocusArea::ApiBrowserList && !filteredItems.empty()) {
                int index = selectedVisibleIndex;
                if (index < 0) {
                    index = static_cast<int>(filteredItems.size()) - 1;
                } else {
                    index = (index + static_cast<int>(filteredItems.size()) - 1) % static_cast<int>(filteredItems.size());
                }
                selectApiBrowserItem(filteredItems.at(index).section, filteredItems.at(index).name, false);
                clampApiBrowserReferenceSelection();
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences && currentItem != nullptr && !currentItem->references.empty()) {
                m_apiBrowserSelectedReferenceIndex = (m_apiBrowserSelectedReferenceIndex + static_cast<int>(currentItem->references.size()) - 1)
                                                     % static_cast<int>(currentItem->references.size());
                return true;
            }
        }

        if (event == ftxui::Event::ArrowDown) {
            if (m_focusArea == FocusArea::ApiBrowserList && !filteredItems.empty()) {
                int index = selectedVisibleIndex;
                if (index < 0) {
                    index = 0;
                } else {
                    index = (index + 1) % static_cast<int>(filteredItems.size());
                }
                selectApiBrowserItem(filteredItems.at(index).section, filteredItems.at(index).name, false);
                clampApiBrowserReferenceSelection();
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences && currentItem != nullptr && !currentItem->references.empty()) {
                m_apiBrowserSelectedReferenceIndex = (m_apiBrowserSelectedReferenceIndex + 1) % static_cast<int>(currentItem->references.size());
                return true;
            }
        }

        if (m_focusArea == FocusArea::ApiBrowserSearch) {
            if (event == ftxui::Event::Backspace && !m_apiBrowserSearch.empty()) {
                m_apiBrowserSearch.pop_back();
                return true;
            }
            if (event.is_character()) {
                m_apiBrowserSearch += event.character();
                return true;
            }
        }
    }

    if (event == ftxui::Event::ArrowLeft) {
        if (m_focusArea == FocusArea::ThingDetails) {
            m_focusArea = FocusArea::ThingList;
            m_showThingDetailInspector = false;
            return true;
        }
        if (m_focusArea == FocusArea::ThingList) {
            m_focusArea = FocusArea::ThingSearch;
            return true;
        }
        if (m_focusArea == FocusArea::ThingSearch) {
            syncMainMenuSelectionToCurrentView();
            m_focusArea = FocusArea::MainMenu;
            return true;
        }
        if (m_focusArea == FocusArea::ConfigureThingClassList) {
            m_focusArea = FocusArea::ConfigureThingClassSearch;
            return true;
        }
        if (m_focusArea == FocusArea::ConfigureThingClassSearch || m_focusArea == FocusArea::ConfigureThingSelection || m_focusArea == FocusArea::ConfigureMenu) {
            syncMainMenuSelectionToCurrentView();
            m_focusArea = FocusArea::MainMenu;
            return true;
        }
        if (m_focusArea == FocusArea::SettingsMenu) {
            syncMainMenuSelectionToCurrentView();
            m_focusArea = FocusArea::MainMenu;
            return true;
        }
        if (m_focusArea == FocusArea::SettingsDetails) {
            m_focusArea = FocusArea::SettingsMenu;
            return true;
        }
        m_focusArea = FocusArea::MainMenu;
        syncMainMenuSelectionToCurrentView();
        return true;
    }

    if (event == ftxui::Event::ArrowRight) {
        if (m_mainView == MainView::Things) {
            if (m_focusArea == FocusArea::MainMenu) {
                m_focusArea = FocusArea::ThingSearch;
            } else if (m_focusArea == FocusArea::ThingSearch) {
                m_focusArea = FocusArea::ThingList;
            } else if (m_focusArea == FocusArea::ThingList && thingDetailEntryCount() > 0) {
                selectInitialThingDetailSection();
                m_focusArea = FocusArea::ThingDetails;
            } else {
                m_focusArea = FocusArea::ThingList;
            }
        } else if (m_mainView == MainView::ConfigureThings) {
            if (!m_haveAllThingClasses && !m_fetchAllThingClassesPending) {
                fetchAllThingClasses();
            }
            if (m_focusArea == FocusArea::ConfigureMenu) {
                m_focusArea = m_configureThingsView == ConfigureThingsView::AddThing ? FocusArea::ConfigureThingClassSearch : FocusArea::ConfigureThingSelection;
            } else if (m_focusArea == FocusArea::ConfigureThingClassSearch) {
                m_focusArea = FocusArea::ConfigureThingClassList;
            } else {
                m_focusArea = FocusArea::ConfigureMenu;
            }
        } else if (m_mainView == MainView::ApiBrowser) {
            if (m_focusArea == FocusArea::MainMenu) {
                m_focusArea = FocusArea::ApiBrowserSearch;
            } else if (m_focusArea == FocusArea::ApiBrowserSearch) {
                m_focusArea = FocusArea::ApiBrowserList;
            } else if (m_focusArea == FocusArea::ApiBrowserList) {
                m_focusArea = FocusArea::ApiBrowserReferences;
            } else {
                m_focusArea = FocusArea::ApiBrowserSearch;
            }
        } else if (m_mainView == MainView::Settings) {
            if (m_focusArea == FocusArea::SettingsMenu) {
                m_focusArea = FocusArea::SettingsDetails;
                clampSettingsDetailsSelection();
            } else if (m_focusArea != FocusArea::SettingsDetails) {
                m_focusArea = FocusArea::SettingsMenu;
            }
        }
        return true;
    }

    if (m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsDetails && m_settingsView == SettingsView::Timezone
        && (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown)) {
        const int filteredCount = m_systemTimeZonesLoaded ? static_cast<int>(filteredSystemTimeZones().size()) : 0;
        m_settingsDetailsLineIndex = nextTimezoneDetailsLineIndex(m_settingsDetailsLineIndex, event == ftxui::Event::ArrowDown ? 1 : -1, filteredCount);
        return true;
    }

    if (event == ftxui::Event::ArrowUp) {
        if (m_focusArea == FocusArea::ThingDetails && thingDetailEntryCount() > 0) {
            m_selectedThingDetailIndex = (m_selectedThingDetailIndex + thingDetailEntryCount() - 1) % thingDetailEntryCount();
            return true;
        }

        if (m_focusArea == FocusArea::ThingList && !filteredThings().empty()) {
            const int count = static_cast<int>(filteredThings().size());
            m_selectedThingIndex = (m_selectedThingIndex + count - 1) % count;
            resetThingDetailSelection();
            return true;
        }

        if (m_mainView == MainView::Settings) {
            if (m_focusArea == FocusArea::SettingsMenu) {
                if (m_settingsView == SettingsView::Reboot) {
                    m_settingsView = SettingsView::ServerInfo;
                } else {
                    m_settingsView = static_cast<SettingsView>(static_cast<int>(m_settingsView) - 1);
                }
                m_settingsDetailsLineIndex = 0;
                ensureSystemCapabilitiesLoaded();
                ensureSystemTimeLoaded();
                ensureSystemUpdateStatusLoaded();
                if (m_settingsView == SettingsView::Timezone) {
                    ensureSystemTimeZonesLoaded();
                } else if (m_settingsView == SettingsView::Update) {
                    ensureSystemPackagesLoaded();
                }
                return true;
            }
            if (m_focusArea == FocusArea::SettingsDetails) {
                const int lineCount = settingsDetailsLineCount();
                if (lineCount > 0) {
                    m_settingsDetailsLineIndex = (m_settingsDetailsLineIndex + lineCount - 1) % lineCount;
                }
                return true;
            }
        }

        if (m_focusArea == FocusArea::ConfigureMenu && m_mainView == MainView::ConfigureThings) {
            if (m_configureThingsView == ConfigureThingsView::AddThing) {
                m_configureThingsView = ConfigureThingsView::RenameThing;
            } else {
                m_configureThingsView = static_cast<ConfigureThingsView>(static_cast<int>(m_configureThingsView) - 1);
            }
            if (m_configureThingsView == ConfigureThingsView::AddThing && !m_haveAllThingClasses && !m_fetchAllThingClassesPending) {
                fetchAllThingClasses();
            }
            return true;
        }

        if (m_focusArea == FocusArea::ConfigureThingClassList && !filteredConfigThingClasses().empty()) {
            const int count = static_cast<int>(filteredConfigThingClasses().size());
            m_selectedConfigureThingClassIndex = (m_selectedConfigureThingClassIndex + count - 1) % count;
            return true;
        }

        if (m_focusArea == FocusArea::ConfigureThingSelection && !m_thingManager.things().empty()) {
            m_selectedConfigureThingIndex = (m_selectedConfigureThingIndex + static_cast<int>(m_thingManager.things().size()) - 1)
                                            % static_cast<int>(m_thingManager.things().size());
            return true;
        }

        if (m_focusArea == FocusArea::ApiBrowserReferences) {
            const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
            const int currentIndex = apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
            if (currentIndex >= 0) {
                const ApiBrowserItem& currentItem = items.at(currentIndex);
                if (!currentItem.references.empty()) {
                    m_apiBrowserSelectedReferenceIndex = (m_apiBrowserSelectedReferenceIndex + static_cast<int>(currentItem.references.size()) - 1)
                                                         % static_cast<int>(currentItem.references.size());
                }
            }
            return true;
        }

        if (m_focusArea == FocusArea::MainMenu) {
            switch (m_selectedMainMenuEntry) {
            case MainMenuEntry::Things:
                applyMainMenuSelection(MainMenuEntry::About);
                break;
            case MainMenuEntry::ConfigureThings:
                applyMainMenuSelection(MainMenuEntry::Things);
                break;
            case MainMenuEntry::ApiBrowser:
                applyMainMenuSelection(MainMenuEntry::ConfigureThings);
                break;
            case MainMenuEntry::Settings:
                applyMainMenuSelection(MainMenuEntry::ApiBrowser);
                break;
            case MainMenuEntry::Logout:
                applyMainMenuSelection(MainMenuEntry::Settings);
                break;
            case MainMenuEntry::About:
                applyMainMenuSelection(MainMenuEntry::Logout);
                break;
            }
            return true;
        }
    }

    if (event == ftxui::Event::ArrowDown) {
        if (m_focusArea == FocusArea::ThingDetails && thingDetailEntryCount() > 0) {
            m_selectedThingDetailIndex = (m_selectedThingDetailIndex + 1) % thingDetailEntryCount();
            return true;
        }

        if (m_focusArea == FocusArea::ThingList && !filteredThings().empty()) {
            const int count = static_cast<int>(filteredThings().size());
            m_selectedThingIndex = (m_selectedThingIndex + 1) % count;
            resetThingDetailSelection();
            return true;
        }

        if (m_mainView == MainView::Settings) {
            if (m_focusArea == FocusArea::SettingsMenu) {
                if (m_settingsView == SettingsView::Reboot) {
                    m_settingsView = SettingsView::ServerInfo;
                } else {
                    m_settingsView = static_cast<SettingsView>(static_cast<int>(m_settingsView) + 1);
                }
                m_settingsDetailsLineIndex = 0;
                ensureSystemCapabilitiesLoaded();
                ensureSystemTimeLoaded();
                ensureSystemUpdateStatusLoaded();
                if (m_settingsView == SettingsView::Timezone) {
                    ensureSystemTimeZonesLoaded();
                } else if (m_settingsView == SettingsView::Update) {
                    ensureSystemPackagesLoaded();
                }
                return true;
            }
            if (m_focusArea == FocusArea::SettingsDetails) {
                const int lineCount = settingsDetailsLineCount();
                if (lineCount > 0) {
                    m_settingsDetailsLineIndex = (m_settingsDetailsLineIndex + 1) % lineCount;
                }
                return true;
            }
        }

        if (m_focusArea == FocusArea::ConfigureMenu && m_mainView == MainView::ConfigureThings) {
            if (m_configureThingsView == ConfigureThingsView::RenameThing) {
                m_configureThingsView = ConfigureThingsView::AddThing;
            } else {
                m_configureThingsView = static_cast<ConfigureThingsView>(static_cast<int>(m_configureThingsView) + 1);
            }
            if (m_configureThingsView == ConfigureThingsView::AddThing && !m_haveAllThingClasses && !m_fetchAllThingClassesPending) {
                fetchAllThingClasses();
            }
            return true;
        }

        if (m_focusArea == FocusArea::ConfigureThingClassList && !filteredConfigThingClasses().empty()) {
            const int count = static_cast<int>(filteredConfigThingClasses().size());
            m_selectedConfigureThingClassIndex = (m_selectedConfigureThingClassIndex + 1) % count;
            return true;
        }

        if (m_focusArea == FocusArea::ConfigureThingSelection && !m_thingManager.things().empty()) {
            m_selectedConfigureThingIndex = (m_selectedConfigureThingIndex + 1) % static_cast<int>(m_thingManager.things().size());
            return true;
        }

        if (m_focusArea == FocusArea::ApiBrowserList) {
            const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
            const std::vector<ApiBrowserItem> filteredItems = filterApiBrowserItems(items, QString::fromStdString(m_apiBrowserSearch));
            const int visibleIndex = apiBrowserItemIndex(filteredItems, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
            if (!filteredItems.empty()) {
                const int nextIndex = visibleIndex < 0 ? 0 : (visibleIndex + 1) % static_cast<int>(filteredItems.size());
                selectApiBrowserItem(filteredItems.at(nextIndex).section, filteredItems.at(nextIndex).name, false);
                clampApiBrowserReferenceSelection();
            }
            return true;
        }

        if (m_focusArea == FocusArea::MainMenu) {
            switch (m_selectedMainMenuEntry) {
            case MainMenuEntry::Things:
                applyMainMenuSelection(MainMenuEntry::ConfigureThings);
                break;
            case MainMenuEntry::ConfigureThings:
                applyMainMenuSelection(MainMenuEntry::ApiBrowser);
                break;
            case MainMenuEntry::ApiBrowser:
                applyMainMenuSelection(MainMenuEntry::Settings);
                break;
            case MainMenuEntry::Settings:
                applyMainMenuSelection(MainMenuEntry::Logout);
                break;
            case MainMenuEntry::Logout:
                applyMainMenuSelection(MainMenuEntry::About);
                break;
            case MainMenuEntry::About:
                applyMainMenuSelection(MainMenuEntry::Things);
                break;
            }
            return true;
        }
    }

    if (m_focusArea == FocusArea::MainMenu && event == ftxui::Event::Return) {
        if (m_selectedMainMenuEntry == MainMenuEntry::Logout) {
            if (m_client.isConnected() && m_isAuthenticated && !m_client.authToken().isEmpty()) {
                logout();
            }
            return true;
        }
        return true;
    }

    if (event == ftxui::Event::Return && m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsMenu) {
        m_focusArea = FocusArea::SettingsDetails;
        clampSettingsDetailsSelection();
        return true;
    }

    if (event == ftxui::Event::Return && m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsDetails) {
        switch (m_settingsView) {
        case SettingsView::ServerInfo:
            return true;
        case SettingsView::Timezone: {
            if (!m_systemTimeZonesLoaded || m_systemActionRequestPending) {
                return true;
            }
            const QStringList filteredTimeZones = filteredSystemTimeZones();
            const int zoneCount = static_cast<int>(filteredTimeZones.size());
            const int zoneIndex = m_settingsDetailsLineIndex - timezoneListStartLineIndex;
            if (zoneIndex < 0 || zoneIndex >= zoneCount) {
                return true;
            }
            const QString timeZone = filteredTimeZones.at(zoneIndex);
            api::ConfigurationSetTimeZoneParams request;
            request.timeZone = timeZone;
            m_systemActionRequestPending = true;
            m_systemActionStatus = "Setting time zone to " + timeZone.toStdString() + "...";
            observeReply(m_client.sendRequest(api::ConfigurationSetTimeZoneMethod::methodName(), request.toJson()),
                         [this](const QJsonObject& message, const QString& transportError) { handleSetTimeZoneReply(message, transportError); });
            return true;
        }
        case SettingsView::Update: {
            if (m_settingsDetailsLineIndex == 4) {
                if (m_systemActionRequestPending) {
                    return true;
                }
                m_systemActionRequestPending = true;
                m_systemActionStatus = "Checking for updates...";
                observeReply(m_client.sendRequest(api::SystemCheckForUpdatesMethod::methodName(), QJsonObject{}),
                             [this](const QJsonObject& message, const QString& transportError) { handleCheckForUpdatesReply(message, transportError); });
                return true;
            }
            if (m_settingsDetailsLineIndex == 5) {
                if (!m_systemPackagesLoaded || m_systemActionRequestPending) {
                    return true;
                }
                if (m_systemPackages.empty()) {
                    m_systemActionStatus = "No packages are available for installation or update.";
                    return true;
                }
                QList<QString> packageIds;
                for (const api::Package& package : m_systemPackages) {
                    if (package.updateAvailable || package.installedVersion.isEmpty()) {
                        packageIds.append(package.id);
                    }
                }
                if (packageIds.isEmpty()) {
                    m_systemActionStatus = "No packages are available for installation or update.";
                    return true;
                }

                api::SystemUpdatePackagesParams request;
                request.packageIds = packageIds;
                m_systemActionRequestPending = true;
                m_systemActionStatus = "Starting package update...";
                observeReply(m_client.sendRequest(api::SystemUpdatePackagesMethod::methodName(), request.toJson()),
                             [this](const QJsonObject& message, const QString& transportError) { handleUpdatePackagesReply(message, transportError); });
                return true;
            }
            return true;
        }
        case SettingsView::Shutdown:
            openPowerActionConfirmDialog(PowerAction::Shutdown);
            return true;
        case SettingsView::Restart:
            openPowerActionConfirmDialog(PowerAction::Restart);
            return true;
        case SettingsView::Reboot:
            openPowerActionConfirmDialog(PowerAction::Reboot);
            return true;
        }
    }

    if (event == ftxui::Event::Character(" ")) {
        if (m_focusArea == FocusArea::ThingDetails && thingDetailEntryCount() > 0) {
            m_showThingDetailInspector = !m_showThingDetailInspector;
            return true;
        }
    }

    if (event == ftxui::Event::Return) {
        if (m_focusArea == FocusArea::ThingDetails) {
            return openSelectedActionDialog();
        }
        if (m_focusArea == FocusArea::ConfigureThingClassList && m_mainView == MainView::ConfigureThings && m_configureThingsView == ConfigureThingsView::AddThing) {
            openAddThingDialog();
            return true;
        }
        if (m_focusArea == FocusArea::ConfigureThingSelection && m_mainView == MainView::ConfigureThings) {
            switch (m_configureThingsView) {
            case ConfigureThingsView::RemoveThing:
                openRemoveThingDialog();
                break;
            case ConfigureThingsView::ReconfigureThing:
                openReconfigureThingDialog();
                break;
            case ConfigureThingsView::RenameThing:
                openRenameThingDialog();
                break;
            case ConfigureThingsView::AddThing:
                break;
            }
            return true;
        }
    }

    if (m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsDetails && m_settingsView == SettingsView::Timezone) {
        if (m_settingsDetailsLineIndex == timezoneSearchLineIndex) {
            if (event == ftxui::Event::Backspace && !m_systemTimeZoneSearch.empty()) {
                m_systemTimeZoneSearch.pop_back();
                clampSettingsDetailsSelection();
                return true;
            }
            if (event.is_character()) {
                m_systemTimeZoneSearch += event.character();
                clampSettingsDetailsSelection();
                return true;
            }
        }
    }

    if (m_focusArea == FocusArea::ThingSearch && m_mainView == MainView::Things) {
        if (event == ftxui::Event::Backspace && !m_thingSearch.empty()) {
            const QUuid selectedId = selectedThingId();
            m_thingSearch.pop_back();
            clampThingSelection(selectedId);
            resetThingDetailSelection();
            return true;
        }
        if (event.is_character()) {
            const QUuid selectedId = selectedThingId();
            m_thingSearch += event.character();
            clampThingSelection(selectedId);
            resetThingDetailSelection();
            return true;
        }
    }

    if (m_focusArea == FocusArea::ConfigureThingClassSearch && m_mainView == MainView::ConfigureThings && m_configureThingsView == ConfigureThingsView::AddThing) {
        if (event == ftxui::Event::Backspace && !m_configureThingSearch.empty()) {
            m_configureThingSearch.pop_back();
            clampConfigureThingClassSelection();
            return true;
        }
        if (event.is_character()) {
            m_configureThingSearch += event.character();
            clampConfigureThingClassSelection();
            return true;
        }
    }

    if (event == ftxui::Event::Character("s") && m_mainView == MainView::Things) {
        cycleThingSortMode();
        return true;
    }

    if (event == ftxui::Event::Character("f") && m_mainView == MainView::Things) {
        cycleThingCategoryFilter();
        return true;
    }

    if (event == ftxui::Event::Character("c")) {
        connectToServer();
        if (m_client.isConnected()) {
            runHandshakeAndLoadThings();
        }
        return true;
    }

    if (event == ftxui::Event::Character("h") || event == ftxui::Event::Character("?")) {
        if (m_focusArea != FocusArea::ThingSearch && m_focusArea != FocusArea::ConfigureThingClassSearch && m_focusArea != FocusArea::ApiBrowserSearch) {
            openHelpView();
            return true;
        }
    }

    if (event == ftxui::Event::Character("t")) {
        fetchThings();
        return true;
    }

    return false;
}

} // namespace nymea
