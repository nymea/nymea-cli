#include <iostream>
#include <string>
#include <vector>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "nymea_jsonrpc_client.h"

#ifndef APP_VERSION
#define APP_VERSION "0.1.0"
#endif

namespace {

ftxui::Element renderThings(const std::vector<std::string>& things, const std::string& status)
{
    ftxui::Elements lines;
    lines.push_back(ftxui::text(status));
    lines.push_back(ftxui::separator());

    if (things.empty()) {
        lines.push_back(ftxui::text("No things found."));
    } else {
        for (const auto& thing : things) {
            lines.push_back(ftxui::text("- " + thing));
        }
    }

    return ftxui::vbox(std::move(lines));
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nymea-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("nymea-cli terminal client for nymead JSON-RPC over TCP."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption hostOption({QStringLiteral("H"), QStringLiteral("host")}, QStringLiteral("nymead host."), QStringLiteral("host"), QStringLiteral("127.0.0.1"));
    QCommandLineOption sslOption({QStringLiteral("s"), QStringLiteral("ssl")}, QStringLiteral("Use SSL/TLS (QSslSocket). Default port: 2222. Plain default port: 2223."));
    QCommandLineOption portOption({QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("nymead TCP port (defaults: 2222 for --ssl, 2223 for plain)."),
                                  QStringLiteral("port"));
    QCommandLineOption timeoutOption({QStringLiteral("t"), QStringLiteral("timeout")},
                                     QStringLiteral("Timeout in milliseconds for connect/reply waits."),
                                     QStringLiteral("ms"),
                                     QStringLiteral("5000"));
    QCommandLineOption usernameOption({QStringLiteral("u"), QStringLiteral("username")}, QStringLiteral("Username for JSONRPC.Authenticate."), QStringLiteral("username"));
    QCommandLineOption passwordOption({QStringLiteral("w"), QStringLiteral("password")}, QStringLiteral("Password for JSONRPC.Authenticate."), QStringLiteral("password"));

    parser.addOption(hostOption);
    parser.addOption(sslOption);
    parser.addOption(portOption);
    parser.addOption(timeoutOption);
    parser.addOption(usernameOption);
    parser.addOption(passwordOption);
    parser.process(app);

    const bool useSsl = parser.isSet(sslOption);
    const int defaultPort = useSsl ? 2222 : 2223;

    int port = defaultPort;
    bool isPortValid = true;
    if (parser.isSet(portOption)) {
        port = parser.value(portOption).toInt(&isPortValid);
    }
    if (!isPortValid || port <= 0 || port > 65535) {
        std::cerr << "Invalid --port value. Expected 1-65535.\n";
        return 1;
    }

    bool isTimeoutValid = false;
    const int timeoutMs = parser.value(timeoutOption).toInt(&isTimeoutValid);
    if (!isTimeoutValid || timeoutMs <= 0) {
        std::cerr << "Invalid --timeout value. Expected positive milliseconds.\n";
        return 1;
    }

    const QString host = parser.value(hostOption);
    const std::string appVersion = app.applicationVersion().toStdString();
    const std::string endpoint = std::string(useSsl ? "ssl://" : "tcp://") + host.toStdString() + ":" + std::to_string(static_cast<unsigned>(port));
    const auto transportSecurity = useSsl ? nymea::NymeaJsonRpcClient::TransportSecurity::SslTls : nymea::NymeaJsonRpcClient::TransportSecurity::PlainTcp;

    std::string loginUsername = parser.value(usernameOption).toStdString();
    std::string loginPassword = parser.value(passwordOption).toStdString();

    ftxui::InputOption passwordInputOption;
    passwordInputOption.password = true;
    auto usernameInput = ftxui::Input(&loginUsername, "Username");
    auto passwordInput = ftxui::Input(&loginPassword, "Password", passwordInputOption);
    auto loginForm = ftxui::Container::Vertical({usernameInput, passwordInput});

    nymea::NymeaJsonRpcClient client;
    std::string connectionStatus = "Disconnected";
    std::string serverVersion = "n/a";
    std::string serverApiVersion = "n/a";

    bool isAuthenticationRequired = false;
    bool isAuthenticated = false;
    bool showLoginForm = false;
    std::string authStatus = "Not authenticated.";

    std::string thingsStatus = "No thing list loaded.";
    std::vector<std::string> things;

    const auto connectToServer = [&]() {
        client.clearAuthToken();
        if (client.connectToHost(host, static_cast<quint16>(port), transportSecurity, timeoutMs)) {
            connectionStatus = std::string(client.isEncrypted() ? "SSL connected to " : "TCP connected to ") + endpoint;
            return;
        }
        connectionStatus = "Connection failed: " + client.lastError().toStdString();
        isAuthenticated = false;
        isAuthenticationRequired = false;
        showLoginForm = false;
    };

    const auto sendHello = [&]() {
        if (!client.isConnected()) {
            thingsStatus = "Cannot send JSONRPC.Hello while disconnected.";
            return false;
        }

        QJsonObject params;
        params.insert(QStringLiteral("locale"), QLocale().name());
        const int requestId = client.sendRequest(QStringLiteral("JSONRPC.Hello"), params);
        if (requestId < 0) {
            thingsStatus = "Failed to send JSONRPC.Hello: " + client.lastError().toStdString();
            return false;
        }

        auto message = client.waitForMessage(timeoutMs);
        if (!message.has_value()) {
            thingsStatus = "No reply for JSONRPC.Hello: " + client.lastError().toStdString();
            return false;
        }

        const QString status = message->value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("unauthorized")) {
            client.clearAuthToken();
            isAuthenticationRequired = true;
            isAuthenticated = false;
            showLoginForm = true;
            authStatus = "Stored token was rejected. Please login.";
            thingsStatus = "JSONRPC.Hello unauthorized (request id " + std::to_string(requestId) + ").";
            return false;
        }

        if (status == QStringLiteral("error")) {
            thingsStatus = "JSONRPC.Hello returned error (request id " + std::to_string(requestId) + ").";
            return false;
        }

        const QJsonObject helloParams = message->value(QStringLiteral("params")).toObject();
        const QString serverVersionValue = helloParams.value(QStringLiteral("version")).toString();
        if (!serverVersionValue.isEmpty()) {
            serverVersion = serverVersionValue.toStdString();
        }

        QString serverApiVersionValue = helloParams.value(QStringLiteral("protocol version")).toString();
        if (serverApiVersionValue.isEmpty()) {
            serverApiVersionValue = helloParams.value(QStringLiteral("protocolVersion")).toString();
        }
        if (serverApiVersionValue.isEmpty()) {
            serverApiVersionValue = message->value(QStringLiteral("jsonrpc")).toString();
        }
        if (!serverApiVersionValue.isEmpty()) {
            serverApiVersion = serverApiVersionValue.toStdString();
        }

        isAuthenticationRequired = helloParams.value(QStringLiteral("authenticationRequired")).toBool();
        const bool helloAuthenticated = helloParams.value(QStringLiteral("authenticated")).toBool();
        isAuthenticated = helloAuthenticated || !isAuthenticationRequired;
        showLoginForm = isAuthenticationRequired && !isAuthenticated;

        if (!isAuthenticationRequired) {
            authStatus = "Server does not require authentication.";
        } else if (isAuthenticated) {
            authStatus = "Authenticated.";
        } else {
            authStatus = "Authentication required.";
        }

        thingsStatus = "JSONRPC.Hello succeeded (request id " + std::to_string(requestId) + ").";
        return true;
    };

    const auto authenticate = [&](const std::string& username, const std::string& password) {
        if (!client.isConnected()) {
            authStatus = "Cannot authenticate while disconnected.";
            return false;
        }

        if (username.empty() || password.empty()) {
            authStatus = "Username and password are required.";
            showLoginForm = true;
            return false;
        }

        QJsonObject params;
        params.insert(QStringLiteral("username"), QString::fromStdString(username));
        params.insert(QStringLiteral("password"), QString::fromStdString(password));
        params.insert(QStringLiteral("deviceName"), QStringLiteral("nymea-cli"));

        const int requestId = client.sendRequest(QStringLiteral("JSONRPC.Authenticate"), params);
        if (requestId < 0) {
            authStatus = "Failed to send authenticate request: " + client.lastError().toStdString();
            showLoginForm = true;
            return false;
        }

        auto message = client.waitForMessage(timeoutMs);
        if (!message.has_value()) {
            authStatus = "No reply for authenticate request: " + client.lastError().toStdString();
            showLoginForm = true;
            return false;
        }

        const QString status = message->value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("error") || status == QStringLiteral("unauthorized")) {
            authStatus = "Authentication rejected by server.";
            showLoginForm = true;
            isAuthenticated = false;
            client.clearAuthToken();
            return false;
        }

        const QJsonObject authParams = message->value(QStringLiteral("params")).toObject();
        if (!authParams.value(QStringLiteral("success")).toBool()) {
            authStatus = "Authentication failed.";
            showLoginForm = true;
            isAuthenticated = false;
            client.clearAuthToken();
            return false;
        }

        const QString token = authParams.value(QStringLiteral("token")).toString();
        if (token.isEmpty()) {
            authStatus = "Authentication succeeded but no token was returned.";
            showLoginForm = true;
            isAuthenticated = false;
            client.clearAuthToken();
            return false;
        }

        client.setAuthToken(token);
        isAuthenticated = true;
        isAuthenticationRequired = false;
        showLoginForm = false;
        authStatus = "Authenticated as " + username + ".";
        thingsStatus = "Authentication succeeded (request id " + std::to_string(requestId) + ").";
        return true;
    };

    const auto fetchThings = [&]() {
        things.clear();

        if (!client.isConnected()) {
            thingsStatus = "Cannot fetch things while disconnected.";
            return;
        }

        if (isAuthenticationRequired && !isAuthenticated) {
            thingsStatus = "Authentication required before fetching things.";
            showLoginForm = true;
            return;
        }

        const int requestId = client.sendRequest(QStringLiteral("Integrations.GetThings"));
        if (requestId < 0) {
            thingsStatus = "Failed to send Integrations.GetThings: " + client.lastError().toStdString();
            return;
        }

        auto message = client.waitForMessage(timeoutMs);
        if (!message.has_value()) {
            thingsStatus = "No reply for Integrations.GetThings: " + client.lastError().toStdString();
            return;
        }

        const QString status = message->value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("unauthorized")) {
            client.clearAuthToken();
            isAuthenticationRequired = true;
            isAuthenticated = false;
            showLoginForm = true;
            authStatus = "Authentication required. Please login.";
            thingsStatus = "Integrations.GetThings unauthorized.";
            return;
        }

        if (status == QStringLiteral("error")) {
            thingsStatus = "Integrations.GetThings returned error.";
            return;
        }

        const QJsonObject params = message->value(QStringLiteral("params")).toObject();
        const QString thingError = params.value(QStringLiteral("thingError")).toString();
        if (!thingError.isEmpty() && thingError != QStringLiteral("ThingErrorNoError")) {
            thingsStatus = "Integrations.GetThings returned error: " + thingError.toStdString();
            return;
        }

        const QJsonArray thingsArray = params.value(QStringLiteral("things")).toArray();
        for (const QJsonValue& thingValue : thingsArray) {
            const QJsonObject thingObject = thingValue.toObject();
            QString name = thingObject.value(QStringLiteral("name")).toString();
            if (name.isEmpty()) {
                name = QStringLiteral("<unnamed>");
            }
            const QString id = thingObject.value(QStringLiteral("id")).toString();
            things.push_back(name.toStdString() + " [" + id.toStdString() + "]");
        }

        thingsStatus = "Loaded " + std::to_string(things.size()) + " thing(s) (request id " + std::to_string(requestId) + ").";
    };

    const auto runHandshakeAndLoadThings = [&]() {
        if (!sendHello()) {
            return;
        }

        if (isAuthenticationRequired && !isAuthenticated) {
            if (!loginUsername.empty() && !loginPassword.empty()) {
                if (!authenticate(loginUsername, loginPassword)) {
                    return;
                }
            } else {
                showLoginForm = true;
                authStatus = "Authentication required. Enter username/password and press Enter.";
                return;
            }
        }

        fetchThings();
    };

    connectToServer();
    if (client.isConnected()) {
        runHandshakeAndLoadThings();
    }

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto ui = ftxui::Renderer([&] {
        ftxui::Elements sections;
        sections.push_back(ftxui::hbox({
                               ftxui::text(" nymea-cli (" + appVersion + ")"),
                               ftxui::filler(),
                               ftxui::text("server " + serverVersion + " | api " + serverApiVersion),
                           })
                           | ftxui::border);
        sections.push_back(ftxui::hbox({
                               ftxui::text(endpoint),
                               ftxui::filler(),
                               ftxui::text(connectionStatus),
                           })
                           | ftxui::border);
        sections.push_back(ftxui::text("Keys: c reconnect, h hello, t refresh things, Enter login, q/Esc quit") | ftxui::dim);
        sections.push_back(ftxui::separator());
        sections.push_back(ftxui::window(ftxui::text("Things from server"), renderThings(things, thingsStatus)) | ftxui::flex);

        if (showLoginForm) {
            sections.push_back(ftxui::separator());
            sections.push_back(ftxui::window(ftxui::text("Authentication required"),
                                             ftxui::vbox({
                                                 ftxui::hbox({
                                                     ftxui::text("Username: "),
                                                     usernameInput->Render() | ftxui::xflex,
                                                 }),
                                                 ftxui::hbox({
                                                     ftxui::text("Password: "),
                                                     passwordInput->Render() | ftxui::xflex,
                                                 }),
                                                 ftxui::separator(),
                                                 ftxui::text(authStatus),
                                             })));
        }

        return ftxui::vbox(std::move(sections)) | ftxui::border | ftxui::flex;
    });

    auto withKeyHandler = ftxui::CatchEvent(ui, [&](ftxui::Event event) {
        if (event == ftxui::Event::Character("q") || event == ftxui::Event::Escape) {
            client.disconnectFromHost();
            screen.ExitLoopClosure()();
            return true;
        }

        if (showLoginForm) {
            if (event == ftxui::Event::Return) {
                if (authenticate(loginUsername, loginPassword)) {
                    fetchThings();
                }
                return true;
            }
            if (loginForm->OnEvent(event)) {
                return true;
            }
        }

        if (event == ftxui::Event::Character("c")) {
            connectToServer();
            if (client.isConnected()) {
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
    });

    screen.Loop(withKeyHandler);
    return 0;
}
