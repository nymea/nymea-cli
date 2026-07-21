// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

namespace nymea {

void Engine::ensureServerInterfacesLoaded()
{
    if (m_serverInterfacesLoaded || m_serverInterfacesPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_serverInterfacesPending = true;
    observeReply(m_client.sendRequest(api::ConfigurationGetConfigurationsMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchServerInterfacesReply(message, transportError); });
}

int Engine::serverInterfaceCount() const
{
    return static_cast<int>(m_tcpServerConfigurations.size() + m_webSocketServerConfigurations.size() + m_webServerConfigurations.size() + m_tunnelProxyServerConfigurations.size());
}

std::optional<Engine::ServerInterfaceSelection> Engine::selectedServerInterface() const
{
    int index = m_settingsDetailsLineIndex;
    if (index < 0) {
        return std::nullopt;
    }

    if (index < static_cast<int>(m_tcpServerConfigurations.size())) {
        return ServerInterfaceSelection{ServerInterfaceType::Tcp, index};
    }
    index -= static_cast<int>(m_tcpServerConfigurations.size());
    if (index < static_cast<int>(m_webSocketServerConfigurations.size())) {
        return ServerInterfaceSelection{ServerInterfaceType::WebSocket, index};
    }
    index -= static_cast<int>(m_webSocketServerConfigurations.size());
    if (index < static_cast<int>(m_webServerConfigurations.size())) {
        return ServerInterfaceSelection{ServerInterfaceType::WebServer, index};
    }
    index -= static_cast<int>(m_webServerConfigurations.size());
    if (index < static_cast<int>(m_tunnelProxyServerConfigurations.size())) {
        return ServerInterfaceSelection{ServerInterfaceType::TunnelProxy, index};
    }
    return std::nullopt;
}

void Engine::clampServerInterfaceSelection()
{
    const int count = serverInterfaceCount();
    if (count <= 0) {
        m_settingsDetailsLineIndex = 0;
        return;
    }
    if (m_settingsDetailsLineIndex < 0) {
        m_settingsDetailsLineIndex = 0;
    } else if (m_settingsDetailsLineIndex >= count) {
        m_settingsDetailsLineIndex = count - 1;
    }
}

std::string Engine::serverInterfaceTypeLabel(ServerInterfaceType type) const
{
    switch (type) {
    case ServerInterfaceType::Tcp:
        return "TCP server";
    case ServerInterfaceType::WebSocket:
        return "WebSocket server";
    case ServerInterfaceType::WebServer:
        return "WebServer";
    case ServerInterfaceType::TunnelProxy:
        return "Tunnel Proxy server";
    }
    return "Server interface";
}

int Engine::serverInterfaceDialogFieldCount() const
{
    if (m_serverInterfaceDialogMode == ServerInterfaceDialogMode::None || m_serverInterfaceDialogMode == ServerInterfaceDialogMode::RemoveConfirm) {
        return 0;
    }

    int count = m_serverInterfaceDialogMode == ServerInterfaceDialogMode::Add ? 5 : 4;
    if (m_serverInterfaceDialogType == ServerInterfaceType::WebServer || m_serverInterfaceDialogType == ServerInterfaceType::TunnelProxy) {
        ++count;
    }
    return count;
}

void Engine::openAddServerInterfaceDialog()
{
    m_previousFocusArea = m_focusArea;
    m_focusArea = FocusArea::ServerInterfaceDialog;
    m_serverInterfaceDialogMode = ServerInterfaceDialogMode::Add;
    m_serverInterfaceRequestPending = false;
    m_serverInterfaceDialogType = ServerInterfaceType::Tcp;
    m_serverInterfaceDialogFieldIndex = 0;
    m_serverInterfaceDialogId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    m_serverInterfaceDialogAddress = "0.0.0.0";
    m_serverInterfaceDialogPort = "2223";
    m_serverInterfaceDialogSslEnabled = false;
    m_serverInterfaceDialogAuthenticationEnabled = false;
    m_serverInterfaceDialogPublicFolder.clear();
    m_serverInterfaceDialogIgnoreSslErrors = false;
    m_serverInterfaceStatus = "Add server interface.";
}

void Engine::openEditServerInterfaceDialog()
{
    const std::optional<ServerInterfaceSelection> selection = selectedServerInterface();
    if (!selection.has_value()) {
        m_serverInterfaceStatus = "No server interface selected.";
        return;
    }

    m_previousFocusArea = m_focusArea;
    m_focusArea = FocusArea::ServerInterfaceDialog;
    m_serverInterfaceDialogMode = ServerInterfaceDialogMode::Edit;
    m_serverInterfaceRequestPending = false;
    m_serverInterfaceDialogType = selection->type;
    m_serverInterfaceDialogFieldIndex = 0;
    m_serverInterfaceDialogPublicFolder.clear();
    m_serverInterfaceDialogIgnoreSslErrors = false;

    auto copyCommon = [this](const auto& configuration) {
        m_serverInterfaceDialogId = configuration.id.toStdString();
        m_serverInterfaceDialogAddress = configuration.address.toStdString();
        m_serverInterfaceDialogPort = QString::number(configuration.port).toStdString();
        m_serverInterfaceDialogSslEnabled = configuration.sslEnabled;
        m_serverInterfaceDialogAuthenticationEnabled = configuration.authenticationEnabled;
    };

    switch (selection->type) {
    case ServerInterfaceType::Tcp:
        copyCommon(m_tcpServerConfigurations.at(selection->index));
        break;
    case ServerInterfaceType::WebSocket:
        copyCommon(m_webSocketServerConfigurations.at(selection->index));
        break;
    case ServerInterfaceType::WebServer: {
        const api::WebServerConfiguration& configuration = m_webServerConfigurations.at(selection->index);
        copyCommon(configuration);
        m_serverInterfaceDialogPublicFolder = configuration.publicFolder.toStdString();
        break;
    }
    case ServerInterfaceType::TunnelProxy: {
        const api::TunnelProxyServerConfiguration& configuration = m_tunnelProxyServerConfigurations.at(selection->index);
        copyCommon(configuration);
        m_serverInterfaceDialogIgnoreSslErrors = configuration.ignoreSslErrors;
        break;
    }
    }
    m_serverInterfaceStatus = "Edit " + serverInterfaceTypeLabel(selection->type) + " " + m_serverInterfaceDialogId + ".";
}

void Engine::openRemoveServerInterfaceDialog()
{
    const std::optional<ServerInterfaceSelection> selection = selectedServerInterface();
    if (!selection.has_value()) {
        m_serverInterfaceStatus = "No server interface selected.";
        return;
    }

    openEditServerInterfaceDialog();
    m_serverInterfaceDialogMode = ServerInterfaceDialogMode::RemoveConfirm;
    m_serverInterfaceRequestPending = false;
    m_serverInterfaceStatus = "Confirm removing " + serverInterfaceTypeLabel(selection->type) + ".";
}

void Engine::closeServerInterfaceDialog()
{
    m_serverInterfaceDialogMode = ServerInterfaceDialogMode::None;
    m_serverInterfaceRequestPending = false;
    m_serverInterfaceDialogFieldIndex = 0;
    m_focusArea = m_previousFocusArea == FocusArea::ServerInterfaceDialog ? FocusArea::SettingsDetails : m_previousFocusArea;
}

bool Engine::submitServerInterfaceDialog()
{
    if (m_serverInterfaceRequestPending || m_serverInterfaceDialogMode == ServerInterfaceDialogMode::None) {
        return true;
    }

    if (m_serverInterfaceDialogMode == ServerInterfaceDialogMode::RemoveConfirm) {
        const QString id = QString::fromStdString(m_serverInterfaceDialogId);
        if (id.trimmed().isEmpty()) {
            m_serverInterfaceStatus = "Interface id must not be empty.";
            return true;
        }

        m_serverInterfaceRequestPending = true;
        m_serverInterfaceStatus = "Removing " + serverInterfaceTypeLabel(m_serverInterfaceDialogType) + "...";
        switch (m_serverInterfaceDialogType) {
        case ServerInterfaceType::Tcp: {
            api::ConfigurationDeleteTcpServerConfigurationParams request;
            request.id = id;
            observeReply(m_client.sendRequest(api::ConfigurationDeleteTcpServerConfigurationMethod::methodName(), request.toJson()),
                         [this](const QJsonObject& message, const QString& transportError) { handleDeleteServerInterfaceReply(message, transportError, ServerInterfaceType::Tcp); });
            break;
        }
        case ServerInterfaceType::WebSocket: {
            api::ConfigurationDeleteWebSocketServerConfigurationParams request;
            request.id = id;
            observeReply(m_client.sendRequest(api::ConfigurationDeleteWebSocketServerConfigurationMethod::methodName(), request.toJson()),
                         [this](const QJsonObject& message, const QString& transportError) {
                             handleDeleteServerInterfaceReply(message, transportError, ServerInterfaceType::WebSocket);
                         });
            break;
        }
        case ServerInterfaceType::WebServer: {
            api::ConfigurationDeleteWebServerConfigurationParams request;
            request.id = id;
            observeReply(m_client.sendRequest(api::ConfigurationDeleteWebServerConfigurationMethod::methodName(), request.toJson()),
                         [this](const QJsonObject& message, const QString& transportError) {
                             handleDeleteServerInterfaceReply(message, transportError, ServerInterfaceType::WebServer);
                         });
            break;
        }
        case ServerInterfaceType::TunnelProxy: {
            api::ConfigurationDeleteTunnelProxyServerConfigurationParams request;
            request.id = id;
            observeReply(m_client.sendRequest(api::ConfigurationDeleteTunnelProxyServerConfigurationMethod::methodName(), request.toJson()),
                         [this](const QJsonObject& message, const QString& transportError) {
                             handleDeleteServerInterfaceReply(message, transportError, ServerInterfaceType::TunnelProxy);
                         });
            break;
        }
        }
        return true;
    }

    const QString id = QString::fromStdString(m_serverInterfaceDialogId).trimmed();
    const QString address = QString::fromStdString(m_serverInterfaceDialogAddress).trimmed();
    bool ok = false;
    const quint64 port = QString::fromStdString(m_serverInterfaceDialogPort).trimmed().toULongLong(&ok);
    if (id.isEmpty()) {
        m_serverInterfaceStatus = "Interface id must not be empty.";
        return true;
    }
    if (address.isEmpty()) {
        m_serverInterfaceStatus = "Address must not be empty.";
        return true;
    }
    if (!ok || port == 0 || port > 65535) {
        m_serverInterfaceStatus = "Port must be an integer between 1 and 65535.";
        return true;
    }

    m_serverInterfaceRequestPending = true;
    m_serverInterfaceStatus = "Saving " + serverInterfaceTypeLabel(m_serverInterfaceDialogType) + "...";
    switch (m_serverInterfaceDialogType) {
    case ServerInterfaceType::Tcp: {
        api::ConfigurationSetTcpServerConfigurationParams request;
        request.configuration.id = id;
        request.configuration.address = address;
        request.configuration.port = port;
        request.configuration.sslEnabled = m_serverInterfaceDialogSslEnabled;
        request.configuration.authenticationEnabled = m_serverInterfaceDialogAuthenticationEnabled;
        observeReply(m_client.sendRequest(api::ConfigurationSetTcpServerConfigurationMethod::methodName(), request.toJson()),
                     [this](const QJsonObject& message, const QString& transportError) { handleSetServerInterfaceReply(message, transportError, ServerInterfaceType::Tcp); });
        break;
    }
    case ServerInterfaceType::WebSocket: {
        api::ConfigurationSetWebSocketServerConfigurationParams request;
        request.configuration.id = id;
        request.configuration.address = address;
        request.configuration.port = port;
        request.configuration.sslEnabled = m_serverInterfaceDialogSslEnabled;
        request.configuration.authenticationEnabled = m_serverInterfaceDialogAuthenticationEnabled;
        observeReply(m_client.sendRequest(api::ConfigurationSetWebSocketServerConfigurationMethod::methodName(), request.toJson()),
                     [this](const QJsonObject& message, const QString& transportError) { handleSetServerInterfaceReply(message, transportError, ServerInterfaceType::WebSocket); });
        break;
    }
    case ServerInterfaceType::WebServer: {
        api::ConfigurationSetWebServerConfigurationParams request;
        request.configuration.id = id;
        request.configuration.address = address;
        request.configuration.port = port;
        request.configuration.sslEnabled = m_serverInterfaceDialogSslEnabled;
        request.configuration.authenticationEnabled = m_serverInterfaceDialogAuthenticationEnabled;
        request.configuration.publicFolder = QString::fromStdString(m_serverInterfaceDialogPublicFolder).trimmed();
        observeReply(m_client.sendRequest(api::ConfigurationSetWebServerConfigurationMethod::methodName(), request.toJson()),
                     [this](const QJsonObject& message, const QString& transportError) { handleSetServerInterfaceReply(message, transportError, ServerInterfaceType::WebServer); });
        break;
    }
    case ServerInterfaceType::TunnelProxy: {
        api::ConfigurationSetTunnelProxyServerConfigurationParams request;
        request.configuration.id = id;
        request.configuration.address = address;
        request.configuration.port = port;
        request.configuration.sslEnabled = m_serverInterfaceDialogSslEnabled;
        request.configuration.authenticationEnabled = m_serverInterfaceDialogAuthenticationEnabled;
        request.configuration.ignoreSslErrors = m_serverInterfaceDialogIgnoreSslErrors;
        observeReply(m_client.sendRequest(api::ConfigurationSetTunnelProxyServerConfigurationMethod::methodName(), request.toJson()),
                     [this](const QJsonObject& message, const QString& transportError) {
                         handleSetServerInterfaceReply(message, transportError, ServerInterfaceType::TunnelProxy);
                     });
        break;
    }
    }
    return true;
}

void Engine::handleFetchServerInterfacesReply(const QJsonObject& message, const QString& transportError)
{
    m_serverInterfacesPending = false;
    if (!transportError.isEmpty()) {
        m_serverInterfaceStatus = "Failed to load server interfaces: " + transportError.toStdString();
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
        m_serverInterfaceStatus = "Server interface request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_serverInterfaceStatus = "Server interface request returned an error.";
        return;
    }

    const api::ConfigurationGetConfigurationsResponse response = api::ConfigurationGetConfigurationsResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_tcpServerConfigurations.assign(response.tcpServerConfigurations.begin(), response.tcpServerConfigurations.end());
    m_webSocketServerConfigurations.assign(response.webSocketServerConfigurations.begin(), response.webSocketServerConfigurations.end());
    m_webServerConfigurations.assign(response.webServerConfigurations.begin(), response.webServerConfigurations.end());
    m_tunnelProxyServerConfigurations.assign(response.tunnelProxyServerConfigurations.begin(), response.tunnelProxyServerConfigurations.end());
    m_serverInterfacesLoaded = true;
    m_serverInterfaceStatus = "Loaded " + std::to_string(serverInterfaceCount()) + " server interfaces.";
    m_settingsWarning.clear();
    clampServerInterfaceSelection();
}

void Engine::handleSetServerInterfaceReply(const QJsonObject& message, const QString& transportError, ServerInterfaceType type)
{
    m_serverInterfaceRequestPending = false;
    if (!transportError.isEmpty()) {
        m_serverInterfaceStatus = "Saving " + serverInterfaceTypeLabel(type) + " failed: " + transportError.toStdString();
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
        m_serverInterfaceStatus = "Saving " + serverInterfaceTypeLabel(type) + " was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_serverInterfaceStatus = "Saving " + serverInterfaceTypeLabel(type) + " returned an error.";
        return;
    }

    api::ConfigurationError configurationError = api::ConfigurationError::ConfigurationErrorNoError;
    switch (type) {
    case ServerInterfaceType::Tcp:
        configurationError = api::ConfigurationSetTcpServerConfigurationResponse::fromJson(message.value(QStringLiteral("params")).toObject()).configurationError;
        break;
    case ServerInterfaceType::WebSocket:
        configurationError = api::ConfigurationSetWebSocketServerConfigurationResponse::fromJson(message.value(QStringLiteral("params")).toObject()).configurationError;
        break;
    case ServerInterfaceType::WebServer:
        configurationError = api::ConfigurationSetWebServerConfigurationResponse::fromJson(message.value(QStringLiteral("params")).toObject()).configurationError;
        break;
    case ServerInterfaceType::TunnelProxy:
        configurationError = api::ConfigurationSetTunnelProxyServerConfigurationResponse::fromJson(message.value(QStringLiteral("params")).toObject()).configurationError;
        break;
    }
    if (configurationError != api::ConfigurationError::ConfigurationErrorNoError) {
        m_serverInterfaceStatus = "Saving " + serverInterfaceTypeLabel(type) + " failed: " + api::toString(configurationError).toStdString();
        return;
    }

    m_serverInterfaceStatus = "Saved " + serverInterfaceTypeLabel(type) + ".";
    closeServerInterfaceDialog();
    m_serverInterfacesLoaded = false;
    ensureServerInterfacesLoaded();
}

void Engine::handleDeleteServerInterfaceReply(const QJsonObject& message, const QString& transportError, ServerInterfaceType type)
{
    m_serverInterfaceRequestPending = false;
    if (!transportError.isEmpty()) {
        m_serverInterfaceStatus = "Removing " + serverInterfaceTypeLabel(type) + " failed: " + transportError.toStdString();
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
        m_serverInterfaceStatus = "Removing " + serverInterfaceTypeLabel(type) + " was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_serverInterfaceStatus = "Removing " + serverInterfaceTypeLabel(type) + " returned an error.";
        return;
    }

    api::ConfigurationError configurationError = api::ConfigurationError::ConfigurationErrorNoError;
    switch (type) {
    case ServerInterfaceType::Tcp:
        configurationError = api::ConfigurationDeleteTcpServerConfigurationResponse::fromJson(message.value(QStringLiteral("params")).toObject()).configurationError;
        break;
    case ServerInterfaceType::WebSocket:
        configurationError = api::ConfigurationDeleteWebSocketServerConfigurationResponse::fromJson(message.value(QStringLiteral("params")).toObject()).configurationError;
        break;
    case ServerInterfaceType::WebServer:
        configurationError = api::ConfigurationDeleteWebServerConfigurationResponse::fromJson(message.value(QStringLiteral("params")).toObject()).configurationError;
        break;
    case ServerInterfaceType::TunnelProxy:
        configurationError = api::ConfigurationDeleteTunnelProxyServerConfigurationResponse::fromJson(message.value(QStringLiteral("params")).toObject()).configurationError;
        break;
    }
    if (configurationError != api::ConfigurationError::ConfigurationErrorNoError) {
        m_serverInterfaceStatus = "Removing " + serverInterfaceTypeLabel(type) + " failed: " + api::toString(configurationError).toStdString();
        return;
    }

    m_serverInterfaceStatus = "Removed " + serverInterfaceTypeLabel(type) + ".";
    closeServerInterfaceDialog();
    m_serverInterfacesLoaded = false;
    ensureServerInterfacesLoaded();
}

} // namespace nymea
