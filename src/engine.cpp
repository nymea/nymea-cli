#include "engine.h"

#include "generated/nymeaapigenerated.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include <array>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

namespace nymea {

namespace {

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

} // namespace

Engine::Engine(EngineOptions options)
    : m_options(std::move(options))
    , m_username(m_options.username)
    , m_password(m_options.password)
{
    m_passwordInputOption.password = true;
    m_usernameInput = ftxui::Input(&m_username, "Username");
    m_passwordInput = ftxui::Input(&m_password, "Password", m_passwordInputOption);
    m_loginForm = ftxui::Container::Vertical({m_usernameInput, m_passwordInput});
}

int Engine::run()
{
    connectToServer();
    if (m_client.isConnected()) {
        runHandshakeAndLoadThings();
    }

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto ui = ftxui::Renderer([this] { return renderUi(); });
    auto withKeyHandler = ftxui::CatchEvent(ui, [&](ftxui::Event event) { return handleEvent(event, screen); });

    screen.Loop(withKeyHandler);
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

bool Engine::connectToServer()
{
    m_client.clearAuthToken();
    loadSavedConnection();
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

bool Engine::sendHello()
{
    if (!m_client.isConnected()) {
        m_thingManager.setStatus("Cannot send JSONRPC.Hello while disconnected.");
        return false;
    }

    QJsonObject params;
    params.insert(QStringLiteral("locale"), QLocale().name());

    const int requestId = m_client.sendRequest(QStringLiteral("JSONRPC.Hello"), params);
    if (requestId < 0) {
        m_thingManager.setStatus("Failed to send JSONRPC.Hello: " + m_client.lastError().toStdString());
        return false;
    }

    auto message = m_client.waitForMessage(m_options.timeoutMs);
    if (!message.has_value()) {
        m_thingManager.setStatus("No reply for JSONRPC.Hello: " + m_client.lastError().toStdString());
        return false;
    }

    const QString status = message->value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Stored token was rejected. Please login.";
        m_thingManager.setStatus("JSONRPC.Hello unauthorized (request id " + std::to_string(requestId) + ").");
        return false;
    }

    if (status == QStringLiteral("error")) {
        m_thingManager.setStatus("JSONRPC.Hello returned error (request id " + std::to_string(requestId) + ").");
        return false;
    }

    const QJsonObject helloParams = message->value(QStringLiteral("params")).toObject();
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
        serverApiVersionValue = message->value(QStringLiteral("jsonrpc")).toString();
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
    return true;
}

bool Engine::authenticate(const std::string& username, const std::string& password)
{
    if (!m_client.isConnected()) {
        m_authStatus = "Cannot authenticate while disconnected.";
        return false;
    }

    if (username.empty() || password.empty()) {
        m_authStatus = "Username and password are required.";
        m_showLoginForm = true;
        return false;
    }

    QJsonObject params;
    params.insert(QStringLiteral("username"), QString::fromStdString(username));
    params.insert(QStringLiteral("password"), QString::fromStdString(password));
    params.insert(QStringLiteral("deviceName"), QStringLiteral("nymea-cli"));

    const int requestId = m_client.sendRequest(QStringLiteral("JSONRPC.Authenticate"), params);
    if (requestId < 0) {
        m_authStatus = "Failed to send authenticate request: " + m_client.lastError().toStdString();
        m_showLoginForm = true;
        return false;
    }

    auto message = m_client.waitForMessage(m_options.timeoutMs);
    if (!message.has_value()) {
        m_authStatus = "No reply for authenticate request: " + m_client.lastError().toStdString();
        m_showLoginForm = true;
        return false;
    }

    const QString status = message->value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("error") || status == QStringLiteral("unauthorized")) {
        m_authStatus = "Authentication rejected by server.";
        m_showLoginForm = true;
        m_isAuthenticated = false;
        m_client.clearAuthToken();
        return false;
    }

    const QJsonObject authParams = message->value(QStringLiteral("params")).toObject();
    if (!authParams.value(QStringLiteral("success")).toBool()) {
        m_authStatus = "Authentication failed.";
        m_showLoginForm = true;
        m_isAuthenticated = false;
        m_client.clearAuthToken();
        return false;
    }

    const QString token = authParams.value(QStringLiteral("token")).toString();
    if (token.isEmpty()) {
        m_authStatus = "Authentication succeeded but no token was returned.";
        m_showLoginForm = true;
        m_isAuthenticated = false;
        m_client.clearAuthToken();
        return false;
    }

    m_client.setAuthToken(token);
    m_isAuthenticated = true;
    m_isAuthenticationRequired = false;
    m_showLoginForm = false;
    m_focusArea = FocusArea::MainMenu;
    m_authStatus = "Authenticated as " + username + ".";
    saveCurrentConnection(true);
    m_thingManager.setStatus("Authentication succeeded (request id " + std::to_string(requestId) + ").");
    return true;
}

bool Engine::fetchThings()
{
    m_thingManager.clear();

    if (!m_client.isConnected()) {
        m_thingManager.setStatus("Cannot fetch things while disconnected.");
        return false;
    }

    if (m_isAuthenticationRequired && !m_isAuthenticated) {
        m_thingManager.setStatus("Authentication required before fetching things.");
        m_showLoginForm = true;
        m_focusArea = FocusArea::LoginForm;
        return false;
    }

    const int requestId = m_client.sendRequest(QStringLiteral("Integrations.GetThings"));
    if (requestId < 0) {
        m_thingManager.setStatus("Failed to send Integrations.GetThings: " + m_client.lastError().toStdString());
        return false;
    }

    auto message = m_client.waitForMessage(m_options.timeoutMs);
    if (!message.has_value()) {
        m_thingManager.setStatus("No reply for Integrations.GetThings: " + m_client.lastError().toStdString());
        return false;
    }

    const QString status = message->value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_thingManager.setStatus("Integrations.GetThings unauthorized.");
        return false;
    }

    if (status == QStringLiteral("error")) {
        m_thingManager.setStatus("Integrations.GetThings returned error.");
        return false;
    }

    std::string errorMessage;
    if (!m_thingManager.updateFromReply(message.value(), errorMessage)) {
        m_thingManager.setStatus(errorMessage);
        return false;
    }

    if (!fetchThingClasses()) {
        return false;
    }

    if (m_thingManager.things().empty()) {
        m_selectedThingIndex = 0;
    } else if (m_selectedThingIndex >= static_cast<int>(m_thingManager.things().size())) {
        m_selectedThingIndex = static_cast<int>(m_thingManager.things().size()) - 1;
    } else if (m_selectedThingIndex < 0) {
        m_selectedThingIndex = 0;
    }

    m_thingManager.setStatus("Loaded " + std::to_string(m_thingManager.things().size()) + " thing(s) (request id " + std::to_string(requestId) + ").");
    return true;
}

bool Engine::fetchThingClasses()
{
    const std::vector<std::string> thingClassIds = m_thingManager.thingClassIds();
    if (thingClassIds.empty()) {
        return true;
    }

    api::IntegrationsGetThingClassesParams params;
    QList<QUuid> ids;
    for (const std::string& thingClassId : thingClassIds) {
        ids.append(QUuid(QString::fromStdString(thingClassId)));
    }
    params.thingClassIds = ids;

    const int requestId = m_client.sendRequest(QStringLiteral("Integrations.GetThingClasses"), params.toJson());
    if (requestId < 0) {
        m_thingManager.setStatus("Thing list loaded, but fetching thing classes failed: " + m_client.lastError().toStdString());
        return true;
    }

    auto message = m_client.waitForMessage(m_options.timeoutMs);
    if (!message.has_value()) {
        m_thingManager.setStatus("Thing list loaded, but no reply for Integrations.GetThingClasses: " + m_client.lastError().toStdString());
        return true;
    }

    const QString status = message->value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_focusArea = FocusArea::LoginForm;
        m_authStatus = "Authentication required. Please login.";
        m_thingManager.setStatus("Integrations.GetThingClasses unauthorized.");
        return false;
    }

    if (status == QStringLiteral("error")) {
        m_thingManager.setStatus("Thing list loaded, but Integrations.GetThingClasses returned error.");
        return true;
    }

    std::string errorMessage;
    if (!m_thingManager.updateThingClassesFromReply(message.value(), errorMessage)) {
        m_thingManager.setStatus("Thing list loaded, but thing class metadata is unavailable: " + errorMessage);
        return true;
    }

    m_thingManager.setStatus("Loaded " + std::to_string(m_thingManager.things().size()) + " thing(s) and enriched them with type metadata (request id " + std::to_string(requestId)
                             + ").");
    return true;
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
    if (!sendHello()) {
        return;
    }

    if (m_isAuthenticationRequired && !m_isAuthenticated) {
        if (!m_username.empty() && !m_password.empty()) {
            if (!authenticate(m_username, m_password)) {
                return;
            }
        } else {
            m_showLoginForm = true;
            m_focusArea = FocusArea::LoginForm;
            m_authStatus = "Authentication required. Enter username/password and press Enter.";
            return;
        }
    }

    fetchThings();
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
        return ftxui::window(ftxui::text("Thing details"), ftxui::text("No thing selected."));
    }

    const api::ThingClass* thingClass = m_thingManager.thingClassForThing(*thing);
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

    ftxui::Elements params;
    if (thing->params.empty()) {
        params.push_back(ftxui::text("No params."));
    } else {
        for (const api::Param& param : thing->params) {
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
            std::vector<std::string> fields;
            appendField(fields, "value", jsonValueToString(param.value));
            if (paramType != nullptr) {
                appendField(fields, "type", api::toString(paramType->type).toStdString());
                appendField(fields, "unit", paramType->unit.has_value() ? api::toString(*paramType->unit).toStdString() : std::string());
                appendField(fields, "input", paramType->inputType.has_value() ? api::toString(*paramType->inputType).toStdString() : std::string());
                appendField(fields, "min", optionalJsonValueToString(paramType->minValue));
                appendField(fields, "max", optionalJsonValueToString(paramType->maxValue));
                appendField(fields, "allowed", optionalJsonValuesToString(paramType->allowedValues));
            }

            params.push_back(ftxui::paragraph(label + ": " + joinFields(fields)));
            if (param.paramTypeId.has_value()) {
                params.push_back(ftxui::text("  id: " + uuidToStd(*param.paramTypeId)) | ftxui::dim);
            }
        }
    }

    ftxui::Elements states;
    if (thing->states.empty()) {
        states.push_back(ftxui::text("No states."));
    } else {
        for (const api::State& state : thing->states) {
            const api::StateType* stateType = m_thingManager.stateTypeForThing(*thing, state);
            const std::string label = [&] {
                if (stateType != nullptr) {
                    return firstNonEmpty({stateType->displayName.toStdString(), stateType->name.toStdString()});
                }
                return firstNonEmpty({uuidToStd(state.stateTypeId), "<unknown state>"});
            }();
            std::vector<std::string> fields;
            appendField(fields, "value", jsonValueToString(state.value));
            if (stateType != nullptr) {
                appendField(fields, "type", api::toString(stateType->type).toStdString());
                appendField(fields, "unit", stateType->unit.has_value() ? api::toString(*stateType->unit).toStdString() : std::string());
                appendField(fields, "io", stateType->ioType.has_value() ? api::toString(*stateType->ioType).toStdString() : std::string());
            }
            appendField(fields, "filter", api::toString(state.filter).toStdString());
            appendField(fields,
                        "min",
                        firstNonEmpty({optionalJsonValueToString(state.minValue), stateType != nullptr ? optionalJsonValueToString(stateType->minValue) : std::string()}));
            appendField(fields,
                        "max",
                        firstNonEmpty({optionalJsonValueToString(state.maxValue), stateType != nullptr ? optionalJsonValueToString(stateType->maxValue) : std::string()}));
            appendField(fields,
                        "allowed",
                        firstNonEmpty(
                            {optionalJsonValuesToString(state.possibleValues), stateType != nullptr ? optionalJsonValuesToString(stateType->possibleValues) : std::string()}));

            states.push_back(ftxui::paragraph(label + ": " + joinFields(fields)));
            states.push_back(ftxui::text("  id: " + uuidToStd(state.stateTypeId)) | ftxui::dim);
        }
    }

    return ftxui::window(ftxui::text("Thing details"),
                         ftxui::vbox({
                             ftxui::window(ftxui::text("Overview"), ftxui::vbox(std::move(metadata))),
                             ftxui::window(ftxui::text("Params"), ftxui::vbox(std::move(params))),
                             ftxui::window(ftxui::text("States"), ftxui::vbox(std::move(states))) | ftxui::flex,
                         }) | ftxui::vscroll_indicator
                             | ftxui::frame);
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

ftxui::Element Engine::renderUi() const
{
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
    sections.push_back(ftxui::text("Keys: Up/Down navigate, Left/Right switch panels, c reconnect, h hello, t refresh things, Enter login, q/Esc quit") | ftxui::dim);
    if (!m_settingsWarning.empty()) {
        sections.push_back(ftxui::text(m_settingsWarning) | ftxui::color(ftxui::Color::Yellow));
    }
    sections.push_back(ftxui::separator());

    ftxui::Element rightPanel;
    if (m_mainView == MainView::Things) {
        rightPanel = ftxui::window(ftxui::text("Things from server"), renderThings()) | ftxui::flex;
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
    if (event == ftxui::Event::Character("q") || event == ftxui::Event::Escape) {
        m_client.disconnectFromHost();
        screen.ExitLoopClosure()();
        return true;
    }

    if (event == ftxui::Event::ArrowLeft) {
        m_focusArea = FocusArea::MainMenu;
        return true;
    }

    if (event == ftxui::Event::ArrowRight) {
        if (m_showLoginForm) {
            m_focusArea = FocusArea::LoginForm;
        } else if (m_mainView == MainView::Things) {
            m_focusArea = FocusArea::ThingList;
        } else if (m_mainView == MainView::Settings) {
            m_focusArea = FocusArea::SettingsMenu;
        }
        return true;
    }

    if (m_showLoginForm) {
        if (event == ftxui::Event::Return) {
            if (authenticate(m_username, m_password)) {
                fetchThings();
            }
            return true;
        }
        if (m_loginForm->OnEvent(event)) {
            return true;
        }
    }

    if (event == ftxui::Event::ArrowUp) {
        if (m_focusArea == FocusArea::ThingList && !m_thingManager.things().empty()) {
            m_selectedThingIndex = (m_selectedThingIndex + static_cast<int>(m_thingManager.things().size()) - 1) % static_cast<int>(m_thingManager.things().size());
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
        if (m_focusArea == FocusArea::ThingList && !m_thingManager.things().empty()) {
            m_selectedThingIndex = (m_selectedThingIndex + 1) % static_cast<int>(m_thingManager.things().size());
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
