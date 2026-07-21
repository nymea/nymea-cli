// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

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
        if (m_configureParamTypes.empty()) {
            // No discovery params to edit: start the discovery immediately.
            m_configureDialogStatus = "Discovering things...";
            submitConfigureDialog();
            return;
        }
        m_configureDialogStatus = "Edit discovery params and press Enter to search.";
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
    auto search = ftxui::text("Search: " + (m_configureThingSelectionSearch.empty() ? std::string("<type to filter>") : m_configureThingSelectionSearch));
    if (m_focusArea == FocusArea::ConfigureThingSelectionSearch) {
        search = renderActiveField(std::move(search) | ftxui::inverted | ftxui::bold | ftxui::color(ftxui::Color::CyanLight), true, 28);
    }
    content.push_back(search);
    content.push_back(ftxui::separator());

    const std::vector<const api::Thing*> things = filteredConfigureThings();
    if (m_thingManager.things().empty()) {
        content.push_back(ftxui::text("No configured things available."));
    } else if (things.empty()) {
        content.push_back(ftxui::text("No things match the current filter."));
    } else {
        for (int index = 0; index < static_cast<int>(things.size()); ++index) {
            auto row = ftxui::text(" " + thingLabel(things.at(index)) + " ");
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
    return renderFocusedWindow(ftxui::text(title), ftxui::vbox(std::move(content)) | ftxui::vscroll_indicator | ftxui::frame,
                               m_focusArea == FocusArea::ConfigureThingSelectionSearch || m_focusArea == FocusArea::ConfigureThingSelection)
           | ftxui::reflect(m_configureDetailsBox);
}

} // namespace nymea
