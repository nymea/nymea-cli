// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineutils.h"

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
#include "generated/jsonrpcsetnotificationstatusparams.h"
#include "generated/jsonrpcsetnotificationstatusresponse.h"
#include "generated/param.h"
#include "generated/state.h"
#include "generated/statetype.h"
#include "generated/thing.h"
#include "generated/thingclass.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace nymea {

#ifndef APP_LICENSE_SPDX
#define APP_LICENSE_SPDX "GPL-3.0-or-later"
#endif

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

std::optional<NumericRangeInputSpec> numericRangeInputSpec(const api::ParamType& paramType)
{
    auto isNumericRangeType = [](api::BasicType type) {
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
    };

    auto isIntegralNumericType = [](api::BasicType type) { return type == api::BasicType::Int || type == api::BasicType::Uint || type == api::BasicType::Time; };

    auto defaultNumericRangeStepSize = [](const NumericRangeInputSpec& spec) {
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
    };

    auto numericRangeFractionalDigits = [](double stepSize) -> int {
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
    };

    auto shouldScaleRangeDisplayAsPercentage = [](const api::ParamType& paramType, double minValue, double maxValue) {
        return paramType.unit.has_value() && *paramType.unit == api::Unit::UnitPercentage && minValue >= 0.0 && maxValue > 0.0 && maxValue <= 1.0 + 1e-9;
    };

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
    if (paramType.type == api::BasicType::Int || paramType.type == api::BasicType::Uint || paramType.type == api::BasicType::Time) {
        return std::to_string(static_cast<long long>(std::llround(clampedValue)));
    }

    QString text = QString::number(clampedValue, 'f', std::max(spec.decimals, 1));
    while (text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    if (text == QStringLiteral("-0")) {
        return "0";
    }
    return text.toStdString();
}

std::string formatNumericRangeValue(const api::ParamType& paramType, const NumericRangeInputSpec& spec, double rawValue)
{
    const double clampedValue = clampAndSnapNumericRangeValue(spec, rawValue);
    const double displayValue = rawNumericRangeValueToDisplay(spec, clampedValue);
    if ((paramType.type == api::BasicType::Int || paramType.type == api::BasicType::Uint || paramType.type == api::BasicType::Time) && spec.displayScale == 1.0) {
        return std::to_string(static_cast<long long>(std::llround(displayValue)));
    }

    QString text = QString::number(displayValue, 'f', spec.displayDecimals);
    while (text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    if (text == QStringLiteral("-0")) {
        return "0";
    }
    return text.toStdString();
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

std::string rangeInputUnitLabel(const api::ParamType& paramType)
{
    if (!paramType.unit.has_value()) {
        return {};
    }
    return prettyUnit(*paramType.unit);
}

std::string formatRangeDisplayValue(const api::ParamType& paramType, const NumericRangeInputSpec& spec, double value)
{
    std::string displayValue = formatNumericRangeValue(paramType, spec, value);
    if (const std::string unitLabel = rangeInputUnitLabel(paramType); !unitLabel.empty()) {
        displayValue += " " + unitLabel;
    }
    return displayValue;
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

bool rangeInputAcceptsCharacter(const api::ParamType& paramType, const std::string& currentValue, const std::string& typedCharacter);
std::string cycleRangeValue(const api::ParamType& paramType, const std::string& rawValue, int delta);

std::optional<double> currentNumericRangeValueFromRaw(const api::ParamType& paramType, const NumericRangeInputSpec& spec, const std::string& rawValue)
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

std::optional<double> currentNumericRangeValue(const api::ParamType& paramType, const NumericRangeInputSpec& spec, const std::string& rawValue)
{
    return ::nymea::currentNumericRangeValueFromRaw(paramType, spec, rawValue);
}

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

std::string prettyJsonValueToString(const QJsonValue& value)
{
    if (value.isArray()) {
        return QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented).toStdString();
    }
    if (value.isObject()) {
        return QJsonDocument(value.toObject()).toJson(QJsonDocument::Indented).toStdString();
    }
    return jsonValueToString(value);
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

std::string normalizedActionDialogValue(const api::ParamType& paramType, const std::string& rawValue)
{
    if (const std::optional<NumericRangeInputSpec> rangeSpec = numericRangeInputSpec(paramType); rangeSpec.has_value()) {
        const std::optional<double> currentValue = ::nymea::currentNumericRangeValueFromRaw(paramType, *rangeSpec, rawValue);
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

std::string normalizedActionSubmissionValue(const api::ParamType& paramType, const std::string& rawValue)
{
    if (const std::optional<NumericRangeInputSpec> rangeSpec = numericRangeInputSpec(paramType); rangeSpec.has_value()) {
        const std::optional<double> currentValue = currentNumericRangeValueFromRaw(paramType, *rangeSpec, rawValue);
        if (currentValue.has_value()) {
            return formatRawNumericRangeValue(paramType, *rangeSpec, *currentValue);
        }
        return rawValue;
    }

    return normalizedActionDialogValue(paramType, rawValue);
}

bool actionParamUsesSelector(const api::ParamType& paramType)
{
    return !selectableValuesForParamType(paramType).empty();
}

bool actionParamUsesRangeInput(const api::ParamType& paramType)
{
    return numericRangeInputSpec(paramType).has_value();
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

    const std::optional<double> currentValue = ::nymea::currentNumericRangeValueFromRaw(paramType, *spec, rawValue);
    const double current = currentValue.value_or(spec->minValue);
    const double nextValue = current + static_cast<double>(delta) * spec->stepSize;
    return formatNumericRangeValue(paramType, *spec, nextValue);
}

std::string cycleSelectableValue(const api::ParamType& paramType, const std::string& rawValue, int delta)
{
    const std::vector<QJsonValue> selectableValues = selectableValuesForParamType(paramType);
    if (selectableValues.empty()) {
        return rawValue;
    }

    const std::string normalizedValue = normalizedActionDialogValue(paramType, rawValue);
    int currentIndex = 0;
    for (int index = 0; index < static_cast<int>(selectableValues.size()); ++index) {
        if (jsonValueToString(selectableValues.at(index)) == normalizedValue) {
            currentIndex = index;
            break;
        }
    }

    const int nextIndex = (currentIndex + delta + static_cast<int>(selectableValues.size())) % static_cast<int>(selectableValues.size());
    return jsonValueToString(selectableValues.at(nextIndex));
}

ftxui::Element renderRangeValueCell(const api::ParamType& paramType, const std::string& rawValue)
{
    const std::optional<NumericRangeInputSpec> spec = numericRangeInputSpec(paramType);
    if (!spec.has_value()) {
        return ftxui::text(rawValue.empty() ? std::string("<empty>") : rawValue);
    }

    const std::optional<double> currentValue = ::nymea::currentNumericRangeValueFromRaw(paramType, *spec, rawValue);
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

} // namespace nymea
