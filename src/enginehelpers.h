// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "engine.h"
#include "engineutils.h"

#include "generated/apiutils.h"
#include "generated/loggingcategory.h"
#include "generated/serialport.h"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace nymea {

inline api::SampleRate sampleRateForChartRange(LogViewRange range)
{
    switch (range) {
    case LogViewRange::Hour:
        return api::SampleRate::SampleRate1Min;
    case LogViewRange::Day:
        return api::SampleRate::SampleRate15Mins;
    case LogViewRange::Week:
        return api::SampleRate::SampleRate1Hour;
    case LogViewRange::Month:
        return api::SampleRate::SampleRate3Hours;
    case LogViewRange::Year:
        return api::SampleRate::SampleRate1Day;
    }
    return api::SampleRate::SampleRate15Mins;
}

inline std::string chartRangeLabel(LogViewRange range)
{
    switch (range) {
    case LogViewRange::Hour:
        return "Hour";
    case LogViewRange::Day:
        return "Day (24h)";
    case LogViewRange::Week:
        return "Week";
    case LogViewRange::Month:
        return "Month";
    case LogViewRange::Year:
        return "Year";
    }
    return {};
}

inline QDateTime chartWindowStart(LogViewRange range, const QDateTime& end)
{
    switch (range) {
    case LogViewRange::Hour:
        return end.addSecs(-3600);
    case LogViewRange::Day:
        return end.addDays(-1);
    case LogViewRange::Week:
        return end.addDays(-7);
    case LogViewRange::Month:
        return end.addMonths(-1);
    case LogViewRange::Year:
        return end.addYears(-1);
    }
    return end.addDays(-1);
}

inline std::string formatChartTimestamp(qint64 msecs, LogViewRange range)
{
    const QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
    switch (range) {
    case LogViewRange::Hour:
    case LogViewRange::Day:
        return dateTime.toString(QStringLiteral("hh:mm")).toStdString();
    case LogViewRange::Week:
    case LogViewRange::Month:
        return dateTime.toString(QStringLiteral("MMM d hh:mm")).toStdString();
    case LogViewRange::Year:
        return dateTime.toString(QStringLiteral("yyyy-MM-dd")).toStdString();
    }
    return dateTime.toString(Qt::ISODate).toStdString();
}

inline std::string formatActionLogEntryValues(const QJsonObject& values)
{
    std::vector<std::string> fields;
    const QStringList preferredKeys = {QStringLiteral("status"), QStringLiteral("triggeredBy"), QStringLiteral("actorName"), QStringLiteral("sourceName")};
    for (const QString& key : preferredKeys) {
        if (values.contains(key)) {
            appendField(fields, key.toStdString(), jsonValueToString(values.value(key)));
        }
    }
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!preferredKeys.contains(it.key())) {
            appendField(fields, it.key().toStdString(), jsonValueToString(it.value()));
        }
    }
    return joinFields(fields);
}

inline std::optional<double> chartLogEntryValue(const QJsonValue& value)
{
    if (value.isBool()) {
        return value.toBool() ? 1.0 : 0.0;
    }
    if (value.isDouble()) {
        return value.toDouble();
    }
    if (value.isString()) {
        const QString text = value.toString();
        if (text.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
            return 1.0;
        }
        if (text.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
            return 0.0;
        }
        bool ok = false;
        const double parsed = text.toDouble(&ok);
        if (ok) {
            return parsed;
        }
    }
    return std::nullopt;
}

inline std::string formatStateLogEntryValue(const QJsonValue& value, const std::string& unitLabel, bool isBool)
{
    if (isBool) {
        const std::optional<double> numeric = chartLogEntryValue(value);
        if (numeric.has_value()) {
            return *numeric >= 0.5 ? "true" : "false";
        }
    }
    std::string text = jsonValueToString(value);
    if (!unitLabel.empty() && !text.empty()) {
        text += " " + unitLabel;
    }
    return text;
}

inline std::string settingsViewLabel(int view)
{
    switch (view) {
    case 0:
        return "Server info";
    case 1:
        return "Timezone";
    case 2:
        return "Update";
    case 3:
        return "Logging categories";
    case 4:
        return "Server interfaces";
    case 5:
        return "Modbus RTU";
    case 6:
        return "Shutdown";
    case 7:
        return "Restart";
    case 8:
        return "Reboot";
    }

    return "Settings";
}

inline std::string powerActionLabel(int action)
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

template<typename Container, typename Item, typename KeyFn>
void upsertByKey(Container& container, const Item& item, KeyFn keyFn)
{
    auto existing = std::find_if(container.begin(), container.end(), [&](const auto& existingItem) { return keyFn(existingItem) == keyFn(item); });
    if (existing == container.end()) {
        container.push_back(item);
    } else {
        *existing = item;
    }
}

template<typename Container, typename Key, typename KeyFn>
void eraseByKey(Container& container, const Key& key, KeyFn keyFn)
{
    container.erase(std::remove_if(container.begin(), container.end(), [&](const auto& item) { return keyFn(item) == key; }), container.end());
}

constexpr int timezoneSearchLineIndex = 4;
constexpr int timezoneListStartLineIndex = 7;
constexpr int loggingCategorySearchLineIndex = 2;
constexpr int loggingCategoryListStartLineIndex = 5;
constexpr int modbusRtuMasterListStartLineIndex = 4;
constexpr int updateProgressBarWidth = 20;

inline int nextFilterListDetailsLineIndex(int currentIndex, int direction, int searchLineIndex, int listStartLineIndex, int filteredCount)
{
    if (filteredCount <= 0) {
        return searchLineIndex;
    }

    const int firstResultLineIndex = listStartLineIndex;
    const int lastResultLineIndex = firstResultLineIndex + filteredCount - 1;

    if (currentIndex == searchLineIndex) {
        return direction > 0 ? firstResultLineIndex : lastResultLineIndex;
    }

    if (currentIndex < firstResultLineIndex) {
        return direction > 0 ? firstResultLineIndex : searchLineIndex;
    }

    if (currentIndex <= lastResultLineIndex) {
        if (direction > 0) {
            return currentIndex == lastResultLineIndex ? searchLineIndex : currentIndex + 1;
        }
        return currentIndex == firstResultLineIndex ? searchLineIndex : currentIndex - 1;
    }

    return direction > 0 ? searchLineIndex : lastResultLineIndex;
}

inline std::string progressBar(qint64 progress)
{
    const qint64 clampedProgress = std::clamp<qint64>(progress, 0, 100);
    const int filled = static_cast<int>((clampedProgress * updateProgressBarWidth) / 100);
    std::string bar = "[";
    for (int index = 0; index < updateProgressBarWidth; ++index) {
        bar += index < filled ? "#" : "-";
    }
    bar += "] ";
    bar += std::to_string(clampedProgress);
    bar += "%";
    return bar;
}

inline std::string loggingLevelLabel(api::LoggingLevel level)
{
    switch (level) {
    case api::LoggingLevel::LoggingLevelCritical:
        return "critical";
    case api::LoggingLevel::LoggingLevelWarning:
        return "warning";
    case api::LoggingLevel::LoggingLevelInfo:
        return "info";
    case api::LoggingLevel::LoggingLevelDebug:
        return "debug";
    }

    return "critical";
}

inline ftxui::Color loggingLevelColor(api::LoggingLevel level)
{
    switch (level) {
    case api::LoggingLevel::LoggingLevelCritical:
        return ftxui::Color::RGB(220, 64, 64);
    case api::LoggingLevel::LoggingLevelWarning:
        return ftxui::Color::RGB(220, 156, 48);
    case api::LoggingLevel::LoggingLevelInfo:
        return ftxui::Color::RGB(72, 176, 112);
    case api::LoggingLevel::LoggingLevelDebug:
        return ftxui::Color::RGB(64, 168, 220);
    }

    return ftxui::Color::RGB(220, 64, 64);
}

inline api::LoggingLevel cycleLoggingLevel(api::LoggingLevel level, int delta)
{
    constexpr std::array<api::LoggingLevel, 4> levels = {
        api::LoggingLevel::LoggingLevelCritical,
        api::LoggingLevel::LoggingLevelWarning,
        api::LoggingLevel::LoggingLevelInfo,
        api::LoggingLevel::LoggingLevelDebug,
    };

    int index = 0;
    for (int i = 0; i < static_cast<int>(levels.size()); ++i) {
        if (levels.at(i) == level) {
            index = i;
            break;
        }
    }

    index = (index + static_cast<int>(levels.size()) + delta) % static_cast<int>(levels.size());
    return levels.at(index);
}

inline void sortLoggingCategories(std::vector<api::LoggingCategory>& categories)
{
    std::sort(categories.begin(), categories.end(), [](const api::LoggingCategory& left, const api::LoggingCategory& right) {
        return QString::compare(left.name, right.name, Qt::CaseInsensitive) < 0;
    });
}

inline std::string serialPortLabel(const api::SerialPort& serialPort)
{
    std::string label = serialPort.systemLocation.toStdString();
    std::vector<std::string> details;
    if (!serialPort.description.isEmpty()) {
        details.push_back(serialPort.description.toStdString());
    }
    if (!serialPort.manufacturer.isEmpty()) {
        details.push_back(serialPort.manufacturer.toStdString());
    }
    if (!serialPort.serialNumber.isEmpty()) {
        details.push_back(serialPort.serialNumber.toStdString());
    }
    if (!details.empty()) {
        label += " (" + joinCommaSeparated(details) + ")";
    }
    return label;
}

inline std::string dataBitsLabel(api::SerialPortDataBits dataBits)
{
    switch (dataBits) {
    case api::SerialPortDataBits::SerialPortDataBitsData5:
        return "5";
    case api::SerialPortDataBits::SerialPortDataBitsData6:
        return "6";
    case api::SerialPortDataBits::SerialPortDataBitsData7:
        return "7";
    case api::SerialPortDataBits::SerialPortDataBitsData8:
        return "8";
    case api::SerialPortDataBits::SerialPortDataBitsUnknownDataBits:
        return "unknown";
    }
    return "unknown";
}

inline std::string parityLabel(api::SerialPortParity parity)
{
    switch (parity) {
    case api::SerialPortParity::SerialPortParityNoParity:
        return "none";
    case api::SerialPortParity::SerialPortParityEvenParity:
        return "even";
    case api::SerialPortParity::SerialPortParityOddParity:
        return "odd";
    case api::SerialPortParity::SerialPortParitySpaceParity:
        return "space";
    case api::SerialPortParity::SerialPortParityMarkParity:
        return "mark";
    case api::SerialPortParity::SerialPortParityUnknownParity:
        return "unknown";
    }
    return "unknown";
}

inline std::string stopBitsLabel(api::SerialPortStopBits stopBits)
{
    switch (stopBits) {
    case api::SerialPortStopBits::SerialPortStopBitsOneStop:
        return "1";
    case api::SerialPortStopBits::SerialPortStopBitsOneAndHalfStop:
        return "1.5";
    case api::SerialPortStopBits::SerialPortStopBitsTwoStop:
        return "2";
    case api::SerialPortStopBits::SerialPortStopBitsUnknownStopBits:
        return "unknown";
    }
    return "unknown";
}

inline const std::array<api::SerialPortDataBits, 4>& dataBitsOptions()
{
    static const std::array<api::SerialPortDataBits, 4> options = {
        api::SerialPortDataBits::SerialPortDataBitsData5,
        api::SerialPortDataBits::SerialPortDataBitsData6,
        api::SerialPortDataBits::SerialPortDataBitsData7,
        api::SerialPortDataBits::SerialPortDataBitsData8,
    };
    return options;
}

inline const std::array<quint64, 14>& baudrateOptions()
{
    static const std::array<quint64, 14> options = {
        1200,
        2400,
        4800,
        9600,
        19200,
        38400,
        57600,
        115200,
        230400,
        460800,
        500000,
        576000,
        921600,
        1000000,
    };
    return options;
}

inline const std::array<api::SerialPortParity, 5>& parityOptions()
{
    static const std::array<api::SerialPortParity, 5> options = {
        api::SerialPortParity::SerialPortParityNoParity,
        api::SerialPortParity::SerialPortParityEvenParity,
        api::SerialPortParity::SerialPortParityOddParity,
        api::SerialPortParity::SerialPortParitySpaceParity,
        api::SerialPortParity::SerialPortParityMarkParity,
    };
    return options;
}

inline const std::array<api::SerialPortStopBits, 3>& stopBitsOptions()
{
    static const std::array<api::SerialPortStopBits, 3> options = {
        api::SerialPortStopBits::SerialPortStopBitsOneStop,
        api::SerialPortStopBits::SerialPortStopBitsOneAndHalfStop,
        api::SerialPortStopBits::SerialPortStopBitsTwoStop,
    };
    return options;
}

template<typename T, size_t Size>
int optionIndex(const std::array<T, Size>& options, T value, int fallback)
{
    for (int index = 0; index < static_cast<int>(options.size()); ++index) {
        if (options.at(index) == value) {
            return index;
        }
    }
    return fallback;
}

inline int cycledIndex(int currentIndex, int count, int delta)
{
    if (count <= 0) {
        return 0;
    }
    return (currentIndex + count + delta) % count;
}


} // namespace nymea
