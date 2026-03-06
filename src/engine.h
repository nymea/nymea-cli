#pragma once

#include "nymeajsonrpcclient.h"
#include "thingmanager.h"

#include <QString>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>

namespace nymea {

struct EngineOptions
{
    QString host = QStringLiteral("127.0.0.1");
    int port = 2223;
    bool useSsl = false;
    int timeoutMs = 5000;
    std::string username;
    std::string password;
    std::string appVersion;
};

class Engine
{
public:
    explicit Engine(EngineOptions options);

    int run();

private:
    std::string endpoint() const;

    bool connectToServer();
    bool sendHello();
    bool authenticate(const std::string& username, const std::string& password);
    bool fetchThings();
    void runHandshakeAndLoadThings();

    ftxui::Element renderThings() const;
    ftxui::Element renderUi() const;
    bool handleEvent(const ftxui::Event& event, ftxui::ScreenInteractive& screen);

    EngineOptions m_options;
    NymeaJsonRpcClient m_client;
    ThingManager m_thingManager;

    std::string m_connectionStatus = "Disconnected";
    std::string m_serverVersion = "n/a";
    std::string m_serverApiVersion = "n/a";

    bool m_isAuthenticationRequired = false;
    bool m_isAuthenticated = false;
    bool m_showLoginForm = false;
    std::string m_authStatus = "Not authenticated.";

    std::string m_username;
    std::string m_password;

    ftxui::InputOption m_passwordInputOption;
    ftxui::Component m_usernameInput;
    ftxui::Component m_passwordInput;
    ftxui::Component m_loginForm;
};

} // namespace nymea
