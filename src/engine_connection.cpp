// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

namespace nymea {

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
    m_systemUpdateStatusStartedAt = std::chrono::steady_clock::now();
    m_systemPackagesLoaded = false;
    m_systemPackagesPending = false;
    m_systemPackages.clear();
    m_systemTimeZonesLoaded = false;
    m_systemTimeZonesPending = false;
    m_systemTimeZones.clear();
    m_systemTimeZoneSearch.clear();
    m_serverInterfacesLoaded = false;
    m_serverInterfacesPending = false;
    m_tcpServerConfigurations.clear();
    m_webSocketServerConfigurations.clear();
    m_webServerConfigurations.clear();
    m_tunnelProxyServerConfigurations.clear();
    m_serverInterfaceStatus.clear();
    m_serverInterfaceDialogMode = ServerInterfaceDialogMode::None;
    m_serverInterfaceRequestPending = false;
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

} // namespace nymea
