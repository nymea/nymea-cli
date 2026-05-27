// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "generated/actiontype.h"
#include "generated/apiutils.h"
#include "generated/param.h"
#include "generated/paramtype.h"
#include "generated/thing.h"
#include "generated/thingclass.h"
#include "generated/thingdescriptor.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QUuid>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <chrono>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <QStringList>

namespace nymea {

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

std::vector<ThingDetailEntry> buildThingDetailEntries(const api::Thing* thing, const api::ThingClass* thingClass);
std::string firstNonEmpty(std::initializer_list<std::string_view> candidates);
std::string optionalQStringToStd(const std::optional<QString>& value);
std::string uuidToStd(const QUuid& value);
void appendField(std::vector<std::string>& fields, const std::string& key, const std::string& value);
std::string jsonValueToString(const QJsonValue& value);
std::string prettyJsonValueToString(const QJsonValue& value);
std::string optionalJsonValueToString(const std::optional<QJsonValue>& value);
std::string optionalJsonValuesToString(const std::optional<QList<QJsonValue>>& values);
std::string joinFields(const std::vector<std::string>& fields);
std::string joinCommaSeparated(const std::vector<std::string>& values);
std::string optionalBoolToString(const std::optional<bool>& value);
std::string optionalDoubleToString(const std::optional<double>& value);
std::string optionalStringListToString(const std::optional<QStringList>& value);
std::string thingErrorLabel(api::ThingError error);
std::string createMethodLabel(api::CreateMethod createMethod);
std::string setupMethodLabel(api::SetupMethod setupMethod);
std::string thingClassLabel(const api::ThingClass& thingClass);
std::string descriptorLabel(const api::ThingDescriptor& descriptor);
bool containsCreateMethod(const api::ThingClass& thingClass, api::CreateMethod createMethod);
bool isThingClassAddable(const api::ThingClass& thingClass);
bool caseInsensitiveContains(const QString& value, const QString& needle);
bool caseInsensitiveContainsAny(const QString& value, std::initializer_list<const char*> needles);
std::string formatActionInvocation(const std::string& actionName, const std::vector<api::ParamType>& paramTypes, const std::vector<std::string>& paramValues);
std::string formatActionExecutionStatus(int requestId, const std::string& actionInvocation, const std::string& result);
std::string busyIndicator(std::chrono::steady_clock::time_point startedAt);
std::string thingLabel(const api::Thing* thing);
std::string prettyUnit(api::Unit unit);
std::optional<ftxui::Color> parseHexColor(const std::string& value);
ftxui::Element renderFocusedWindow(ftxui::Element title, ftxui::Element body, bool focused);
ftxui::Element renderActiveField(ftxui::Element element, bool focused, int minimumWidth = 0);
ftxui::Element renderBoolValue(bool value);
ftxui::Element renderColorValue(const std::string& colorString);
std::optional<std::string> formatUnixTimestampValue(const QJsonValue& value);
ftxui::Element renderValueCell(const QJsonValue& value, std::optional<api::BasicType> basicType, std::optional<api::Unit> unit);
ftxui::Element renderTwoColumnRow(const std::string& name, ftxui::Element value, bool selected, bool focused);
std::optional<QJsonValue> parseActionInputValue(const std::string& input, api::BasicType type);
bool buildParamList(const std::vector<api::ParamType>& paramTypes, const std::vector<std::string>& rawValues, api::ParamList& params, std::string& errorMessage);
std::string normalizedActionDialogValue(const api::ParamType& paramType, const std::string& rawValue);
bool actionParamUsesSelector(const api::ParamType& paramType);
bool actionParamUsesRangeInput(const api::ParamType& paramType);
bool handleParamValueEditEvent(const ftxui::Event& event, const api::ParamType& paramType, std::string& currentValue, bool& isRangeTextEditing);
std::string rangeInputSummary(const api::ParamType& paramType);
std::string cycleSelectableValue(const api::ParamType& paramType, const std::string& rawValue, int delta);
std::string normalizedActionSubmissionValue(const api::ParamType& paramType, const std::string& rawValue);
std::optional<double> currentNumericRangeValueFromRaw(const api::ParamType& paramType, const NumericRangeInputSpec& spec, const std::string& rawValue);
ftxui::Element renderActionDialogValueCell(const api::ParamType& paramType, const std::string& rawValue);

} // namespace nymea
