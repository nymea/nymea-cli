#include "engine.h"

#include <QJsonObject>
#include <QLocale>

#include <utility>

namespace nymea {

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

bool Engine::connectToServer()
{
    m_client.clearAuthToken();
    const NymeaJsonRpcClient::TransportSecurity security = m_options.useSsl ? NymeaJsonRpcClient::TransportSecurity::SslTls : NymeaJsonRpcClient::TransportSecurity::PlainTcp;
    if (m_client.connectToHost(m_options.host, static_cast<quint16>(m_options.port), security, m_options.timeoutMs)) {
        m_connectionStatus = std::string(m_client.isEncrypted() ? "SSL connected to " : "TCP connected to ") + endpoint();
        return true;
    }

    m_connectionStatus = "Connection failed: " + m_client.lastError().toStdString();
    m_serverVersion = "n/a";
    m_serverApiVersion = "n/a";
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
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
        m_authStatus = "Stored token was rejected. Please login.";
        m_thingManager.setStatus("JSONRPC.Hello unauthorized (request id " + std::to_string(requestId) + ").");
        return false;
    }

    if (status == QStringLiteral("error")) {
        m_thingManager.setStatus("JSONRPC.Hello returned error (request id " + std::to_string(requestId) + ").");
        return false;
    }

    const QJsonObject helloParams = message->value(QStringLiteral("params")).toObject();
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

    m_isAuthenticationRequired = helloParams.value(QStringLiteral("authenticationRequired")).toBool();
    const bool helloAuthenticated = helloParams.value(QStringLiteral("authenticated")).toBool();
    m_isAuthenticated = helloAuthenticated || !m_isAuthenticationRequired;
    m_showLoginForm = m_isAuthenticationRequired && !m_isAuthenticated;

    if (!m_isAuthenticationRequired) {
        m_authStatus = "Server does not require authentication.";
    } else if (m_isAuthenticated) {
        m_authStatus = "Authenticated.";
    } else {
        m_authStatus = "Authentication required.";
    }

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
    m_authStatus = "Authenticated as " + username + ".";
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
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_showLoginForm = true;
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

    m_thingManager.setStatus("Loaded " + std::to_string(m_thingManager.things().size()) + " thing(s) (request id " + std::to_string(requestId) + ").");
    return true;
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
            m_authStatus = "Authentication required. Enter username/password and press Enter.";
            return;
        }
    }

    fetchThings();
}

ftxui::Element Engine::renderThings() const
{
    ftxui::Elements lines;
    lines.push_back(ftxui::text(m_thingManager.status()));
    lines.push_back(ftxui::separator());

    if (m_thingManager.things().empty()) {
        lines.push_back(ftxui::text("No things found."));
    } else {
        for (const Thing& thing : m_thingManager.things()) {
            std::string line = "- " + thing.name;
            if (!thing.id.empty()) {
                line += " [" + thing.id + "]";
            }
            lines.push_back(ftxui::text(line));
        }
    }

    return ftxui::vbox(std::move(lines));
}

ftxui::Element Engine::renderUi() const
{
    ftxui::Elements sections;
    sections.push_back(ftxui::hbox({
                           ftxui::text(" nymea-cli (" + m_options.appVersion + ")"),
                           ftxui::filler(),
                           ftxui::text("server " + m_serverVersion + " | api " + m_serverApiVersion),
                       })
                       | ftxui::border);
    sections.push_back(ftxui::hbox({
                           ftxui::text(endpoint()),
                           ftxui::filler(),
                           ftxui::text(m_connectionStatus),
                       })
                       | ftxui::border);
    sections.push_back(ftxui::text("Keys: c reconnect, h hello, t refresh things, Enter login, q/Esc quit") | ftxui::dim);
    sections.push_back(ftxui::separator());
    sections.push_back(ftxui::window(ftxui::text("Things from server"), renderThings()) | ftxui::flex);

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
