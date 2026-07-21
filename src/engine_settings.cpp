// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

namespace nymea {

void Engine::openPowerActionConfirmDialog(PowerAction action)
{
    m_systemAction = action;
    m_showSystemActionConfirm = true;
    m_systemActionStatus = "Press Enter to confirm or Esc to cancel.";
}

void Engine::closePowerActionConfirmDialog()
{
    m_showSystemActionConfirm = false;
    m_systemActionRequestPending = false;
}

void Engine::executePowerAction()
{
    if (!m_client.isConnected() || m_systemActionRequestPending) {
        return;
    }

    m_systemActionRequestPending = true;
    m_systemActionStatus = "Sending " + powerActionLabel(static_cast<int>(m_systemAction)) + " request...";

    auto send = [&](const QString& method, auto params, auto handler) { observeReply(m_client.sendRequest(method, params.toJson()), handler); };

    switch (m_systemAction) {
    case PowerAction::Shutdown: {
        api::SystemShutdownParams params;
        send(api::SystemShutdownMethod::methodName(), params, [this](const QJsonObject& message, const QString& transportError) {
            handlePowerActionReply(message, transportError, PowerAction::Shutdown);
        });
        break;
    }
    case PowerAction::Restart: {
        api::SystemRestartParams params;
        send(api::SystemRestartMethod::methodName(), params, [this](const QJsonObject& message, const QString& transportError) {
            handlePowerActionReply(message, transportError, PowerAction::Restart);
        });
        break;
    }
    case PowerAction::Reboot: {
        api::SystemRebootParams params;
        send(api::SystemRebootMethod::methodName(), params, [this](const QJsonObject& message, const QString& transportError) {
            handlePowerActionReply(message, transportError, PowerAction::Reboot);
        });
        break;
    }
    }
}

void Engine::ensureSystemCapabilitiesLoaded()
{
    if (m_systemCapabilitiesLoaded || m_systemCapabilitiesPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemCapabilitiesPending = true;
    observeReply(m_client.sendRequest(api::SystemGetCapabilitiesMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemCapabilitiesReply(message, transportError); });
}

void Engine::ensureSystemTimeLoaded()
{
    if (m_systemTimeLoaded || m_systemTimePending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemTimePending = true;
    observeReply(m_client.sendRequest(api::SystemGetTimeMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemTimeReply(message, transportError); });
}

void Engine::ensureSystemUpdateStatusLoaded()
{
    if (m_systemUpdateStatusLoaded || m_systemUpdateStatusPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemUpdateStatusPending = true;
    observeReply(m_client.sendRequest(api::SystemGetUpdateStatusMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemUpdateStatusReply(message, transportError); });
}

void Engine::ensureSystemPackagesLoaded()
{
    if (m_systemPackagesLoaded || m_systemPackagesPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemPackagesPending = true;
    observeReply(m_client.sendRequest(api::SystemGetPackagesMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemPackagesReply(message, transportError); });
}

void Engine::applySystemUpdateStatus(const api::SystemGetUpdateStatusResponse& status)
{
    const bool wasActive = m_systemUpdateStatusLoaded && (m_systemUpdateStatus.busy || m_systemUpdateStatus.updateRunning);
    const bool isActive = status.busy || status.updateRunning;
    m_systemUpdateStatus = status;
    m_systemUpdateStatusLoaded = true;
    if (isActive && !wasActive) {
        m_systemUpdateStatusStartedAt = std::chrono::steady_clock::now();
    }
}

bool Engine::systemUpdateInteractionBusy() const
{
    return m_systemUpdateStatusLoaded && (m_systemUpdateStatus.busy || m_systemUpdateStatus.updateRunning);
}

std::vector<const api::Package*> Engine::updateAvailablePackages() const
{
    std::vector<const api::Package*> packages;
    for (const api::Package& package : m_systemPackages) {
        if (package.updateAvailable) {
            packages.push_back(&package);
        }
    }
    return packages;
}

int Engine::updateActionCount() const
{
    if (systemUpdateInteractionBusy()) {
        return 0;
    }
    return m_systemPackagesLoaded && !updateAvailablePackages().empty() ? 2 : 1;
}

void Engine::ensureSystemTimeZonesLoaded()
{
    if (m_systemTimeZonesLoaded || m_systemTimeZonesPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_systemTimeZonesPending = true;
    observeReply(m_client.sendRequest(api::SystemGetTimeZonesMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchSystemTimeZonesReply(message, transportError); });
}

void Engine::ensureLoggingCategoriesLoaded()
{
    if (m_loggingCategoriesLoaded || m_loggingCategoriesPending || !m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    m_loggingCategoriesPending = true;
    observeReply(m_client.sendRequest(api::DebugGetLoggingCategoriesMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleFetchLoggingCategoriesReply(message, transportError); });
}

QStringList Engine::filteredSystemTimeZones() const
{
    if (m_systemTimeZoneSearch.empty()) {
        return m_systemTimeZones;
    }

    QStringList filtered;
    const QString search = QString::fromStdString(m_systemTimeZoneSearch);
    for (const QString& timeZone : m_systemTimeZones) {
        if (caseInsensitiveContains(timeZone, search)) {
            filtered.append(timeZone);
        }
    }
    return filtered;
}

std::vector<api::LoggingCategory> Engine::filteredLoggingCategories() const
{
    if (m_loggingCategorySearch.empty()) {
        return m_loggingCategories;
    }

    std::vector<api::LoggingCategory> filtered;
    const QString search = QString::fromStdString(m_loggingCategorySearch);
    for (const api::LoggingCategory& category : m_loggingCategories) {
        if (caseInsensitiveContains(category.name, search)) {
            filtered.push_back(category);
        }
    }
    return filtered;
}

void Engine::handleFetchSystemCapabilitiesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemCapabilitiesPending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load system capabilities: " + transportError.toStdString();
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
        m_settingsWarning = "Settings warning: system capabilities request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: system capabilities request returned an error.";
        return;
    }

    m_systemCapabilities = api::SystemGetCapabilitiesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_systemCapabilitiesLoaded = true;
    m_settingsWarning.clear();
}

void Engine::handleFetchSystemTimeReply(const QJsonObject& message, const QString& transportError)
{
    m_systemTimePending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load system time: " + transportError.toStdString();
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
        m_settingsWarning = "Settings warning: system time request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: system time request returned an error.";
        return;
    }

    m_systemTime = api::SystemGetTimeResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_systemTimeLoaded = true;
    m_settingsWarning.clear();
}

void Engine::handleFetchSystemUpdateStatusReply(const QJsonObject& message, const QString& transportError)
{
    m_systemUpdateStatusPending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load update status: " + transportError.toStdString();
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
        m_settingsWarning = "Settings warning: update status request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: update status request returned an error.";
        return;
    }

    applySystemUpdateStatus(api::SystemGetUpdateStatusResponse::fromJson(message.value(QStringLiteral("params")).toObject()));
    m_settingsWarning.clear();
    clampSettingsDetailsSelection();
}

void Engine::handleFetchSystemPackagesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemPackagesPending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load packages: " + transportError.toStdString();
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
        m_settingsWarning = "Settings warning: package request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: package request returned an error.";
        return;
    }

    const api::SystemGetPackagesResponse response = api::SystemGetPackagesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_systemPackages.clear();
    m_systemPackages.reserve(response.packages.size());
    for (const api::Package& package : response.packages) {
        m_systemPackages.push_back(package);
    }
    m_systemPackagesLoaded = true;
    m_settingsWarning.clear();
    clampSettingsDetailsSelection();
}

void Engine::handleFetchSystemTimeZonesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemTimeZonesPending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load time zones: " + transportError.toStdString();
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
        m_settingsWarning = "Settings warning: time zone request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: time zone request returned an error.";
        return;
    }

    const api::SystemGetTimeZonesResponse response = api::SystemGetTimeZonesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_systemTimeZones = response.timeZones;
    m_systemTimeZonesLoaded = true;
    m_settingsWarning.clear();
}

void Engine::handleFetchLoggingCategoriesReply(const QJsonObject& message, const QString& transportError)
{
    m_loggingCategoriesPending = false;
    if (!transportError.isEmpty()) {
        m_settingsWarning = "Settings warning: failed to load logging categories: " + transportError.toStdString();
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
        m_settingsWarning = "Settings warning: logging categories request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_settingsWarning = "Settings warning: logging categories request returned an error.";
        return;
    }

    const api::DebugGetLoggingCategoriesResponse response = api::DebugGetLoggingCategoriesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_loggingCategories.clear();
    m_loggingCategories.reserve(response.loggingCategories.size());
    for (const api::LoggingCategory& category : response.loggingCategories) {
        m_loggingCategories.push_back(category);
    }
    sortLoggingCategories(m_loggingCategories);
    m_loggingCategoriesLoaded = true;
    m_loggingCategoryStatus = "Loaded " + std::to_string(m_loggingCategories.size()) + " logging categories.";
    m_settingsWarning.clear();
    clampSettingsDetailsSelection();
}

void Engine::handleCheckForUpdatesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemActionRequestPending = false;
    if (!transportError.isEmpty()) {
        m_systemActionStatus = "Check for updates failed: " + transportError.toStdString();
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
        m_systemActionStatus = "Check for updates was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_systemActionStatus = "Check for updates returned an error.";
        return;
    }

    const api::SystemCheckForUpdatesResponse response = api::SystemCheckForUpdatesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (!response.success) {
        m_systemActionStatus = "Check for updates failed.";
        return;
    }

    m_systemActionStatus = "Update check started.";
    m_settingsWarning.clear();
    m_systemUpdateStatusLoaded = false;
    m_systemPackagesLoaded = false;
    ensureSystemUpdateStatusLoaded();
    ensureSystemPackagesLoaded();
}

void Engine::handleSetTimeZoneReply(const QJsonObject& message, const QString& transportError)
{
    m_systemActionRequestPending = false;
    if (!transportError.isEmpty()) {
        m_systemActionStatus = "Failed to set time zone: " + transportError.toStdString();
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
        m_systemActionStatus = "Setting the time zone was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_systemActionStatus = "Setting the time zone returned an error.";
        return;
    }

    const api::ConfigurationSetTimeZoneResponse response = api::ConfigurationSetTimeZoneResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.configurationError != api::ConfigurationError::ConfigurationErrorNoError) {
        m_systemActionStatus = "Setting the time zone failed: " + api::toString(response.configurationError).toStdString();
        return;
    }

    m_systemActionStatus = "Time zone updated.";
    m_settingsWarning.clear();
    m_systemTimeLoaded = false;
    ensureSystemTimeLoaded();
}

void Engine::handleUpdatePackagesReply(const QJsonObject& message, const QString& transportError)
{
    m_systemActionRequestPending = false;
    if (!transportError.isEmpty()) {
        m_systemActionStatus = "Update request failed: " + transportError.toStdString();
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
        m_systemActionStatus = "Updating packages was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_systemActionStatus = "Update request returned an error.";
        return;
    }

    const api::SystemUpdatePackagesResponse response = api::SystemUpdatePackagesResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (!response.success) {
        m_systemActionStatus = "Update request failed.";
        return;
    }

    m_systemActionStatus = "Update started.";
    m_settingsWarning.clear();
    m_systemUpdateStatusLoaded = false;
    m_systemPackagesLoaded = false;
    ensureSystemUpdateStatusLoaded();
    ensureSystemPackagesLoaded();
}

void Engine::handleSetLoggingCategoryLevelReply(const QJsonObject& message, const QString& transportError, const QString& categoryName, api::LoggingLevel level)
{
    m_systemActionRequestPending = false;
    if (!transportError.isEmpty()) {
        m_loggingCategoryStatus = "Logging category update failed: " + transportError.toStdString();
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
        m_loggingCategoryStatus = "Setting the logging category level was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_loggingCategoryStatus = "Setting the logging category level returned an error.";
        return;
    }

    const api::DebugSetLoggingCategoryLevelResponse response = api::DebugSetLoggingCategoryLevelResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.debugError != api::DebugError::DebugErrorNoError) {
        m_loggingCategoryStatus = "Setting the logging category level failed: " + api::toString(response.debugError).toStdString();
        return;
    }

    for (api::LoggingCategory& category : m_loggingCategories) {
        if (category.name == categoryName) {
            category.level = level;
            break;
        }
    }
    sortLoggingCategories(m_loggingCategories);
    m_loggingCategoryStatus = "Logging category " + categoryName.toStdString() + " set to " + loggingLevelLabel(level) + ".";
    m_settingsWarning.clear();
    clampSettingsDetailsSelection();
}

void Engine::handlePowerActionReply(const QJsonObject& message, const QString& transportError, PowerAction action)
{
    m_systemActionRequestPending = false;
    if (!transportError.isEmpty()) {
        m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " failed: " + transportError.toStdString();
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
        m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " returned an error.";
        return;
    }

    bool success = false;
    switch (action) {
    case PowerAction::Shutdown:
        success = api::SystemShutdownResponse::fromJson(message.value(QStringLiteral("params")).toObject()).success;
        break;
    case PowerAction::Restart:
        success = api::SystemRestartResponse::fromJson(message.value(QStringLiteral("params")).toObject()).success;
        break;
    case PowerAction::Reboot:
        success = api::SystemRebootResponse::fromJson(message.value(QStringLiteral("params")).toObject()).success;
        break;
    }

    if (!success) {
        m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " request was rejected.";
        return;
    }

    m_systemActionStatus = powerActionLabel(static_cast<int>(action)) + " requested.";
    m_settingsWarning.clear();
    closePowerActionConfirmDialog();
}

ftxui::Element Engine::renderSettingsMenu() const
{
    constexpr std::array<const char*, 9> menuItems = {"Server info", "Timezone", "Update", "Logging categories", "Server interfaces", "Modbus RTU", "Shutdown", "Restart", "Reboot"};

    ftxui::Elements entries;
    for (int index = 0; index < static_cast<int>(menuItems.size()); ++index) {
        const bool selected = static_cast<int>(m_settingsView) == index;
        auto entry = ftxui::text(std::string(" ") + menuItems.at(index) + " ");
        if (selected) {
            entry = entry | ftxui::bold | ftxui::inverted;
        }
        if (m_focusArea == FocusArea::SettingsMenu && selected) {
            entry = entry | ftxui::color(ftxui::Color::CyanLight);
        } else if (m_focusArea == FocusArea::SettingsDetails && selected) {
            entry = entry | ftxui::color(ftxui::Color::CyanLight);
        }
        if (selected) {
            entry = entry | ftxui::focus;
        }
        entries.push_back(entry);
    }

    return renderFocusedWindow(ftxui::text("Settings"), ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::frame, m_focusArea == FocusArea::SettingsMenu)
           | ftxui::reflect(m_settingsMenuBox);
}

ftxui::Element Engine::renderSettingsDetails() const
{
    ftxui::Elements lines;
    int lineIndex = 0;
    auto pushLine = [&](ftxui::Element line) {
        if (m_focusArea == FocusArea::SettingsDetails && lineIndex == m_settingsDetailsLineIndex) {
            line = line | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight) | ftxui::focus;
        }
        lines.push_back(std::move(line));
        ++lineIndex;
    };
    auto pushStaticLine = [&](ftxui::Element line) {
        lines.push_back(std::move(line));
        ++lineIndex;
    };
    auto pushSelectableLine = [&](ftxui::Element line, int minimumWidth = 0) {
        const bool selected = m_focusArea == FocusArea::SettingsDetails && lineIndex == m_settingsDetailsLineIndex;
        pushLine(renderActiveField(std::move(line), selected, minimumWidth));
    };

    if (m_settingsView == SettingsView::ServerInfo) {
        const QString fingerprint = m_client.peerCertificateFingerprint();
        pushLine(ftxui::text("Connection: " + endpoint()));
        pushLine(ftxui::text("Display name: " + connectionDisplayName()));
        pushLine(ftxui::text("Settings path: " + m_connectionSettings.settingsPath().toStdString()));
        pushLine(ftxui::text("Transport: " + std::string(m_options.useSsl ? "SSL/TLS" : "Plain TCP")));
        pushLine(ftxui::text("Server uuid: " + m_serverUuid.toString(QUuid::WithoutBraces).toStdString()));
        pushLine(ftxui::text("Authentication: " + m_authStatus));
        pushLine(ftxui::text("Stored token: " + std::string(m_savedConnection.has_value() && !m_savedConnection->token.isEmpty() ? "available" : "none")));
        pushLine(ftxui::text("TLS fingerprint: " + (fingerprint.isEmpty() ? std::string("n/a") : fingerprint.toStdString())));
    } else if (m_settingsView == SettingsView::Timezone) {
        const QStringList filteredTimeZones = filteredSystemTimeZones();
        pushLine(ftxui::text("Current time zone: " + (m_systemTimeLoaded ? m_systemTime.timeZone.toStdString() : std::string("<loading>"))));
        pushLine(ftxui::text("Automatic time: " + (m_systemTimeLoaded ? std::string(m_systemTime.automaticTime ? "enabled" : "disabled") : std::string("n/a"))));
        pushLine(ftxui::text("Automatic time available: " + (m_systemTimeLoaded ? std::string(m_systemTime.automaticTimeAvailable ? "yes" : "no") : std::string("n/a"))));
        pushLine(ftxui::separator());
        auto search = ftxui::text("Search: " + (m_systemTimeZoneSearch.empty() ? std::string("<type to filter>") : m_systemTimeZoneSearch));
        if (m_focusArea == FocusArea::SettingsDetails && m_settingsDetailsLineIndex == timezoneSearchLineIndex) {
            search = renderActiveField(std::move(search) | ftxui::inverted | ftxui::bold | ftxui::color(ftxui::Color::CyanLight), true, 32);
        }
        pushLine(std::move(search));
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Available time zones") | ftxui::bold);
        if (!m_systemTimeZonesLoaded) {
            pushLine(ftxui::text("Loading time zones..."));
        } else {
            if (filteredTimeZones.isEmpty()) {
                pushLine(ftxui::text("No time zones match the current filter."));
            } else {
                for (const QString& timeZone : filteredTimeZones) {
                    pushSelectableLine(ftxui::text(" " + timeZone.toStdString() + " "), 24);
                }
            }
        }
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Type to search, Up/Down move, Enter applies the selected time zone.") | ftxui::dim);
    } else if (m_settingsView == SettingsView::Update) {
        const std::string capabilityText = m_systemCapabilitiesLoaded ? std::string("Update management: ") + (m_systemCapabilities.updateManagement ? "available" : "unavailable")
                                                                            + " (" + api::toString(m_systemCapabilities.updateManagementType).toStdString() + ")"
                                                                      : std::string("Update management: loading...");
        const std::vector<const api::Package*> updatePackages = updateAvailablePackages();
        const bool updateRunning = m_systemUpdateStatusLoaded && m_systemUpdateStatus.updateRunning;
        const bool updaterBusy = m_systemUpdateStatusLoaded && m_systemUpdateStatus.busy;
        int updateActionIndex = 0;
        auto pushUpdateAction = [&](ftxui::Element line, int minimumWidth = 0) {
            const bool selected = m_focusArea == FocusArea::SettingsDetails && m_settingsDetailsLineIndex == updateActionIndex;
            if (selected) {
                line = std::move(line) | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight);
            }
            lines.push_back(renderActiveField(std::move(line), selected, minimumWidth));
            ++lineIndex;
            ++updateActionIndex;
        };

        pushStaticLine(ftxui::text(capabilityText));
        if (m_systemUpdateStatusLoaded) {
            std::string statusText;
            if (updateRunning) {
                statusText = "Update running: ";
                statusText += m_systemUpdateStatus.updateProgress.has_value() ? progressBar(*m_systemUpdateStatus.updateProgress) : busyIndicator(m_systemUpdateStatusStartedAt);
            } else if (updaterBusy) {
                statusText = "Busy: " + busyIndicator(m_systemUpdateStatusStartedAt) + " Busy";
            } else {
                statusText = "Idle";
            }
            pushStaticLine(ftxui::text("Status: " + statusText));
        } else {
            pushStaticLine(ftxui::text("Status: loading..."));
        }
        if (!m_systemActionStatus.empty()) {
            pushStaticLine(ftxui::text("Message: " + m_systemActionStatus));
        }
        pushStaticLine(ftxui::separator());
        pushStaticLine(ftxui::text("Actions") | ftxui::bold);
        if (systemUpdateInteractionBusy()) {
            pushStaticLine(ftxui::text(updateRunning ? "Update is running." : "Updater is busy.") | ftxui::dim);
        } else {
            pushUpdateAction(ftxui::text(" Check for updates "), 24);
            if (!updatePackages.empty()) {
                pushUpdateAction(ftxui::text(" Perform update "), 24);
            }
        }
        pushStaticLine(ftxui::separator());
        pushStaticLine(ftxui::text("Packages with updates") | ftxui::bold);
        if (!m_systemPackagesLoaded) {
            pushStaticLine(ftxui::text("Loading packages..."));
        } else if (updatePackages.empty()) {
            pushStaticLine(ftxui::text("No updates available."));
        } else {
            for (const api::Package* package : updatePackages) {
                std::string label = package->displayName.toStdString();
                if (!package->installedVersion.isEmpty() || !package->candidateVersion.isEmpty()) {
                    label += " (" + package->installedVersion.toStdString();
                    if (!package->candidateVersion.isEmpty()) {
                        label += " -> " + package->candidateVersion.toStdString();
                    }
                    label += ")";
                }
                label += " [update available]";
                if (!package->summary.isEmpty()) {
                    label += " - " + package->summary.toStdString();
                }
                pushStaticLine(ftxui::paragraph(label));
            }
        }
        pushStaticLine(ftxui::separator());
        pushStaticLine(ftxui::text(systemUpdateInteractionBusy() ? "Update actions are disabled until the updater is idle."
                                                                 : (m_systemUpdateStatusPending ? "Loading update status..." : "Enter runs the selected update action."))
                       | ftxui::dim);
    } else if (m_settingsView == SettingsView::LoggingCategories) {
        const std::vector<api::LoggingCategory> filteredCategories = filteredLoggingCategories();
        const std::string statusText = m_loggingCategoryStatus.empty() ? std::string("Status: ") + (m_loggingCategoriesPending ? "loading..." : "ready")
                                                                       : "Status: " + m_loggingCategoryStatus;
        pushLine(ftxui::text(statusText));
        pushLine(ftxui::separator());
        auto search = ftxui::text("Filter: " + (m_loggingCategorySearch.empty() ? std::string("<type to filter>") : m_loggingCategorySearch));
        if (m_focusArea == FocusArea::SettingsDetails && m_settingsDetailsLineIndex == loggingCategorySearchLineIndex) {
            search = renderActiveField(std::move(search) | ftxui::inverted | ftxui::bold | ftxui::color(ftxui::Color::CyanLight), true, 32);
        }
        pushLine(std::move(search));
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Logging categories") | ftxui::bold);
        if (!m_loggingCategoriesLoaded) {
            pushLine(ftxui::text("Loading logging categories..."));
        } else if (filteredCategories.empty()) {
            pushLine(ftxui::text("No logging categories match the current filter."));
        } else {
            for (const api::LoggingCategory& category : filteredCategories) {
                const std::string label = " " + category.name.toStdString() + " [" + loggingLevelLabel(category.level) + "] ";
                const ftxui::Color levelColor = loggingLevelColor(category.level);
                pushSelectableLine(ftxui::hbox({
                                       ftxui::text(" ") | ftxui::bgcolor(levelColor),
                                       ftxui::text(label) | ftxui::color(levelColor),
                                   }),
                                   36);
            }
        }
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Type to filter. Left/Right or Space changes the selected level.") | ftxui::dim);
    } else if (m_settingsView == SettingsView::ServerInterfaces) {
        const std::string statusText = m_serverInterfaceStatus.empty() ? std::string("Status: ") + (m_serverInterfacesPending ? "loading..." : "ready")
                                                                       : "Status: " + m_serverInterfaceStatus;
        int serverInterfaceRowIndex = 0;
        auto pushServerInterfaceRow = [&](const std::string& label) {
            auto row = ftxui::text(label);
            const bool selected = m_focusArea == FocusArea::SettingsDetails && m_settingsDetailsLineIndex == serverInterfaceRowIndex;
            if (selected) {
                row = row | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight);
            }
            lines.push_back(renderActiveField(std::move(row), selected, 56));
            ++lineIndex;
            ++serverInterfaceRowIndex;
        };
        auto commonLabel = [](const std::string& type, const api::ServerConfiguration& configuration) {
            return " " + type + " | " + configuration.id.toStdString() + " | " + configuration.address.toStdString() + ":" + QString::number(configuration.port).toStdString()
                   + " | ssl " + std::string(configuration.sslEnabled ? "on" : "off") + " | auth " + std::string(configuration.authenticationEnabled ? "on" : "off") + " ";
        };

        pushStaticLine(ftxui::text(statusText));
        pushStaticLine(ftxui::text("Actions: a add, Enter/e edit, d delete, r refresh") | ftxui::dim);
        pushStaticLine(ftxui::separator());
        if (!m_serverInterfacesLoaded) {
            pushStaticLine(ftxui::text("Loading server interfaces..."));
        } else if (serverInterfaceCount() == 0) {
            pushStaticLine(ftxui::text("No server interfaces configured."));
        } else {
            pushStaticLine(ftxui::text("TCP server") | ftxui::bold);
            for (const api::ServerConfiguration& configuration : m_tcpServerConfigurations) {
                pushServerInterfaceRow(commonLabel("TCP", configuration));
            }
            pushStaticLine(ftxui::text("WebSocket server") | ftxui::bold);
            for (const api::ServerConfiguration& configuration : m_webSocketServerConfigurations) {
                pushServerInterfaceRow(commonLabel("WebSocket", configuration));
            }
            pushStaticLine(ftxui::text("WebServer") | ftxui::bold);
            for (const api::WebServerConfiguration& configuration : m_webServerConfigurations) {
                std::string label = " WebServer | " + configuration.id.toStdString() + " | " + configuration.address.toStdString() + ":"
                                    + QString::number(configuration.port).toStdString() + " | ssl " + std::string(configuration.sslEnabled ? "on" : "off") + " | auth "
                                    + std::string(configuration.authenticationEnabled ? "on" : "off");
                if (!configuration.publicFolder.isEmpty()) {
                    label += " | " + configuration.publicFolder.toStdString();
                }
                label += " ";
                pushServerInterfaceRow(label);
            }
            pushStaticLine(ftxui::text("Tunnel Proxy server") | ftxui::bold);
            for (const api::TunnelProxyServerConfiguration& configuration : m_tunnelProxyServerConfigurations) {
                std::string label = " Tunnel Proxy | " + configuration.id.toStdString() + " | " + configuration.address.toStdString() + ":"
                                    + QString::number(configuration.port).toStdString() + " | ssl " + std::string(configuration.sslEnabled ? "on" : "off") + " | auth "
                                    + std::string(configuration.authenticationEnabled ? "on" : "off") + " | ignore SSL errors "
                                    + std::string(configuration.ignoreSslErrors ? "on" : "off") + " ";
                pushServerInterfaceRow(label);
            }
        }
        pushStaticLine(ftxui::separator());
        if (const std::optional<ServerInterfaceSelection> selection = selectedServerInterface(); selection.has_value()) {
            pushStaticLine(ftxui::text("Selected: " + serverInterfaceTypeLabel(selection->type)) | ftxui::bold);
        } else {
            pushStaticLine(ftxui::text("Selected: none") | ftxui::bold);
        }
    } else if (m_settingsView == SettingsView::ModbusRtu) {
        const api::ModbusRtuMaster* selectedMaster = selectedModbusRtuMaster();
        const std::string statusText = m_modbusRtuStatus.empty() ? std::string("Status: ") + ((m_modbusRtuMastersPending || m_modbusRtuSerialPortsPending) ? "loading..." : "ready")
                                                                 : "Status: " + m_modbusRtuStatus;
        pushLine(ftxui::text(statusText));
        pushLine(ftxui::text("Actions: a add, Enter/e edit, d delete, r refresh") | ftxui::dim);
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Configured masters") | ftxui::bold);
        if (!m_modbusRtuMastersLoaded) {
            pushLine(ftxui::text("Loading Modbus RTU masters..."));
        } else if (m_modbusRtuMasters.empty()) {
            pushLine(ftxui::text("No Modbus RTU masters configured."));
        } else {
            for (const api::ModbusRtuMaster& master : m_modbusRtuMasters) {
                const std::string connected = master.connected ? "connected" : "disconnected";
                const std::string label = " " + master.serialPort.toStdString() + " | " + QString::number(master.baudrate).toStdString() + " " + dataBitsLabel(master.dataBits)
                                          + parityLabel(master.parity).substr(0, 1) + stopBitsLabel(master.stopBits) + " | " + connected + " ";
                pushSelectableLine(ftxui::text(label), 42);
            }
        }
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Selected master") | ftxui::bold);
        if (selectedMaster == nullptr) {
            pushLine(ftxui::text("No master selected."));
        } else {
            pushLine(ftxui::text("UUID: " + selectedMaster->modbusUuid.toString(QUuid::WithoutBraces).toStdString()));
            pushLine(ftxui::text("Serial port: " + selectedMaster->serialPort.toStdString()));
            pushLine(ftxui::text("State: " + std::string(selectedMaster->connected ? "connected" : "disconnected")));
            pushLine(ftxui::text("Data: " + QString::number(selectedMaster->baudrate).toStdString() + " baud, " + dataBitsLabel(selectedMaster->dataBits) + " data bits, "
                                 + parityLabel(selectedMaster->parity) + " parity, " + stopBitsLabel(selectedMaster->stopBits) + " stop bits"));
            pushLine(ftxui::text("Timeout: " + QString::number(selectedMaster->timeout).toStdString()
                                 + " ms | Retries: " + QString::number(selectedMaster->numberOfRetries).toStdString()));
        }
        pushLine(ftxui::separator());
        pushLine(ftxui::text("Available serial ports") | ftxui::bold);
        if (!m_modbusRtuSerialPortsLoaded) {
            pushLine(ftxui::text("Loading serial ports..."));
        } else if (m_modbusRtuSerialPorts.empty()) {
            pushLine(ftxui::text("No serial ports discovered. Add still supports manual paths."));
        } else {
            for (const api::SerialPort& serialPort : m_modbusRtuSerialPorts) {
                pushLine(ftxui::paragraph(serialPortLabel(serialPort)));
            }
        }
    } else {
        const std::string action = powerActionLabel(static_cast<int>(m_systemAction));
        pushLine(ftxui::text(action) | ftxui::bold | ftxui::center | ftxui::border | ftxui::color(ftxui::Color::RedLight));
        pushStaticLine(ftxui::paragraph("This will request a " + action + " on the server."));
        pushStaticLine(ftxui::paragraph("Enter opens the confirmation dialog, and Left or Esc returns to the settings menu."));
        if (!m_systemActionStatus.empty()) {
            pushStaticLine(ftxui::separator());
            pushStaticLine(ftxui::text(m_systemActionStatus));
        }
    }

    return renderFocusedWindow(ftxui::text(settingsViewLabel(static_cast<int>(m_settingsView))),
                               ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame,
                               m_focusArea == FocusArea::SettingsDetails)
           | ftxui::reflect(m_settingsDetailsBox);
}

ftxui::Element Engine::renderLogout() const
{
    ftxui::Elements lines;
    lines.push_back(ftxui::text("Logout"));
    lines.push_back(ftxui::separator());
    lines.push_back(ftxui::paragraph("Logout revokes the current token on the server, clears the saved token locally, and reconnects to the same server."));
    lines.push_back(ftxui::separator());
    lines.push_back(ftxui::text("Only execution option: Logout") | ftxui::bold);
    lines.push_back(ftxui::text("Press Enter to logout, or Left to return to the menu.") | ftxui::dim);

    return renderFocusedWindow(ftxui::text("Logout"), ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame, m_mainView == MainView::Logout) | ftxui::flex;
}

int Engine::settingsDetailsLineCount() const
{
    switch (m_settingsView) {
    case SettingsView::ServerInfo:
        return 8;
    case SettingsView::Timezone:
        return m_systemTimeZonesLoaded ? 9 + std::max(1, static_cast<int>(filteredSystemTimeZones().size())) : 10;
    case SettingsView::Update:
        return updateActionCount();
    case SettingsView::LoggingCategories:
        return m_loggingCategoriesLoaded ? 7 + std::max(1, static_cast<int>(filteredLoggingCategories().size())) : 8;
    case SettingsView::ServerInterfaces:
        return std::max(1, serverInterfaceCount());
    case SettingsView::ModbusRtu:
        return modbusRtuMasterListStartLineIndex + std::max(1, static_cast<int>(m_modbusRtuMasters.size()));
    case SettingsView::Shutdown:
    case SettingsView::Restart:
    case SettingsView::Reboot:
        return 1;
    }
    return 0;
}

void Engine::clampSettingsDetailsSelection()
{
    const int lineCount = settingsDetailsLineCount();
    if (lineCount <= 0) {
        m_settingsDetailsLineIndex = 0;
        return;
    }

    if (m_settingsView == SettingsView::Timezone) {
        const int filteredCount = m_systemTimeZonesLoaded ? static_cast<int>(filteredSystemTimeZones().size()) : 0;
        const int firstResultLineIndex = timezoneListStartLineIndex;
        if (filteredCount <= 0) {
            m_settingsDetailsLineIndex = timezoneSearchLineIndex;
            return;
        }

        const int lastResultLineIndex = firstResultLineIndex + filteredCount - 1;
        if (m_settingsDetailsLineIndex == timezoneSearchLineIndex) {
            return;
        }
        if (m_settingsDetailsLineIndex < firstResultLineIndex) {
            m_settingsDetailsLineIndex = timezoneSearchLineIndex;
            return;
        }
        if (m_settingsDetailsLineIndex > lastResultLineIndex) {
            m_settingsDetailsLineIndex = lastResultLineIndex;
            return;
        }
    }

    if (m_settingsView == SettingsView::LoggingCategories) {
        const int filteredCount = m_loggingCategoriesLoaded ? static_cast<int>(filteredLoggingCategories().size()) : 0;
        const int firstResultLineIndex = loggingCategoryListStartLineIndex;
        if (filteredCount <= 0) {
            m_settingsDetailsLineIndex = loggingCategorySearchLineIndex;
            return;
        }

        const int lastResultLineIndex = firstResultLineIndex + filteredCount - 1;
        if (m_settingsDetailsLineIndex == loggingCategorySearchLineIndex) {
            return;
        }
        if (m_settingsDetailsLineIndex < firstResultLineIndex) {
            m_settingsDetailsLineIndex = loggingCategorySearchLineIndex;
            return;
        }
        if (m_settingsDetailsLineIndex > lastResultLineIndex) {
            m_settingsDetailsLineIndex = lastResultLineIndex;
            return;
        }
    }

    if (m_settingsView == SettingsView::ModbusRtu) {
        clampModbusRtuSelection();
        return;
    }

    if (m_settingsView == SettingsView::ServerInterfaces) {
        clampServerInterfaceSelection();
        return;
    }

    if (m_settingsDetailsLineIndex < 0) {
        m_settingsDetailsLineIndex = 0;
    } else if (m_settingsDetailsLineIndex >= lineCount) {
        m_settingsDetailsLineIndex = lineCount - 1;
    }
}

} // namespace nymea
