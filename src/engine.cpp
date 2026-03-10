#include "engine.h"

#include "generated/actiontype.h"
#include "generated/apiutils.h"
#include "generated/integrationsexecuteactionparams.h"
#include "generated/integrationsexecuteactionresponse.h"
#include "generated/integrationsgetthingclassesparams.h"
#include "generated/integrationsstatechangednotificationparams.h"
#include "generated/integrationsthingaddednotificationparams.h"
#include "generated/integrationsthingchangednotificationparams.h"
#include "generated/integrationsthingremovednotificationparams.h"
#include "generated/integrationsthingsettingchangednotificationparams.h"
#include "generated/jsonrpcsetnotificationstatusparams.h"
#include "generated/jsonrpcsetnotificationstatusresponse.h"
#include "generated/param.h"
#include "generated/state.h"
#include "generated/statetype.h"
#include "generated/thing.h"
#include "generated/thingclass.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMetaObject>

#include <array>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

namespace nymea {

namespace {

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

std::string thingErrorLabel(api::ThingError error)
{
    std::string value = api::toString(error).toStdString();
    constexpr std::string_view prefix = "ThingError";
    if (value.rfind(prefix.data(), 0) == 0) {
        value.erase(0, prefix.size());
    }
    return value;
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

ftxui::Element renderValueCell(const QJsonValue& value, std::optional<api::BasicType> basicType, std::optional<api::Unit> unit)
{
    if ((basicType.has_value() && *basicType == api::BasicType::Bool) || value.isBool()) {
        return renderBoolValue(value.toBool());
    }

    if (basicType.has_value() && *basicType == api::BasicType::Color && value.isString()) {
        return renderColorValue(value.toString().toStdString());
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

    if (actionParamUsesSelector(paramType)) {
        return ftxui::text("< " + normalizedValue + " >");
    }

    return ftxui::text(normalizedValue.empty() ? std::string("<empty>") : normalizedValue);
}

} // namespace

Engine::Engine(EngineOptions options)
    : m_options(std::move(options))
    , m_username(m_options.username)
    , m_password(m_options.password)
{
    m_client.setNotificationHandler([this](const QJsonObject& message) { enqueueUiTask([this, message]() { handleNotification(message); }); });
    m_passwordInputOption.password = true;
    m_usernameInput = ftxui::Input(&m_username, "Username");
    m_passwordInput = ftxui::Input(&m_password, "Password", m_passwordInputOption);
    m_loginForm = ftxui::Container::Vertical({m_usernameInput, m_passwordInput});
}

int Engine::run()
{
    auto screen = ftxui::ScreenInteractive::Fullscreen();
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

void Engine::clampThingSelection()
{
    if (m_thingManager.things().empty()) {
        m_selectedThingIndex = 0;
        m_showThingDetailInspector = false;
        if (m_focusArea == FocusArea::ThingDetails) {
            m_focusArea = FocusArea::ThingList;
        }
        return;
    }

    if (m_selectedThingIndex < 0) {
        m_selectedThingIndex = 0;
    } else if (m_selectedThingIndex >= static_cast<int>(m_thingManager.things().size())) {
        m_selectedThingIndex = static_cast<int>(m_thingManager.things().size()) - 1;
    }
}

int Engine::thingDetailEntryCount() const
{
    const api::Thing* thing = m_thingManager.thingAt(m_selectedThingIndex);
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
    const api::Thing* thing = m_thingManager.thingAt(m_selectedThingIndex);
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
        const std::string rawValue = normalizedActionDialogValue(paramType, m_actionDialogParamValues.at(index));
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

void Engine::handleNotification(const QJsonObject& message)
{
    const QString notificationName = message.value(QStringLiteral("notification")).toString();
    const QJsonObject params = message.value(QStringLiteral("params")).toObject();

    if (notificationName == api::IntegrationsStateChangedNotification::notificationName()) {
        const api::IntegrationsStateChangedNotificationParams notification = api::IntegrationsStateChangedNotificationParams::fromJson(params);
        if (m_thingManager
                .updateThingState(notification.thingId, notification.stateTypeId, notification.value, notification.minValue, notification.maxValue, notification.possibleValues)) {
            clampThingSelection();
            clampThingDetailSelection();
            m_thingManager.setStatus("Live update: state changed on " + thingLabel(m_thingManager.thingById(notification.thingId)) + ".");
        }
        return;
    }

    if (notificationName == api::IntegrationsThingAddedNotification::notificationName()) {
        const api::IntegrationsThingAddedNotificationParams notification = api::IntegrationsThingAddedNotificationParams::fromJson(params);
        m_thingManager.upsertThing(notification.thing);
        clampThingSelection();
        clampThingDetailSelection();

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
        clampThingSelection();
        clampThingDetailSelection();

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
            clampThingSelection();
            clampThingDetailSelection();
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

bool Engine::connectToServer()
{
    m_client.clearAuthToken();
    loadSavedConnection();
    m_helloPending = false;
    m_authenticationPending = false;
    m_notificationSetupPending = false;
    m_fetchThingsPending = false;
    m_fetchThingClassesPending = false;
    m_actionExecutionPending = false;
    m_pendingActionRequestId = -1;
    m_pendingActionInvocation.clear();
    m_notificationsEnabled = false;
    m_securityWarning.clear();
    m_settingsWarning.clear();
    m_serverVersion = "n/a";
    m_serverApiVersion = "n/a";
    m_serverUuid = QUuid();
    m_serverName.clear();

    const NymeaJsonRpcClient::TransportSecurity security = m_options.useSsl ? NymeaJsonRpcClient::TransportSecurity::SslTls : NymeaJsonRpcClient::TransportSecurity::PlainTcp;
    if (m_client.connectToHost(m_options.host, static_cast<quint16>(m_options.port), security, m_options.timeoutMs)) {
        m_connectionStatus = std::string(m_client.isEncrypted() ? "SSL connected to " : "TCP connected to ") + endpoint();
        updateCertificateWarning();
        return true;
    }

    m_connectionStatus = "Connection failed: " + m_client.lastError().toStdString();
    m_isAuthenticationRequired = false;
    m_isAuthenticated = false;
    m_showLoginForm = false;
    m_authStatus = "Not authenticated.";
    m_thingManager.clear();
    m_thingManager.setStatus(m_connectionStatus);
    return false;
}

void Engine::handleHelloReply(const QJsonObject& message, const QString& transportError)
{
    m_helloPending = false;

    if (!transportError.isEmpty()) {
        m_thingManager.setStatus("No reply for JSONRPC.Hello: " + transportError.toStdString());
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
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Stored token was rejected. Please login.";
        m_thingManager.setStatus("JSONRPC.Hello unauthorized (request id " + std::to_string(requestId) + ").");
        return;
    }

    if (status == QStringLiteral("error")) {
        m_thingManager.setStatus("JSONRPC.Hello returned error (request id " + std::to_string(requestId) + ").");
        return;
    }

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
    const bool helloAuthenticated = helloParams.value(QStringLiteral("authenticated")).toBool();
    m_isAuthenticated = helloAuthenticated || !m_isAuthenticationRequired;
    m_showLoginForm = m_isAuthenticationRequired && !m_isAuthenticated;

    if (!m_isAuthenticationRequired) {
        m_authStatus = "Server does not require authentication.";
    } else if (m_isAuthenticated) {
        m_authStatus = m_client.authToken().isEmpty() ? "Authenticated." : "Authenticated using stored token.";
    } else {
        m_authStatus = "Authentication required.";
    }

    saveCurrentConnection(false);
    m_thingManager.setStatus("JSONRPC.Hello succeeded (request id " + std::to_string(requestId) + ").");

    if (m_isAuthenticationRequired && !m_isAuthenticated) {
        if (!m_username.empty() && !m_password.empty()) {
            authenticate(m_username, m_password);
        } else {
            m_showLoginForm = true;
            m_focusArea = FocusArea::LoginForm;
            m_authStatus = "Authentication required. Enter username/password and press Enter.";
        }
        return;
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
        m_showLoginForm = true;
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    const int requestId = message.value(QStringLiteral("id")).toInt(-1);
    if (status == QStringLiteral("error") || status == QStringLiteral("unauthorized")) {
        if (status == QStringLiteral("unauthorized")) {
            clearStoredToken();
        }
        m_authStatus = "Authentication rejected by server.";
        m_showLoginForm = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_client.clearAuthToken();
        return;
    }

    const QJsonObject authParams = message.value(QStringLiteral("params")).toObject();
    if (!authParams.value(QStringLiteral("success")).toBool()) {
        m_authStatus = "Authentication failed.";
        m_showLoginForm = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_client.clearAuthToken();
        return;
    }

    const QString token = authParams.value(QStringLiteral("token")).toString();
    if (token.isEmpty()) {
        m_authStatus = "Authentication succeeded but no token was returned.";
        m_showLoginForm = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_client.clearAuthToken();
        return;
    }

    m_client.setAuthToken(token);
    m_isAuthenticated = true;
    m_isAuthenticationRequired = false;
    m_notificationsEnabled = false;
    m_showLoginForm = false;
    m_focusArea = FocusArea::MainMenu;
    m_authStatus = "Authenticated as " + m_username + ".";
    saveCurrentConnection(true);
    m_thingManager.setStatus("Authentication succeeded (request id " + std::to_string(requestId) + ").");
    enableNotifications(true);
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

    clampThingSelection();
    clampThingDetailSelection();
    m_thingManager.setStatus("Loaded " + std::to_string(m_thingManager.things().size()) + " thing(s) (request id " + std::to_string(requestId) + ").");
    fetchThingClasses();
}

void Engine::fetchThings()
{
    if (m_fetchThingsPending) {
        return;
    }

    m_thingManager.clear();

    if (!m_client.isConnected()) {
        m_thingManager.setStatus("Cannot fetch things while disconnected.");
        return;
    }

    if (m_isAuthenticationRequired && !m_isAuthenticated) {
        m_thingManager.setStatus("Authentication required before fetching things.");
        m_showLoginForm = true;
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

    m_thingManager.setStatus("Loaded " + std::to_string(m_thingManager.things().size()) + " thing(s) and enriched them with type metadata (request id " + std::to_string(requestId)
                             + ").");
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

void Engine::loadSavedConnection()
{
    m_savedConnection = m_connectionSettings.loadConnectionByEndpoint(m_options.host, m_options.port, m_options.useSsl);
    if (!m_savedConnection.has_value()) {
        return;
    }

    if (!m_savedConnection->token.isEmpty()) {
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

void Engine::clearStoredToken()
{
    QUuid hostUuid = m_serverUuid;
    if (hostUuid.isNull() && m_savedConnection.has_value()) {
        hostUuid = m_savedConnection->hostUuid;
    }

    QString errorMessage;
    if (!m_connectionSettings.clearToken(hostUuid, errorMessage)) {
        m_settingsWarning = "Settings warning: " + errorMessage.toStdString();
        return;
    }

    if (m_savedConnection.has_value() && m_savedConnection->hostUuid == hostUuid) {
        m_savedConnection->token.clear();
    }
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
    constexpr std::array<const char*, 2> menuItems = {"Things", "Settings"};

    ftxui::Elements entries;
    for (int index = 0; index < static_cast<int>(menuItems.size()); ++index) {
        const bool selected = (m_mainView == MainView::Things && index == 0) || (m_mainView == MainView::Settings && index == 1);
        auto entry = ftxui::text(std::string(" ") + menuItems.at(index) + " ");
        if (selected) {
            entry = entry | ftxui::bold | ftxui::inverted;
        }
        if (m_focusArea == FocusArea::MainMenu && selected) {
            entry = entry | ftxui::color(ftxui::Color::CyanLight);
        }
        if (selected) {
            entry = entry | ftxui::focus;
        }
        entries.push_back(entry);
    }

    return ftxui::window(ftxui::text("Menu"), ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::frame);
}

ftxui::Element Engine::renderThingList() const
{
    ftxui::Elements lines;
    lines.push_back(ftxui::text(m_thingManager.status()));
    lines.push_back(ftxui::separator());

    if (m_thingManager.things().empty()) {
        lines.push_back(ftxui::text("No things found."));
    } else {
        for (int index = 0; index < static_cast<int>(m_thingManager.things().size()); ++index) {
            const api::Thing& thing = m_thingManager.things().at(index);
            const std::string thingName = thing.name.has_value() && !thing.name->isEmpty() ? thing.name->toStdString() : std::string("<unnamed>");
            auto entry = ftxui::text(std::string(" ") + thingName + " ");
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

    return ftxui::window(ftxui::text("Things"), ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame);
}

ftxui::Element Engine::renderThingDetails() const
{
    const api::Thing* thing = m_thingManager.thingAt(m_selectedThingIndex);
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

    auto detailBrowser = ftxui::window(ftxui::text("Values"), ftxui::vbox(std::move(browserRows)) | ftxui::vscroll_indicator | ftxui::frame) | ftxui::flex;

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

    return ftxui::vbox(std::move(detailContent)) | ftxui::flex;
}

ftxui::Element Engine::renderSettingsMenu() const
{
    constexpr std::array<const char*, 2> menuItems = {"General", "About"};

    ftxui::Elements entries;
    for (int index = 0; index < static_cast<int>(menuItems.size()); ++index) {
        const bool selected = (m_settingsView == SettingsView::General && index == 0) || (m_settingsView == SettingsView::About && index == 1);
        auto entry = ftxui::text(std::string(" ") + menuItems.at(index) + " ");
        if (selected) {
            entry = entry | ftxui::bold | ftxui::inverted;
        }
        if (m_focusArea == FocusArea::SettingsMenu && selected) {
            entry = entry | ftxui::color(ftxui::Color::CyanLight);
        }
        if (selected) {
            entry = entry | ftxui::focus;
        }
        entries.push_back(entry);
    }

    return ftxui::window(ftxui::text("Settings"), ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::frame);
}

ftxui::Element Engine::renderSettingsDetails() const
{
    ftxui::Elements lines;

    if (m_settingsView == SettingsView::General) {
        const QString fingerprint = m_client.peerCertificateFingerprint();
        lines.push_back(ftxui::text("Connection: " + endpoint()));
        lines.push_back(ftxui::text("Display name: " + connectionDisplayName()));
        lines.push_back(ftxui::text("Settings path: " + m_connectionSettings.settingsPath().toStdString()));
        lines.push_back(ftxui::text("Transport: " + std::string(m_options.useSsl ? "SSL/TLS" : "Plain TCP")));
        lines.push_back(ftxui::text("Server uuid: " + m_serverUuid.toString(QUuid::WithoutBraces).toStdString()));
        lines.push_back(ftxui::text("Authentication: " + m_authStatus));
        lines.push_back(ftxui::text("Stored token: " + std::string(m_savedConnection.has_value() && !m_savedConnection->token.isEmpty() ? "available" : "none")));
        lines.push_back(ftxui::text("TLS fingerprint: " + (fingerprint.isEmpty() ? std::string("n/a") : fingerprint.toStdString())));
    } else {
        lines.push_back(ftxui::text("nymea-cli"));
        lines.push_back(ftxui::separator());
        lines.push_back(ftxui::text("Application version: " + m_options.appVersion));
        lines.push_back(ftxui::text("Server version: " + m_serverVersion));
        lines.push_back(ftxui::text("Server API version: " + m_serverApiVersion));
        lines.push_back(ftxui::text("Purpose: terminal client for nymead"));
        lines.push_back(ftxui::text("Navigation: Up/Down move, Left/Right switch panels"));
    }

    return ftxui::window(ftxui::text(m_settingsView == SettingsView::General ? "General" : "About"), ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame);
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
    if (m_actionExecutionPending) {
        if (ftxui::ScreenInteractive* screen = ftxui::ScreenInteractive::Active(); screen != nullptr) {
            screen->RequestAnimationFrame();
        }
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
    sections.push_back(ftxui::text("Keys: Up/Down navigate, Left/Right switch panels, Space inspector, c reconnect, h hello, t refresh things, Enter login, q/Esc quit")
                       | ftxui::dim);
    if (!m_settingsWarning.empty()) {
        sections.push_back(ftxui::text(m_settingsWarning) | ftxui::color(ftxui::Color::Yellow));
    }
    sections.push_back(ftxui::separator());

    ftxui::Element rightPanel;
    if (m_mainView == MainView::Things) {
        rightPanel = renderThings() | ftxui::flex;
    } else {
        rightPanel = ftxui::vbox({
                         renderSettingsMenu(),
                         renderSettingsDetails() | ftxui::flex,
                     })
                     | ftxui::flex;
    }

    sections.push_back(ftxui::hbox({
                           renderMainMenu() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 24),
                           rightPanel | ftxui::flex,
                       })
                       | ftxui::flex);

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
        sections.push_back(ftxui::window(ftxui::text("Execute action"),
                                         ftxui::vbox({
                                             ftxui::vbox(std::move(dialogBody)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::flex,
                                             ftxui::separator(),
                                             ftxui::vbox(std::move(dialogFooter)),
                                         }))
                           | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 80));
    }

    if (m_showLoginForm) {
        sections.push_back(ftxui::separator());
        sections.push_back(ftxui::window(ftxui::text("Authentication required"),
                                         ftxui::vbox({
                                             ftxui::hbox({
                                                 ftxui::text("Username: "),
                                                 m_usernameInput->Render() | ftxui::xflex,
                                             }),
                                             ftxui::hbox({
                                                 ftxui::text("Password: "),
                                                 m_passwordInput->Render() | ftxui::xflex,
                                             }),
                                             ftxui::separator(),
                                             ftxui::text(m_authStatus),
                                         })));
    }

    return ftxui::vbox(std::move(sections)) | ftxui::border | ftxui::flex;
}

bool Engine::handleEvent(const ftxui::Event& event, ftxui::ScreenInteractive& screen)
{
    if (event == ftxui::Event::Custom) {
        drainUiTasks();
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
            return true;
        }
        if (event == ftxui::Event::ArrowDown && !m_actionDialogParamTypes.empty()) {
            m_actionDialogSelectedParamIndex = (m_actionDialogSelectedParamIndex + 1) % static_cast<int>(m_actionDialogParamTypes.size());
            return true;
        }
        if (!m_actionDialogParamTypes.empty() && m_actionDialogSelectedParamIndex >= 0 && m_actionDialogSelectedParamIndex < static_cast<int>(m_actionDialogParamValues.size())) {
            const api::ParamType& currentParamType = m_actionDialogParamTypes.at(m_actionDialogSelectedParamIndex);
            std::string& currentValue = m_actionDialogParamValues.at(m_actionDialogSelectedParamIndex);
            if (actionParamUsesSelector(currentParamType)) {
                if (event == ftxui::Event::ArrowLeft) {
                    currentValue = cycleSelectableValue(currentParamType, currentValue, -1);
                    return true;
                }
                if (event == ftxui::Event::ArrowRight || event == ftxui::Event::Character(" ")) {
                    currentValue = cycleSelectableValue(currentParamType, currentValue, 1);
                    return true;
                }
            }
            if (!actionParamUsesSelector(currentParamType) && event == ftxui::Event::Backspace && !currentValue.empty()) {
                currentValue.pop_back();
                return true;
            }
            if (!actionParamUsesSelector(currentParamType) && event.is_character()) {
                currentValue += event.character();
                return true;
            }
        }

        return true;
    }

    if (event == ftxui::Event::Character("q") || event == ftxui::Event::Escape) {
        m_client.disconnectFromHost();
        screen.ExitLoopClosure()();
        return true;
    }

    if (event == ftxui::Event::ArrowLeft) {
        if (m_focusArea == FocusArea::ThingDetails) {
            m_focusArea = FocusArea::ThingList;
            m_showThingDetailInspector = false;
            return true;
        }
        m_focusArea = FocusArea::MainMenu;
        return true;
    }

    if (event == ftxui::Event::ArrowRight) {
        if (m_showLoginForm) {
            m_focusArea = FocusArea::LoginForm;
        } else if (m_mainView == MainView::Things) {
            if (m_focusArea == FocusArea::ThingList && thingDetailEntryCount() > 0) {
                m_focusArea = FocusArea::ThingDetails;
            } else {
                m_focusArea = FocusArea::ThingList;
            }
        } else if (m_mainView == MainView::Settings) {
            m_focusArea = FocusArea::SettingsMenu;
        }
        return true;
    }

    if (m_showLoginForm) {
        if (event == ftxui::Event::Return) {
            authenticate(m_username, m_password);
            return true;
        }
        if (m_loginForm->OnEvent(event)) {
            return true;
        }
    }

    if (event == ftxui::Event::ArrowUp) {
        if (m_focusArea == FocusArea::ThingDetails && thingDetailEntryCount() > 0) {
            m_selectedThingDetailIndex = (m_selectedThingDetailIndex + thingDetailEntryCount() - 1) % thingDetailEntryCount();
            return true;
        }

        if (m_focusArea == FocusArea::ThingList && !m_thingManager.things().empty()) {
            m_selectedThingIndex = (m_selectedThingIndex + static_cast<int>(m_thingManager.things().size()) - 1) % static_cast<int>(m_thingManager.things().size());
            resetThingDetailSelection();
            return true;
        }

        if (m_focusArea == FocusArea::SettingsMenu && m_mainView == MainView::Settings) {
            m_settingsView = m_settingsView == SettingsView::General ? SettingsView::About : SettingsView::General;
            return true;
        }

        if (m_focusArea == FocusArea::MainMenu) {
            m_mainView = m_mainView == MainView::Things ? MainView::Settings : MainView::Things;
            return true;
        }
    }

    if (event == ftxui::Event::ArrowDown) {
        if (m_focusArea == FocusArea::ThingDetails && thingDetailEntryCount() > 0) {
            m_selectedThingDetailIndex = (m_selectedThingDetailIndex + 1) % thingDetailEntryCount();
            return true;
        }

        if (m_focusArea == FocusArea::ThingList && !m_thingManager.things().empty()) {
            m_selectedThingIndex = (m_selectedThingIndex + 1) % static_cast<int>(m_thingManager.things().size());
            resetThingDetailSelection();
            return true;
        }

        if (m_focusArea == FocusArea::SettingsMenu && m_mainView == MainView::Settings) {
            m_settingsView = m_settingsView == SettingsView::General ? SettingsView::About : SettingsView::General;
            return true;
        }

        if (m_focusArea == FocusArea::MainMenu) {
            m_mainView = m_mainView == MainView::Things ? MainView::Settings : MainView::Things;
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
    }

    if (event == ftxui::Event::Character("c")) {
        connectToServer();
        if (m_client.isConnected()) {
            runHandshakeAndLoadThings();
        }
        return true;
    }

    if (event == ftxui::Event::Character("h")) {
        runHandshakeAndLoadThings();
        return true;
    }

    if (event == ftxui::Event::Character("t")) {
        fetchThings();
        return true;
    }

    return false;
}

} // namespace nymea
