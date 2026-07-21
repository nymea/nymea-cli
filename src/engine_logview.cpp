// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

namespace nymea {

const api::StateType* Engine::selectedChartableStateType() const
{
    const api::Thing* thing = selectedThing();
    if (thing == nullptr) {
        return nullptr;
    }

    const api::ThingClass* thingClass = m_thingManager.thingClassForThing(*thing);
    if (thingClass == nullptr) {
        return nullptr;
    }

    const std::vector<ThingDetailEntry> detailEntries = buildThingDetailEntries(thing, thingClass);
    if (m_selectedThingDetailIndex < 0 || m_selectedThingDetailIndex >= static_cast<int>(detailEntries.size())) {
        return nullptr;
    }

    const ThingDetailEntry& selectedEntry = detailEntries.at(m_selectedThingDetailIndex);
    if (selectedEntry.type != ThingDetailEntry::Type::State) {
        return nullptr;
    }

    const api::State& state = thing->states.at(selectedEntry.index);
    const api::StateType* stateType = m_thingManager.stateTypeForThing(*thing, state);
    if (stateType == nullptr) {
        return nullptr;
    }

    // For now the chart is limited to temperature/humidity interface states and boolean states.
    const bool isTemperature = thingClass->interfaces.contains(QStringLiteral("temperaturesensor")) && stateType->name == QStringLiteral("temperature");
    const bool isHumidity = thingClass->interfaces.contains(QStringLiteral("humiditysensor")) && stateType->name == QStringLiteral("humidity");
    const bool isBool = stateType->type == api::BasicType::Bool;
    if (!isTemperature && !isHumidity && !isBool) {
        return nullptr;
    }

    return stateType;
}

bool Engine::openSelectedLogView()
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
    if (selectedEntry.type == ThingDetailEntry::Type::State) {
        const api::State& state = thing->states.at(selectedEntry.index);
        const api::StateType* stateType = m_thingManager.stateTypeForThing(*thing, state);
        if (stateType == nullptr) {
            return false;
        }
        // Only states with logging enabled can be opened (older servers that do not report the set are assumed available).
        const bool logged = !thing->loggedStateTypeIds.has_value() || thing->loggedStateTypeIds->contains(stateType->id);
        if (!logged) {
            return false;
        }
        m_logView.isAction = false;
        m_logView.chartable = selectedChartableStateType() != nullptr;
        m_logView.typeId = stateType->id;
        m_logView.typeName = stateType->name;
        m_logView.typeLabel = firstNonEmpty({stateType->displayName.toStdString(), stateType->name.toStdString(), "State"});
        m_logView.unitLabel = stateType->unit.has_value() ? prettyUnit(*stateType->unit) : std::string();
        m_logView.isBool = stateType->type == api::BasicType::Bool;
    } else if (selectedEntry.type == ThingDetailEntry::Type::Action) {
        const api::ActionType* actionType = m_thingManager.actionTypeForThing(*thing, selectedEntry.index);
        if (actionType == nullptr) {
            return false;
        }
        m_logView.isAction = true;
        m_logView.chartable = false;
        m_logView.typeId = actionType->id;
        m_logView.typeName = actionType->name;
        m_logView.typeLabel = firstNonEmpty({actionType->displayName.toStdString(), actionType->name.toStdString(), "Action"});
        m_logView.unitLabel.clear();
        m_logView.isBool = false;
    } else {
        return false;
    }

    m_showThingDetailInspector = false;
    m_logView.followLatest = false;
    m_logView.visible = true;
    m_logView.thingId = thing->id;
    m_logView.thingLabel = thingLabel(thing);
    m_logView.range = LogViewRange::Day;
    m_logView.windowEnd = QDateTime::currentDateTime();
    m_logView.samples.clear();
    m_logView.listEntries.clear();
    m_logView.listSelectionIndex = 0;
    m_logView.status.clear();
    m_focusArea = FocusArea::LogView;
    fetchLogViewData();
    return true;
}

void Engine::closeLogView()
{
    const quint64 generation = m_logView.fetchGeneration + 1;
    m_logView = LogViewModel{};
    m_logView.fetchGeneration = generation;
    if (m_focusArea == FocusArea::LogView) {
        m_focusArea = FocusArea::ThingDetails;
    }
}

void Engine::setLogViewRange(LogViewRange range)
{
    m_logView.range = range;
    m_logView.windowEnd = QDateTime::currentDateTime();
    fetchLogViewData();
}

void Engine::stepLogViewWindow(int direction)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime windowStart = chartWindowStart(m_logView.range, m_logView.windowEnd);
    const qint64 stepMs = (m_logView.windowEnd.toMSecsSinceEpoch() - windowStart.toMSecsSinceEpoch()) / 4;
    QDateTime candidate = m_logView.windowEnd.addMSecs(direction * stepMs);
    if (candidate > now) {
        candidate = now;
    }
    if (candidate == m_logView.windowEnd) {
        return;
    }
    m_logView.windowEnd = candidate;
    fetchLogViewData();
}

void Engine::fetchLogViewData()
{
    const quint64 generation = ++m_logView.fetchGeneration;
    const QDateTime windowStart = chartWindowStart(m_logView.range, m_logView.windowEnd);

    api::LoggingGetLogEntriesParams params;
    // The server registers state/action log sources with the braced QUuid form.
    const QString sourcePrefix = m_logView.isAction ? QStringLiteral("action-") : QStringLiteral("state-");
    params.sources = QList<QString>{sourcePrefix + m_logView.thingId.toString() + QStringLiteral("-") + m_logView.typeName};
    params.startTime = static_cast<quint64>(windowStart.toMSecsSinceEpoch());
    params.endTime = static_cast<quint64>(m_logView.windowEnd.toMSecsSinceEpoch());
    if (m_logView.isAction || !m_logView.chartable) {
        // Actions and non-chartable states: fetch the most recent raw entries for the list.
        params.sampleRate = api::SampleRate::SampleRateAny;
        params.sortOrder = api::SortOrder::DescendingOrder;
        params.limit = 500;
    } else if (m_logView.isBool) {
        // Bool states are logged discretely (on change); fetch one extra range back so the level at the window start is known.
        params.startTime = static_cast<quint64>(chartWindowStart(m_logView.range, windowStart).toMSecsSinceEpoch());
        params.sampleRate = api::SampleRate::SampleRateAny;
        params.sortOrder = api::SortOrder::AscendingOrder;
    } else {
        params.sampleRate = sampleRateForChartRange(m_logView.range);
        params.sortOrder = api::SortOrder::AscendingOrder;
    }

    m_logView.fetchPending = true;
    m_logView.fetchStartedAt = std::chrono::steady_clock::now();
    m_logView.status.clear();
    observeReply(m_client.sendRequest(api::LoggingGetLogEntriesMethod::methodName(), params.toJson()),
                 [this, generation](const QJsonObject& message, const QString& transportError) { handleLogViewReply(generation, message, transportError); });
}

void Engine::handleLogViewReply(quint64 generation, const QJsonObject& message, const QString& transportError)
{
    if (!m_logView.visible || generation != m_logView.fetchGeneration) {
        return;
    }

    m_logView.fetchPending = false;
    if (!transportError.isEmpty()) {
        m_logView.status = "Failed to load log entries: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        closeLogView();
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_logView.status = "Log entries request returned an error.";
        return;
    }

    const api::LoggingGetLogEntriesResponse response = api::LoggingGetLogEntriesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_logView.samples.clear();
    m_logView.listEntries.clear();
    m_logView.listSelectionIndex = 0;

    if (response.logEntries.has_value()) {
        m_logView.listEntries.reserve(response.logEntries->size());
        m_logView.samples.reserve(response.logEntries->size());
        for (const api::LogEntry& entry : *response.logEntries) {
            const qint64 timestamp = static_cast<qint64>(entry.timestamp);
            if (m_logView.isAction) {
                m_logView.listEntries.emplace_back(timestamp, formatActionLogEntryValues(entry.values));
            } else {
                const QJsonValue value = entry.values.value(m_logView.typeName);
                m_logView.listEntries.emplace_back(timestamp, formatStateLogEntryValue(value, m_logView.unitLabel, m_logView.isBool));
                if (m_logView.chartable) {
                    const std::optional<double> numeric = chartLogEntryValue(value);
                    if (numeric.has_value()) {
                        m_logView.samples.emplace_back(timestamp, *numeric);
                    }
                }
            }
        }
    }

    // Present the list oldest-first so the newest entry is at the bottom (follow/jump-to-bottom target).
    std::stable_sort(m_logView.listEntries.begin(), m_logView.listEntries.end(),
                     [](const auto& left, const auto& right) { return left.first < right.first; });
    std::sort(m_logView.samples.begin(), m_logView.samples.end());

    const bool hasData = m_logView.chartable ? !m_logView.samples.empty() : !m_logView.listEntries.empty();
    if (!hasData) {
        m_logView.status = "No log data in this range.";
    }
    if (!m_logView.listEntries.empty()) {
        m_logView.listSelectionIndex = static_cast<int>(m_logView.listEntries.size()) - 1;
    }
}

void Engine::appendLiveLogEntry(const api::LogEntry& entry)
{
    constexpr size_t maxEntries = 2000;
    const qint64 timestamp = static_cast<qint64>(entry.timestamp);
    if (m_logView.isAction) {
        m_logView.listEntries.emplace_back(timestamp, formatActionLogEntryValues(entry.values));
    } else {
        const QJsonValue value = entry.values.value(m_logView.typeName);
        m_logView.listEntries.emplace_back(timestamp, formatStateLogEntryValue(value, m_logView.unitLabel, m_logView.isBool));
        if (m_logView.chartable) {
            const std::optional<double> numeric = chartLogEntryValue(value);
            if (numeric.has_value()) {
                m_logView.samples.emplace_back(timestamp, *numeric);
            }
        }
    }

    if (m_logView.listEntries.size() > maxEntries) {
        const size_t drop = m_logView.listEntries.size() - maxEntries;
        m_logView.listEntries.erase(m_logView.listEntries.begin(), m_logView.listEntries.begin() + drop);
        m_logView.listSelectionIndex = std::max(0, m_logView.listSelectionIndex - static_cast<int>(drop));
    }
    if (m_logView.samples.size() > maxEntries) {
        m_logView.samples.erase(m_logView.samples.begin(), m_logView.samples.begin() + (m_logView.samples.size() - maxEntries));
    }

    if (m_logView.followLatest) {
        m_logView.windowEnd = QDateTime::currentDateTime();
        m_logView.listSelectionIndex = static_cast<int>(m_logView.listEntries.size()) - 1;
    } else if (m_logView.listSelectionIndex >= static_cast<int>(m_logView.listEntries.size())) {
        m_logView.listSelectionIndex = std::max(0, static_cast<int>(m_logView.listEntries.size()) - 1);
    }
    m_logView.status.clear();
}

bool Engine::logViewLoggingEnabled() const
{
    const api::Thing* thing = m_thingManager.thingById(m_logView.thingId);
    if (thing == nullptr) {
        return true;
    }
    const std::optional<QList<QUuid>>& loggedTypeIds = m_logView.isAction ? thing->loggedActionTypeIds : thing->loggedStateTypeIds;
    if (!loggedTypeIds.has_value()) {
        // Older servers do not report logged types; assume logging is available.
        return true;
    }
    return loggedTypeIds->contains(m_logView.typeId);
}

void Engine::toggleLogViewLogging()
{
    if (m_logView.setLoggingPending) {
        return;
    }

    const bool enable = !logViewLoggingEnabled();
    QString methodName;
    QJsonObject paramsJson;
    if (m_logView.isAction) {
        api::IntegrationsSetActionLoggingParams params;
        params.thingId = m_logView.thingId;
        params.actionTypeId = m_logView.typeId;
        params.enabled = enable;
        methodName = api::IntegrationsSetActionLoggingMethod::methodName();
        paramsJson = params.toJson();
    } else {
        api::IntegrationsSetStateLoggingParams params;
        params.thingId = m_logView.thingId;
        params.stateTypeId = m_logView.typeId;
        params.enabled = enable;
        methodName = api::IntegrationsSetStateLoggingMethod::methodName();
        paramsJson = params.toJson();
    }

    m_logView.setLoggingPending = true;
    m_logView.setLoggingStartedAt = std::chrono::steady_clock::now();
    m_logView.status = enable ? "Enabling logging..." : "Disabling logging...";
    observeReply(m_client.sendRequest(methodName, paramsJson),
                 [this, thingId = m_logView.thingId, typeId = m_logView.typeId, enable](const QJsonObject& message, const QString& transportError) {
                     handleSetLoggingReply(thingId, typeId, enable, message, transportError);
                 });
}

void Engine::handleSetLoggingReply(const QUuid& thingId, const QUuid& typeId, bool enabled, const QJsonObject& message, const QString& transportError)
{
    if (!m_logView.visible || m_logView.thingId != thingId || m_logView.typeId != typeId) {
        return;
    }

    m_logView.setLoggingPending = false;
    if (!transportError.isEmpty()) {
        m_logView.status = "Failed to update logging: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        closeLogView();
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_logView.status = "Set logging request returned an error.";
        return;
    }

    // SetStateLogging and SetActionLogging responses share the same shape.
    const api::IntegrationsSetStateLoggingResponse response = api::IntegrationsSetStateLoggingResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.thingError != api::ThingError::ThingErrorNoError) {
        m_logView.status = "Failed to update logging: " + api::toString(response.thingError).toStdString();
        return;
    }

    m_logView.status = enabled ? "Logging enabled. New entries will appear as they are logged." : "Logging disabled.";
}

ftxui::Element Engine::renderLogView() const
{
    constexpr int yLabelWidth = 12;
    const QDateTime windowStart = chartWindowStart(m_logView.range, m_logView.windowEnd);
    const qint64 startMs = windowStart.toMSecsSinceEpoch();
    const qint64 endMs = m_logView.windowEnd.toMSecsSinceEpoch();

    const bool loggingEnabled = logViewLoggingEnabled();

    ftxui::Elements infoRow;
    infoRow.push_back(ftxui::text("Range: " + chartRangeLabel(m_logView.range)) | ftxui::bold);
    infoRow.push_back(ftxui::text("  Logging: "));
    if (m_logView.setLoggingPending) {
        infoRow.push_back(ftxui::text(busyIndicator(m_logView.setLoggingStartedAt) + " updating") | ftxui::color(ftxui::Color::CyanLight));
    } else if (loggingEnabled) {
        infoRow.push_back(ftxui::text("on") | ftxui::color(ftxui::Color::Green));
    } else {
        infoRow.push_back(ftxui::text("off") | ftxui::color(ftxui::Color::Yellow));
    }
    infoRow.push_back(ftxui::text("  Follow: "));
    infoRow.push_back(ftxui::text(m_logView.followLatest ? "on" : "off")
                      | ftxui::color(m_logView.followLatest ? ftxui::Color::Green : ftxui::Color::GrayDark));
    infoRow.push_back(ftxui::filler());
    if (m_logView.fetchPending) {
        infoRow.push_back(ftxui::text(busyIndicator(m_logView.fetchStartedAt) + " Fetching...  ") | ftxui::color(ftxui::Color::CyanLight));
    }
    infoRow.push_back(ftxui::text(windowStart.toString(QStringLiteral("yyyy-MM-dd hh:mm")).toStdString() + " -> "
                                  + m_logView.windowEnd.toString(QStringLiteral("yyyy-MM-dd hh:mm")).toStdString()));

    auto buildLogList = [this]() {
        ftxui::Elements rows;
        rows.reserve(m_logView.listEntries.size());
        for (int index = 0; index < static_cast<int>(m_logView.listEntries.size()); ++index) {
            const auto& entry = m_logView.listEntries.at(index);
            const std::string timestamp = QDateTime::fromMSecsSinceEpoch(entry.first).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")).toStdString();
            ftxui::Element row = ftxui::hbox({
                ftxui::text(timestamp) | ftxui::color(ftxui::Color::CyanLight),
                ftxui::text("  "),
                ftxui::paragraph(entry.second) | ftxui::flex,
            });
            if (index == m_logView.listSelectionIndex) {
                row = row | ftxui::inverted | ftxui::focus;
            }
            rows.push_back(std::move(row));
        }
        return ftxui::vbox(std::move(rows)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::flex;
    };

    ftxui::Element body;
    bool statusShownAsPlaceholder = false;
    const bool hasData = m_logView.chartable ? !m_logView.samples.empty() : !m_logView.listEntries.empty();
    if (!hasData) {
        std::string placeholder;
        if (m_logView.fetchPending) {
            placeholder = "Fetching data...";
        } else if (!loggingEnabled && !m_logView.setLoggingPending) {
            placeholder = std::string("Logging is disabled for this ") + (m_logView.isAction ? "action" : "state") + ". Press x to enable logging.";
        } else {
            placeholder = firstNonEmpty({m_logView.status, "No data."});
            statusShownAsPlaceholder = !m_logView.status.empty();
        }
        body = ftxui::vbox({
                   ftxui::filler(),
                   ftxui::hbox({ftxui::filler(), ftxui::text(placeholder) | ftxui::dim, ftxui::filler()}),
                   ftxui::filler(),
               })
               | ftxui::flex;
    } else if (!m_logView.chartable) {
        // Actions and non-chartable states: browsable list only.
        body = buildLogList();
    } else {
        double minValue = m_logView.samples.front().second;
        double maxValue = minValue;
        for (const auto& sample : m_logView.samples) {
            minValue = std::min(minValue, sample.second);
            maxValue = std::max(maxValue, sample.second);
        }
        if (m_logView.isBool) {
            minValue = 0.0;
            maxValue = 1.0;
        } else if (maxValue - minValue < 1e-9) {
            const double padding = std::max(0.5, std::abs(maxValue) * 0.05);
            minValue -= padding;
            maxValue += padding;
        }

        auto graphFunction = [samples = m_logView.samples, startMs, endMs, minValue, maxValue](int width, int height) {
            std::vector<int> output(static_cast<size_t>(std::max(width, 0)), 0);
            if (width <= 0 || height <= 0 || samples.empty() || endMs <= startMs) {
                return output;
            }
            size_t cursor = 0;
            for (int x = 0; x < width; ++x) {
                const qint64 columnEnd = startMs + ((x + 1) * (endMs - startMs)) / width;
                while (cursor + 1 < samples.size() && samples[cursor + 1].first <= columnEnd) {
                    ++cursor;
                }
                if (samples[cursor].first > columnEnd) {
                    continue;
                }
                const double normalized = (samples[cursor].second - minValue) / (maxValue - minValue);
                output[static_cast<size_t>(x)] = 1 + static_cast<int>(std::lround(normalized * (height - 1)));
            }
            return output;
        };

        auto valueLabel = [this](double value) {
            if (m_logView.isBool) {
                return std::string(value >= 0.5 ? "true" : "false");
            }
            std::string label = QString::number(value, 'f', 1).toStdString();
            if (!m_logView.unitLabel.empty()) {
                label += " " + m_logView.unitLabel;
            }
            return label;
        };
        auto midLabel = m_logView.isBool ? ftxui::text("") : ftxui::text(valueLabel((minValue + maxValue) / 2.0)) | ftxui::dim;
        auto yAxis = ftxui::vbox({
                         ftxui::text(valueLabel(maxValue)),
                         ftxui::filler(),
                         std::move(midLabel),
                         ftxui::filler(),
                         ftxui::text(valueLabel(minValue)),
                     })
                     | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, yLabelWidth);
        auto xAxis = ftxui::hbox({
            ftxui::text(std::string(yLabelWidth + 1, ' ')),
            ftxui::text(formatChartTimestamp(startMs, m_logView.range)),
            ftxui::filler(),
            ftxui::text(formatChartTimestamp((startMs + endMs) / 2, m_logView.range)) | ftxui::dim,
            ftxui::filler(),
            ftxui::text(formatChartTimestamp(endMs, m_logView.range)),
        });
        ftxui::Element graphBody = ftxui::vbox({
                                       ftxui::hbox({
                                           yAxis,
                                           ftxui::separator(),
                                           ftxui::graph(std::move(graphFunction)) | ftxui::flex | ftxui::color(ftxui::Color::CyanLight),
                                       }) | ftxui::flex,
                                       xAxis,
                                   })
                                   | ftxui::flex;
        // Show the log-entry list to the right of the graph.
        body = ftxui::hbox({
                   std::move(graphBody) | ftxui::flex,
                   ftxui::separator(),
                   buildLogList() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 48),
               })
               | ftxui::flex;
    }

    ftxui::Elements content;
    content.push_back(ftxui::hbox(std::move(infoRow)));
    content.push_back(ftxui::separator());
    content.push_back(body);
    if (!m_logView.status.empty() && !statusShownAsPlaceholder) {
        content.push_back(ftxui::text(m_logView.status) | ftxui::color(ftxui::Color::Yellow));
    }
    const std::string keyHints = "Keys: Up/Down scroll, f follow latest, h/d/w/m/y range, Left/Right move 1/4 window, x toggle logging, Esc/q close";
    content.push_back(ftxui::text(keyHints) | ftxui::dim);

    const std::string title = (m_logView.isAction ? "Action log: " : "State history: ") + m_logView.thingLabel + " / " + m_logView.typeLabel;
    return renderFocusedWindow(ftxui::text(title), ftxui::vbox(std::move(content)) | ftxui::flex, m_focusArea == FocusArea::LogView) | ftxui::flex;
}

} // namespace nymea
