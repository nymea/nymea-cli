// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

namespace nymea {

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

ftxui::Element Engine::renderThingList() const
{
    ftxui::Elements lines;
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
            const bool logged = stateType != nullptr && thing->loggedStateTypeIds.has_value() && thing->loggedStateTypeIds->contains(stateType->id);
            ftxui::Element indicator = logged ? (ftxui::text(" L ") | ftxui::color(ftxui::Color::Black) | ftxui::bgcolor(ftxui::Color::Green)) : ftxui::text("   ");
            browserRows.push_back(ftxui::hbox({
                std::move(indicator),
                renderTwoColumnRow(label, renderValueCell(state.value, basicType, unit), isSelected, m_focusArea == FocusArea::ThingDetails) | ftxui::xflex,
            }));
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
            if (selectedChartableStateType() != nullptr) {
                lines.push_back(ftxui::separator());
                lines.push_back(ftxui::text("l opens history chart") | ftxui::dim);
            }
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
            lines.push_back(ftxui::text("l opens action log") | ftxui::dim);
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

} // namespace nymea
