// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

namespace nymea {

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

    if (notificationName == api::LoggingLogEntryAddedNotification::notificationName()) {
        if (m_logView.visible) {
            const api::LoggingLogEntryAddedNotificationParams notification = api::LoggingLogEntryAddedNotificationParams::fromJson(params);
            const api::LogEntry& entry = notification.logEntry;
            const QString source = (m_logView.isAction ? QStringLiteral("action-") : QStringLiteral("state-")) + m_logView.thingId.toString() + QStringLiteral("-")
                                   + m_logView.typeName;
            if (entry.source == source) {
                appendLiveLogEntry(entry);
            }
        }
        return;
    }

    if (notificationName == api::DebugLoggingCategoryLevelChangedNotification::notificationName()) {
        const api::DebugLoggingCategoryLevelChangedNotificationParams notification = api::DebugLoggingCategoryLevelChangedNotificationParams::fromJson(params);
        for (api::LoggingCategory& category : m_loggingCategories) {
            if (category.name == notification.name) {
                category.level = notification.level;
                sortLoggingCategories(m_loggingCategories);
                clampSettingsDetailsSelection();
                m_loggingCategoryStatus = "Logging category " + notification.name.toStdString() + " changed to " + loggingLevelLabel(notification.level) + ".";
                break;
            }
        }
        return;
    }

    if (notificationName == api::SystemUpdateStatusChangedNotification::notificationName()) {
        const bool wasActive = m_systemUpdateStatusLoaded && (m_systemUpdateStatus.busy || m_systemUpdateStatus.updateRunning);
        const api::SystemUpdateStatusChangedNotificationParams notification = api::SystemUpdateStatusChangedNotificationParams::fromJson(params);
        api::SystemGetUpdateStatusResponse status;
        status.busy = notification.busy;
        status.updateProgress = notification.updateProgress;
        status.updateRunning = notification.updateRunning;
        applySystemUpdateStatus(status);
        const bool isActive = status.busy || status.updateRunning;
        if (wasActive && !isActive) {
            m_systemPackagesLoaded = false;
            ensureSystemPackagesLoaded();
        }
        clampSettingsDetailsSelection();
        return;
    }

    if (notificationName == api::ConfigurationTcpServerConfigurationChangedNotification::notificationName()) {
        const api::ConfigurationTcpServerConfigurationChangedNotificationParams notification = api::ConfigurationTcpServerConfigurationChangedNotificationParams::fromJson(params);
        upsertByKey(m_tcpServerConfigurations, notification.tcpServerConfiguration, [](const api::ServerConfiguration& configuration) { return configuration.id; });
        m_serverInterfacesLoaded = true;
        clampServerInterfaceSelection();
        m_serverInterfaceStatus = "Live update: TCP server configuration changed.";
        return;
    }

    if (notificationName == api::ConfigurationTcpServerConfigurationRemovedNotification::notificationName()) {
        const api::ConfigurationTcpServerConfigurationRemovedNotificationParams notification = api::ConfigurationTcpServerConfigurationRemovedNotificationParams::fromJson(params);
        eraseByKey(m_tcpServerConfigurations, notification.id, [](const api::ServerConfiguration& configuration) { return configuration.id; });
        clampServerInterfaceSelection();
        m_serverInterfaceStatus = "Live update: TCP server configuration removed.";
        return;
    }

    if (notificationName == api::ConfigurationWebSocketServerConfigurationChangedNotification::notificationName()) {
        const api::ConfigurationWebSocketServerConfigurationChangedNotificationParams notification
            = api::ConfigurationWebSocketServerConfigurationChangedNotificationParams::fromJson(params);
        upsertByKey(m_webSocketServerConfigurations, notification.webSocketServerConfiguration, [](const api::ServerConfiguration& configuration) { return configuration.id; });
        m_serverInterfacesLoaded = true;
        clampServerInterfaceSelection();
        m_serverInterfaceStatus = "Live update: WebSocket server configuration changed.";
        return;
    }

    if (notificationName == api::ConfigurationWebSocketServerConfigurationRemovedNotification::notificationName()) {
        const api::ConfigurationWebSocketServerConfigurationRemovedNotificationParams notification
            = api::ConfigurationWebSocketServerConfigurationRemovedNotificationParams::fromJson(params);
        eraseByKey(m_webSocketServerConfigurations, notification.id, [](const api::ServerConfiguration& configuration) { return configuration.id; });
        clampServerInterfaceSelection();
        m_serverInterfaceStatus = "Live update: WebSocket server configuration removed.";
        return;
    }

    if (notificationName == api::ConfigurationWebServerConfigurationChangedNotification::notificationName()) {
        const api::ConfigurationWebServerConfigurationChangedNotificationParams notification = api::ConfigurationWebServerConfigurationChangedNotificationParams::fromJson(params);
        upsertByKey(m_webServerConfigurations, notification.webServerConfiguration, [](const api::WebServerConfiguration& configuration) { return configuration.id; });
        m_serverInterfacesLoaded = true;
        clampServerInterfaceSelection();
        m_serverInterfaceStatus = "Live update: WebServer configuration changed.";
        return;
    }

    if (notificationName == api::ConfigurationWebServerConfigurationRemovedNotification::notificationName()) {
        const api::ConfigurationWebServerConfigurationRemovedNotificationParams notification = api::ConfigurationWebServerConfigurationRemovedNotificationParams::fromJson(params);
        eraseByKey(m_webServerConfigurations, notification.id, [](const api::WebServerConfiguration& configuration) { return configuration.id; });
        clampServerInterfaceSelection();
        m_serverInterfaceStatus = "Live update: WebServer configuration removed.";
        return;
    }

    if (notificationName == api::ConfigurationTunnelProxyServerConfigurationChangedNotification::notificationName()) {
        const api::ConfigurationTunnelProxyServerConfigurationChangedNotificationParams notification
            = api::ConfigurationTunnelProxyServerConfigurationChangedNotificationParams::fromJson(params);
        upsertByKey(m_tunnelProxyServerConfigurations, notification.tunnelProxyServerConfiguration, [](const api::TunnelProxyServerConfiguration& configuration) {
            return configuration.id;
        });
        m_serverInterfacesLoaded = true;
        clampServerInterfaceSelection();
        m_serverInterfaceStatus = "Live update: Tunnel Proxy server configuration changed.";
        return;
    }

    if (notificationName == api::ConfigurationTunnelProxyServerConfigurationRemovedNotification::notificationName()) {
        const api::ConfigurationTunnelProxyServerConfigurationRemovedNotificationParams notification
            = api::ConfigurationTunnelProxyServerConfigurationRemovedNotificationParams::fromJson(params);
        eraseByKey(m_tunnelProxyServerConfigurations, notification.id, [](const api::TunnelProxyServerConfiguration& configuration) { return configuration.id; });
        clampServerInterfaceSelection();
        m_serverInterfaceStatus = "Live update: Tunnel Proxy server configuration removed.";
        return;
    }

    if (notificationName == api::ModbusRtuModbusRtuMasterAddedNotification::notificationName()) {
        const api::ModbusRtuModbusRtuMasterAddedNotificationParams notification = api::ModbusRtuModbusRtuMasterAddedNotificationParams::fromJson(params);
        upsertByKey(m_modbusRtuMasters, notification.modbusRtuMaster, [](const api::ModbusRtuMaster& master) { return master.modbusUuid; });
        m_modbusRtuMastersLoaded = true;
        clampModbusRtuSelection();
        m_modbusRtuStatus = "Live update: Modbus RTU master added.";
        return;
    }

    if (notificationName == api::ModbusRtuModbusRtuMasterChangedNotification::notificationName()) {
        const api::ModbusRtuModbusRtuMasterChangedNotificationParams notification = api::ModbusRtuModbusRtuMasterChangedNotificationParams::fromJson(params);
        upsertByKey(m_modbusRtuMasters, notification.modbusRtuMaster, [](const api::ModbusRtuMaster& master) { return master.modbusUuid; });
        m_modbusRtuMastersLoaded = true;
        clampModbusRtuSelection();
        m_modbusRtuStatus = "Live update: Modbus RTU master changed.";
        return;
    }

    if (notificationName == api::ModbusRtuModbusRtuMasterRemovedNotification::notificationName()) {
        const api::ModbusRtuModbusRtuMasterRemovedNotificationParams notification = api::ModbusRtuModbusRtuMasterRemovedNotificationParams::fromJson(params);
        eraseByKey(m_modbusRtuMasters, notification.modbusUuid, [](const api::ModbusRtuMaster& master) { return master.modbusUuid; });
        clampModbusRtuSelection();
        m_modbusRtuStatus = "Live update: Modbus RTU master removed.";
        return;
    }

    if (notificationName == api::ModbusRtuSerialPortAddedNotification::notificationName()) {
        const api::ModbusRtuSerialPortAddedNotificationParams notification = api::ModbusRtuSerialPortAddedNotificationParams::fromJson(params);
        upsertByKey(m_modbusRtuSerialPorts, notification.serialPort, [](const api::SerialPort& serialPort) { return serialPort.systemLocation; });
        m_modbusRtuSerialPortsLoaded = true;
        return;
    }

    if (notificationName == api::ModbusRtuSerialPortRemovedNotification::notificationName()) {
        const api::ModbusRtuSerialPortRemovedNotificationParams notification = api::ModbusRtuSerialPortRemovedNotificationParams::fromJson(params);
        eraseByKey(m_modbusRtuSerialPorts, notification.serialPort.systemLocation, [](const api::SerialPort& serialPort) { return serialPort.systemLocation; });
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
        m_thingManager.setStatus(m_thingManager.status() + " Live updates enabled for Integrations, Debug, Configuration, Modbus RTU, and Logging.");
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
    request.namespaces = QStringList{QStringLiteral("Integrations"), QStringLiteral("Debug"), QStringLiteral("Configuration"), QStringLiteral("ModbusRtu"),
                                     QStringLiteral("Logging")};

    observeReply(m_client.sendRequest(api::JSONRPCSetNotificationStatusMethod::methodName(), request.toJson()),
                 [this, fetchThingsAfterReply](const QJsonObject& message, const QString& transportError) {
                     handleEnableNotificationsReply(message, transportError, fetchThingsAfterReply);
                 });
}

} // namespace nymea
