// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

#include <ftxui/screen/terminal.hpp>

#include <utility>

namespace nymea {


Engine::Engine(EngineOptions options)
    : m_options(std::move(options))
    , m_username(m_options.username)
    , m_password(m_options.password)
{
    m_client.setNotificationHandler([this](const QJsonObject& message) { enqueueUiTask([this, message]() { handleNotification(message); }); });
    m_client.setStateHandler([this](bool connected, bool encrypted, const QString& peerCertificateFingerprint, const QString& authToken, const QString& lastError) {
        enqueueUiTask([this, connected, encrypted, peerCertificateFingerprint, authToken, lastError]() {
            handleClientStateChanged(connected, encrypted, peerCertificateFingerprint, authToken, lastError);
        });
    });
    m_passwordInputOption.password = true;
    m_usernameInput = ftxui::Input(&m_username, "Username");
    m_passwordInput = ftxui::Input(&m_password, "Password", m_passwordInputOption);
    m_loginForm = ftxui::Container::Vertical({m_usernameInput, m_passwordInput});
}

int Engine::run()
{
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse();
    m_screen = &screen;
    auto ui = ftxui::Renderer([this] { return renderUi(); });
    auto withKeyHandler = ftxui::CatchEvent(ui, [&](ftxui::Event event) { return handleEvent(event, screen); });

    connectToServer();
    if (m_client.isConnected()) {
        runHandshakeAndLoadThings();
    }

    screen.Loop(withKeyHandler);
    m_screen = nullptr;
    return 0;
}

void Engine::openHelpView()
{
    if (m_mainView == MainView::Help) {
        return;
    }

    m_previousMainView = m_mainView;
    m_previousFocusArea = m_focusArea;
    m_mainView = MainView::Help;
    m_helpLineIndex = 0;
    m_focusArea = FocusArea::MainMenu;
}

void Engine::closeHelpView()
{
    if (m_mainView != MainView::Help) {
        return;
    }

    m_mainView = m_previousMainView;
    m_focusArea = m_previousFocusArea;
    switch (m_mainView) {
    case MainView::Things:
        clampThingSelection();
        clampThingDetailSelection();
        break;
    case MainView::ConfigureThings:
        clampConfigureThingClassSelection();
        clampConfigureThingSelection();
        break;
    case MainView::ApiBrowser:
        clampApiBrowserSelection();
        clampApiBrowserReferenceSelection();
        clampApiBrowserJsonSelection();
        break;
    case MainView::Settings:
        clampSettingsDetailsSelection();
        break;
    case MainView::Logout:
        break;
    case MainView::About:
        break;
    case MainView::Help:
        break;
    }
}

void Engine::observeReply(JsonRpcReply* reply, std::function<void(const QJsonObject&, const QString&)> handler)
{
    if (reply == nullptr) {
        enqueueUiTask([handler = std::move(handler)]() mutable { handler(QJsonObject{}, QStringLiteral("Failed to create JSON-RPC reply.")); });
        return;
    }

    auto dispatch = [this, reply, handler = std::move(handler)]() mutable {
        const QJsonObject message = reply->message();
        const QString transportError = reply->transportError();
        enqueueUiTask([reply, handler = std::move(handler), message, transportError]() mutable {
            handler(message, transportError);
            QMetaObject::invokeMethod(reply, "deleteLater", Qt::QueuedConnection);
        });
    };

    if (reply->isFinished()) {
        dispatch();
        return;
    }

    QObject::connect(reply, &JsonRpcReply::finished, [dispatch = std::move(dispatch)]() mutable { dispatch(); });
}

void Engine::enqueueUiTask(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_uiTaskMutex);
        m_uiTasks.push_back(std::move(task));
    }

    if (m_screen != nullptr) {
        m_screen->PostEvent(ftxui::Event::Custom);
    }
}

void Engine::drainUiTasks()
{
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock(m_uiTaskMutex);
        tasks.swap(m_uiTasks);
    }

    for (auto& task : tasks) {
        task();
    }
}

void Engine::runHandshakeAndLoadThings()
{
    sendHello();
}

Engine::MainMenuEntry Engine::nextMainMenuEntry(MainMenuEntry entry, int delta)
{
    const int count = static_cast<int>(s_mainMenuEntries.size());
    const int index = static_cast<int>(std::find(s_mainMenuEntries.begin(), s_mainMenuEntries.end(), entry) - s_mainMenuEntries.begin());
    return s_mainMenuEntries.at(((index + delta) % count + count) % count);
}

const std::string* Engine::focusedFilterText() const
{
    switch (m_focusArea) {
    case FocusArea::ThingList:
        return &m_thingSearch;
    case FocusArea::ConfigureThingClassList:
        return &m_configureThingSearch;
    case FocusArea::ConfigureThingSelection:
        return &m_configureThingSelectionSearch;
    case FocusArea::ApiBrowserList:
        return &m_apiBrowserSearch;
    case FocusArea::SettingsDetails:
        if (m_settingsView == SettingsView::Timezone) {
            return &m_systemTimeZoneSearch;
        }
        if (m_settingsView == SettingsView::LoggingCategories) {
            return &m_loggingCategorySearch;
        }
        return nullptr;
    default:
        return nullptr;
    }
}

std::string* Engine::focusedFilterText()
{
    return const_cast<std::string*>(std::as_const(*this).focusedFilterText());
}

bool Engine::isFilterEditing() const
{
    return m_filterEditing && focusedFilterText() != nullptr;
}

void Engine::applyFilterChange(const QUuid& previousThingId)
{
    switch (m_focusArea) {
    case FocusArea::ThingList:
        clampThingSelection(previousThingId);
        resetThingDetailSelection();
        break;
    case FocusArea::ConfigureThingClassList:
        clampConfigureThingClassSelection();
        break;
    case FocusArea::ConfigureThingSelection:
        m_selectedConfigureThingIndex = 0;
        clampConfigureThingSelection();
        break;
    case FocusArea::ApiBrowserList:
        clampApiBrowserSelection();
        clampApiBrowserReferenceSelection();
        break;
    case FocusArea::SettingsDetails:
        clampSettingsDetailsSelection();
        break;
    default:
        break;
    }
}

ftxui::Element Engine::renderFilterRow(const std::string& text, bool listFocused, int minimumWidth) const
{
    if (listFocused && isFilterEditing()) {
        return renderActiveField(ftxui::text("Filter: " + text + "_") | ftxui::inverted | ftxui::bold | ftxui::color(ftxui::Color::CyanLight), true, minimumWidth);
    }
    if (text.empty()) {
        return ftxui::text("Filter: / to search") | ftxui::dim;
    }
    return ftxui::text("Filter: " + text);
}

ftxui::Element Engine::renderMainMenu() const
{
    constexpr std::array<const char*, 6> menuItems = {"Things", "Configure things", "API browser", "Settings", "Logout", "About"};
    const bool apiBrowserEnabled = m_client.isConnected() && (!m_isAuthenticationRequired || m_isAuthenticated);
    const bool logoutEnabled = m_client.isConnected() && m_isAuthenticated && !m_client.authToken().isEmpty();

    int tabWidth = 0;
    for (const char* item : menuItems) {
        tabWidth = std::max(tabWidth, static_cast<int>(std::string(item).size()) + 2);
    }
    // Separators between tabs plus the outer border take the remaining columns.
    const int requiredWidth = tabWidth * static_cast<int>(menuItems.size()) + static_cast<int>(menuItems.size()) + 1;
    const bool focused = m_focusArea == FocusArea::MainMenu;
    const bool equalTabs = ftxui::Terminal::Size().dimx >= requiredWidth;

    ftxui::Elements entries;
    for (int index = 0; index < static_cast<int>(menuItems.size()); ++index) {
        const MainMenuEntry menuEntry = s_mainMenuEntries.at(index);
        const bool enabled = (menuEntry != MainMenuEntry::ApiBrowser || apiBrowserEnabled) && (menuEntry != MainMenuEntry::Logout || logoutEnabled);
        const bool selected = m_selectedMainMenuEntry == menuEntry;
        const int minimumWidth = equalTabs ? tabWidth : static_cast<int>(std::string(menuItems.at(index)).size()) + 2;
        auto entry = ftxui::text(menuItems.at(index)) | ftxui::center | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, minimumWidth) | ftxui::xflex_grow;
        if (!enabled) {
            entry = entry | ftxui::dim;
        }
        if (selected && enabled) {
            entry = focused ? entry | ftxui::bold | ftxui::color(ftxui::Color::White) | ftxui::bgcolor(ftxui::Color::SteelBlue) : entry | ftxui::bold | ftxui::inverted;
        }
        if (index > 0) {
            entries.push_back(focused ? ftxui::separator() | ftxui::color(ftxui::Color::SteelBlue) : ftxui::separator());
        }
        entries.push_back(entry);
    }

    return ftxui::vbox({
        ftxui::hbox(std::move(entries)) | ftxui::reflect(m_mainMenuBox),
        focused ? ftxui::separator() | ftxui::color(ftxui::Color::SteelBlue) : ftxui::separator(),
    });
}

ftxui::Element Engine::renderAbout() const
{
    ftxui::Elements lines;
    lines.push_back(ftxui::text("nymea-cli"));
    lines.push_back(ftxui::separator());
    lines.push_back(ftxui::text("Application version: " + m_options.appVersion));
    lines.push_back(ftxui::text("Project license: " APP_LICENSE_SPDX));
    lines.push_back(ftxui::text("Server version: " + m_serverVersion));
    lines.push_back(ftxui::text("Server API version: " + m_serverApiVersion));
    lines.push_back(ftxui::text("Purpose: terminal client for nymead"));
    lines.push_back(ftxui::text("Navigation: Tab/Shift+Tab switch tabs, Up/Down move, Left/Right switch panels"));
    lines.push_back(ftxui::separator());
    lines.push_back(ftxui::text("Open source components"));
    lines.push_back(ftxui::text("Qt Core + Network version: " + std::string(qVersion())));
    lines.push_back(ftxui::paragraph("Qt licensing reference: https://www.qt.io/licensing/"));
    lines.push_back(ftxui::paragraph("Qt project reference: https://www.qt.io/"));
    lines.push_back(ftxui::text("FTXUI version: " FTXUI_VERSION));
    lines.push_back(ftxui::text("FTXUI license: MIT"));
    lines.push_back(ftxui::paragraph("FTXUI project reference: https://github.com/ArthurSonzogni/FTXUI"));
    lines.push_back(ftxui::paragraph("FTXUI license reference: https://github.com/ArthurSonzogni/FTXUI/blob/" FTXUI_VERSION "/LICENSE"));

    return renderFocusedWindow(ftxui::text("About"), ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame, m_mainView == MainView::About) | ftxui::flex;
}

ftxui::Element Engine::renderConnectionLost() const
{
    return ftxui::vbox({
               ftxui::filler(),
               ftxui::hbox({
                   ftxui::filler(),
                   ftxui::window(ftxui::text("Connection lost"),
                                 ftxui::vbox({
                                     ftxui::text("Connection lost, try to reconnect."),
                                     ftxui::separator(),
                                     ftxui::text("Press c to reconnect, q or Esc to quit.") | ftxui::dim,
                                 }))
                       | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 44),
                   ftxui::filler(),
               }),
               ftxui::filler(),
           })
           | ftxui::flex;
}

ftxui::Element Engine::renderThings() const
{
    return ftxui::hbox({
               renderThingList() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 36),
               renderThingDetails() | ftxui::flex,
           })
           | ftxui::flex;
}

ftxui::Element Engine::renderDialogFieldRow(bool selected, const std::string& label, const std::string& value, int minimumWidth) const
{
    ftxui::Element marker = ftxui::text(selected ? "> " : "  ");
    ftxui::Element valueElement = ftxui::text(value) | ftxui::flex;
    if (selected) {
        marker = marker | ftxui::bold | ftxui::color(ftxui::Color::CyanLight);
        valueElement = valueElement | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight);
    }
    ftxui::Element row = ftxui::hbox({
        std::move(marker),
        ftxui::text(label + ": "),
        std::move(valueElement),
    });
    if (selected) {
        row = renderActiveField(std::move(row), true, minimumWidth);
    }
    return row;
}

ftxui::Element Engine::renderUi()
{
    drainUiTasks();
    if (m_connectionLost) {
        return renderConnectionLost();
    }

    if (m_mainView == MainView::Things) {
        clampThingDetailSelection();
    } else if (m_mainView == MainView::ConfigureThings) {
        clampConfigureThingClassSelection();
        clampConfigureThingSelection();
    } else if (m_mainView == MainView::Settings) {
        clampSettingsDetailsSelection();
    } else if (m_mainView == MainView::ApiBrowser) {
        clampApiBrowserSelection();
        clampApiBrowserReferenceSelection();
        clampApiBrowserJsonSelection();
    } else if (m_mainView == MainView::Help) {
        clampHelpSelection();
    }
    if (m_actionExecutionPending || m_configureRequestPending || m_logView.fetchPending || m_logView.setLoggingPending) {
        if (ftxui::ScreenInteractive* screen = ftxui::ScreenInteractive::Active(); screen != nullptr) {
            screen->RequestAnimationFrame();
        }
    }

    if (m_showLoginForm) {
        ftxui::Elements sections;
        sections.push_back(ftxui::hbox({
            ftxui::text(" nymea-cli (" + m_options.appVersion + ")"),
            ftxui::filler(),
            ftxui::text(endpoint() + " " + m_serverName.toStdString() + " | " + m_serverVersion + " | API " + m_serverApiVersion + " "),
        }));
        sections.push_back(ftxui::separator());
        sections.push_back(ftxui::filler());
        ftxui::Elements loginBody;
        if (m_pushButtonAuthAvailable) {
            loginBody.push_back(ftxui::text("Please press the push button for authentication.") | ftxui::bold);
            loginBody.push_back(ftxui::separator());
            if (!m_authStatus.empty()) {
                loginBody.push_back(ftxui::text(m_authStatus));
            }
            if (m_pushButtonAuthPending) {
                loginBody.push_back(ftxui::text("Waiting for push-button authentication to finish...") | ftxui::dim);
            }
            loginBody.push_back(ftxui::text("Enter requests push-button authentication again, Esc quits.") | ftxui::dim);
        } else {
            loginBody.push_back(ftxui::hbox({
                ftxui::text("Username: "),
                renderActiveField(m_usernameInput->Render() | ftxui::xflex, m_loginSelectedInputIndex == 0, 26),
            }));
            loginBody.push_back(ftxui::hbox({
                ftxui::text("Password: "),
                renderActiveField(m_passwordInput->Render() | ftxui::xflex, m_loginSelectedInputIndex == 1, 26),
            }));
            loginBody.push_back(ftxui::separator());
            if (!m_authStatus.empty()) {
                loginBody.push_back(ftxui::text(m_authStatus));
            }
            if (!m_logoutStatus.empty()) {
                loginBody.push_back(ftxui::paragraph(m_logoutStatus) | ftxui::dim);
            }
            loginBody.push_back(ftxui::text("Enter submits credentials, Esc quits.") | ftxui::dim);
        }
        sections.push_back(ftxui::hbox({
            ftxui::filler(),
            renderFocusedWindow(ftxui::text("Authentication required"), ftxui::vbox(std::move(loginBody)) | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 56), m_showLoginForm),
            ftxui::filler(),
        }));
        sections.push_back(ftxui::filler());
        return ftxui::vbox(std::move(sections)) | ftxui::flex;
    }

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
        ftxui::text(endpoint() + " " + m_serverName.toStdString() + " | " + m_serverVersion + " | API " + m_serverApiVersion + " "),
    }));
    const bool showTabs = !m_logView.visible && m_mainView != MainView::Help;
    sections.push_back(showTabs && m_focusArea == FocusArea::MainMenu ? ftxui::separator() | ftxui::color(ftxui::Color::SteelBlue) : ftxui::separator());
    if (showTabs) {
        sections.push_back(renderMainMenu());
    } else {
        m_mainMenuBox = {};
    }
    if (!m_settingsWarning.empty()) {
        sections.push_back(ftxui::text(m_settingsWarning) | ftxui::color(ftxui::Color::Yellow));
    }
    if (m_logView.visible) {
        sections.push_back(renderLogView() | ftxui::flex);
        return ftxui::vbox(std::move(sections)) | ftxui::border | ftxui::flex;
    }
    if (m_mainView == MainView::Help) {
        sections.push_back(renderHelp() | ftxui::flex);
        return ftxui::vbox(std::move(sections)) | ftxui::border | ftxui::flex;
    }
    std::string keyHintLine = "Keys: Tab/Shift+Tab switch tabs, Up/Down navigate, Left/Right switch panels, / filter, s sort, f category, Space inspector, l logs, c reconnect, "
                              "t refresh things, ?/h help, Enter opens actions/setup, q/Esc quit";
    if (m_mainView == MainView::ApiBrowser) {
        keyHintLine = "Keys: Tab/Shift+Tab switch tabs, Up/Down navigate, Left back, Right switch browser panes, Enter follows a reference, / filter, c reconnect, t refresh "
                      "things, ?/h help, q/Esc quit";
    } else if (m_mainView == MainView::Settings) {
        keyHintLine = "Keys: Tab/Shift+Tab switch tabs, Up/Down select settings, Right/Enter open details, Enter applies/edits, "
                      "Server interfaces/Modbus RTU a/e/d/r add/edit/delete/refresh, Left returns, ?/h help, q/Esc quit";
    } else if (m_mainView == MainView::Logout) {
        keyHintLine = "Keys: Tab/Shift+Tab switch tabs, Enter logs out, ?/h help, q/Esc quit";
    } else if (m_mainView == MainView::About) {
        keyHintLine = "Keys: Tab/Shift+Tab switch tabs, ?/h help, q/Esc quit";
    }

    ftxui::Element rightPanel;
    if (m_mainView == MainView::Things) {
        rightPanel = renderThings() | ftxui::flex;
    } else if (m_mainView == MainView::ConfigureThings) {
        rightPanel = ftxui::hbox({
                         renderConfigureMenu() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 28),
                         renderConfigureDetails() | ftxui::flex,
                     })
                     | ftxui::flex;
    } else if (m_mainView == MainView::ApiBrowser) {
        rightPanel = renderApiBrowser() | ftxui::flex;
    } else if (m_mainView == MainView::Logout) {
        rightPanel = renderLogout() | ftxui::flex;
    } else if (m_mainView == MainView::About) {
        rightPanel = renderAbout() | ftxui::flex;
    } else {
        rightPanel = ftxui::hbox({
                         renderSettingsMenu() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 28),
                         renderSettingsDetails() | ftxui::flex,
                     })
                     | ftxui::flex;
    }

    sections.push_back(rightPanel | ftxui::flex);

    if (m_showLogoutConfirm) {
        ftxui::Elements dialogBody;
        dialogBody.push_back(ftxui::text("Warning") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
        dialogBody.push_back(ftxui::separator());
        dialogBody.push_back(ftxui::paragraph("This will revoke the current token on the server, forget the saved token locally, and reconnect to the same endpoint."));
        if (!m_logoutStatus.empty()) {
            dialogBody.push_back(ftxui::separator());
            dialogBody.push_back(ftxui::text(m_logoutStatus));
            dialogBody.push_back(ftxui::text("You will be returned to the login form if authentication is required.") | ftxui::dim);
        }
        dialogBody.push_back(ftxui::separator());
        dialogBody.push_back(ftxui::text(m_logoutRequestPending ? "Revoking token and reconnecting..." : "Enter confirms logout, Esc cancels.") | ftxui::dim);
        sections.push_back(ftxui::separator());
        sections.push_back(renderFocusedWindow(ftxui::text("Confirm logout"), ftxui::vbox(std::move(dialogBody)), m_showLogoutConfirm));
    }

    if (m_showSystemActionConfirm) {
        ftxui::Elements dialogBody;
        dialogBody.push_back(ftxui::text("Warning") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
        dialogBody.push_back(ftxui::separator());
        dialogBody.push_back(ftxui::paragraph("This will request a " + powerActionLabel(static_cast<int>(m_systemAction)) + " on the server."));
        if (!m_systemActionStatus.empty()) {
            dialogBody.push_back(ftxui::separator());
            dialogBody.push_back(ftxui::text(m_systemActionStatus));
        }
        dialogBody.push_back(ftxui::separator());
        dialogBody.push_back(ftxui::text(m_systemActionRequestPending ? "Sending request..." : "Enter confirms, Esc or Left cancels.") | ftxui::dim);
        sections.push_back(ftxui::separator());
        sections.push_back(
            renderFocusedWindow(ftxui::text(powerActionLabel(static_cast<int>(m_systemAction)) + " confirmation"), ftxui::vbox(std::move(dialogBody)), m_showSystemActionConfirm));
    }

    if (m_serverInterfaceDialogMode != ServerInterfaceDialogMode::None) {
        ftxui::Elements dialogBody;
        const bool removeConfirm = m_serverInterfaceDialogMode == ServerInterfaceDialogMode::RemoveConfirm;
        if (removeConfirm) {
            dialogBody.push_back(ftxui::text("Warning") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
            dialogBody.push_back(ftxui::separator());
            dialogBody.push_back(ftxui::text("Type: " + serverInterfaceTypeLabel(m_serverInterfaceDialogType)));
            dialogBody.push_back(ftxui::text("ID: " + m_serverInterfaceDialogId));
            dialogBody.push_back(ftxui::text("Endpoint: " + m_serverInterfaceDialogAddress + ":" + m_serverInterfaceDialogPort));
            if (m_serverInterfaceDialogType == ServerInterfaceType::Tcp || m_serverInterfaceDialogType == ServerInterfaceType::WebSocket
                || m_serverInterfaceDialogType == ServerInterfaceType::TunnelProxy) {
                dialogBody.push_back(ftxui::paragraph("Changing or removing the interface used by this connection may disconnect the CLI.") | ftxui::color(ftxui::Color::Yellow));
            }
            dialogBody.push_back(ftxui::separator());
            dialogBody.push_back(ftxui::text(m_serverInterfaceRequestPending ? "Removing..." : "Enter confirms removal, Esc cancels.") | ftxui::dim);
        } else {
            auto pushField = [&](int index, const std::string& label, const std::string& value) {
                const bool selected = m_focusArea == FocusArea::ServerInterfaceDialog && m_serverInterfaceDialogFieldIndex == index;
                dialogBody.push_back(renderDialogFieldRow(selected, label, value, 56));
            };
            auto boolText = [](bool value) { return std::string("< ") + (value ? "on" : "off") + " >"; };

            dialogBody.push_back(ftxui::text("ID: " + m_serverInterfaceDialogId) | ftxui::dim);
            int fieldIndex = 0;
            if (m_serverInterfaceDialogMode == ServerInterfaceDialogMode::Add) {
                pushField(fieldIndex++, "Type", "< " + serverInterfaceTypeLabel(m_serverInterfaceDialogType) + " >");
            }
            pushField(fieldIndex++, "Address", m_serverInterfaceDialogAddress.empty() ? "<empty>" : m_serverInterfaceDialogAddress);
            pushField(fieldIndex++, "Port", m_serverInterfaceDialogPort.empty() ? "<empty>" : m_serverInterfaceDialogPort);
            pushField(fieldIndex++, "SSL", boolText(m_serverInterfaceDialogSslEnabled));
            pushField(fieldIndex++, "Authentication", boolText(m_serverInterfaceDialogAuthenticationEnabled));
            if (m_serverInterfaceDialogType == ServerInterfaceType::WebServer) {
                pushField(fieldIndex++, "Public folder", m_serverInterfaceDialogPublicFolder.empty() ? "<empty>" : m_serverInterfaceDialogPublicFolder);
            } else if (m_serverInterfaceDialogType == ServerInterfaceType::TunnelProxy) {
                pushField(fieldIndex++, "Ignore SSL errors", boolText(m_serverInterfaceDialogIgnoreSslErrors));
            }
            dialogBody.push_back(ftxui::separator());
            if (m_serverInterfaceDialogType == ServerInterfaceType::Tcp || m_serverInterfaceDialogType == ServerInterfaceType::WebSocket
                || m_serverInterfaceDialogType == ServerInterfaceType::TunnelProxy) {
                dialogBody.push_back(ftxui::paragraph("Changing the interface used by this connection may disconnect the CLI.") | ftxui::color(ftxui::Color::Yellow));
            }
            if (!m_serverInterfaceStatus.empty()) {
                dialogBody.push_back(ftxui::paragraph(m_serverInterfaceStatus));
            }
            dialogBody.push_back(ftxui::text(m_serverInterfaceRequestPending
                                                 ? "Sending request..."
                                                 : "Up/Down moves, Left/Right or Space cycles selectors, type edits text fields, Enter saves, Esc cancels.")
                                 | ftxui::dim);
        }

        const std::string title = m_serverInterfaceDialogMode == ServerInterfaceDialogMode::Add
                                      ? "Add server interface"
                                      : (m_serverInterfaceDialogMode == ServerInterfaceDialogMode::Edit ? "Edit server interface" : "Remove server interface");
        sections.push_back(ftxui::separator());
        sections.push_back(renderFocusedWindow(ftxui::text(title),
                                               ftxui::vbox(std::move(dialogBody)) | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 76),
                                               m_focusArea == FocusArea::ServerInterfaceDialog));
    }

    if (m_modbusRtuDialogMode != ModbusRtuDialogMode::None) {
        ftxui::Elements dialogBody;
        const bool removeConfirm = m_modbusRtuDialogMode == ModbusRtuDialogMode::RemoveConfirm;
        if (removeConfirm) {
            dialogBody.push_back(ftxui::text("Warning") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
            dialogBody.push_back(ftxui::separator());
            dialogBody.push_back(ftxui::text("Serial port: " + m_modbusRtuDialogSerialPort));
            dialogBody.push_back(ftxui::text("UUID: " + m_modbusRtuDialogUuid.toString(QUuid::WithoutBraces).toStdString()));
            dialogBody.push_back(ftxui::separator());
            dialogBody.push_back(ftxui::text(m_modbusRtuRequestPending ? "Removing..." : "Enter confirms removal, Esc cancels.") | ftxui::dim);
        } else {
            auto pushField = [&](int index, const std::string& label, const std::string& value) {
                const bool selected = m_focusArea == FocusArea::ModbusRtuDialog && m_modbusRtuDialogFieldIndex == index;
                dialogBody.push_back(renderDialogFieldRow(selected, label, value, 52));
            };

            const auto& dataBits = dataBitsOptions();
            const auto& baudrates = baudrateOptions();
            const auto& parity = parityOptions();
            const auto& stopBits = stopBitsOptions();
            pushField(0, "Serial port", m_modbusRtuDialogSerialPort.empty() ? "<type or cycle discovered ports>" : m_modbusRtuDialogSerialPort);
            pushField(1, "Baudrate", QString::number(baudrates.at(m_modbusRtuDialogBaudrateIndex)).toStdString());
            pushField(2, "Data bits", dataBitsLabel(dataBits.at(m_modbusRtuDialogDataBitsIndex)));
            pushField(3, "Parity", parityLabel(parity.at(m_modbusRtuDialogParityIndex)));
            pushField(4, "Stop bits", stopBitsLabel(stopBits.at(m_modbusRtuDialogStopBitsIndex)));
            pushField(5, "Timeout ms", m_modbusRtuDialogTimeout);
            pushField(6, "Retries", m_modbusRtuDialogRetries);
            dialogBody.push_back(ftxui::separator());
            if (!m_modbusRtuStatus.empty()) {
                dialogBody.push_back(ftxui::paragraph(m_modbusRtuStatus));
            }
            dialogBody.push_back(ftxui::text(m_modbusRtuRequestPending
                                                 ? "Sending request..."
                                                 : "Up/Down moves, Left/Right cycles serial port and selectors, type edits text fields, Enter saves, Esc cancels.")
                                 | ftxui::dim);
        }

        const std::string title = m_modbusRtuDialogMode == ModbusRtuDialogMode::Add
                                      ? "Add Modbus RTU master"
                                      : (m_modbusRtuDialogMode == ModbusRtuDialogMode::Edit ? "Edit Modbus RTU master" : "Remove Modbus RTU master");
        sections.push_back(ftxui::separator());
        sections.push_back(renderFocusedWindow(ftxui::text(title),
                                               ftxui::vbox(std::move(dialogBody)) | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 72),
                                               m_focusArea == FocusArea::ModbusRtuDialog));
    }

    if (m_showActionDialog) {
        ftxui::Elements dialogBody;
        dialogBody.push_back(ftxui::text("Action: " + m_actionDialogActionName));
        dialogBody.push_back(ftxui::separator());

        if (m_actionDialogParamTypes.empty()) {
            dialogBody.push_back(ftxui::text("This action has no params."));
        } else {
            for (int index = 0; index < static_cast<int>(m_actionDialogParamTypes.size()); ++index) {
                const api::ParamType& paramType = m_actionDialogParamTypes.at(index);
                const std::string label = firstNonEmpty({paramType.displayName.toStdString(), paramType.name.toStdString(), "<param>"});
                dialogBody.push_back(renderTwoColumnRow(label,
                                                        renderActionDialogValueCell(paramType, m_actionDialogParamValues.at(index)),
                                                        index == m_actionDialogSelectedParamIndex,
                                                        m_focusArea == FocusArea::ActionDialog));
            }
        }

        ftxui::Elements dialogFooter;
        dialogFooter.push_back(ftxui::text(m_actionDialogStatus));
        if (!m_actionDialogParamTypes.empty() && m_actionDialogSelectedParamIndex >= 0 && m_actionDialogSelectedParamIndex < static_cast<int>(m_actionDialogParamTypes.size())) {
            const api::ParamType& selectedParamType = m_actionDialogParamTypes.at(m_actionDialogSelectedParamIndex);
            if (actionParamUsesSelector(selectedParamType)) {
                dialogFooter.push_back(ftxui::text("Up/Down select param, Left/Right/Space choose value, Enter execute, Esc close.") | ftxui::dim);
            } else if (actionParamUsesRangeInput(selectedParamType)) {
                dialogFooter.push_back(ftxui::text(rangeInputSummary(selectedParamType)));
                if (const std::optional<NumericRangeInputSpec> rangeSpec = numericRangeInputSpec(selectedParamType);
                    rangeSpec.has_value()
                    && !::nymea::currentNumericRangeValueFromRaw(selectedParamType, *rangeSpec, m_actionDialogParamValues.at(m_actionDialogSelectedParamIndex)).has_value()
                    && !m_actionDialogParamValues.at(m_actionDialogSelectedParamIndex).empty()) {
                    dialogFooter.push_back(ftxui::text("Current value is incomplete or invalid. Type a number or use Left/Right to snap into range.")
                                           | ftxui::color(ftxui::Color::YellowLight));
                }
                dialogFooter.push_back(ftxui::text("Type a number, Left/Right adjust the bar, Up/Down select param, Enter execute, Esc close.") | ftxui::dim);
            } else {
                dialogFooter.push_back(ftxui::text("Type to edit, Up/Down select param, Enter execute, Esc close.") | ftxui::dim);
            }
        } else {
            dialogFooter.push_back(ftxui::text("Enter executes, Esc closes.") | ftxui::dim);
        }

        if (!m_lastActionExecutionStatus.empty()) {
            dialogFooter.push_back(ftxui::separator());
            dialogFooter.push_back(
                ftxui::hbox({
                    ftxui::text(" " + m_lastActionExecutionStatus),
                    ftxui::filler(),
                })
                | ftxui::bold | ftxui::color(ftxui::Color::Black)
                | ftxui::bgcolor(m_actionExecutionPending ? ftxui::Color::CyanLight : (m_lastActionExecutionStatusWarning ? ftxui::Color::YellowLight : ftxui::Color::GreenLight)));
        }

        sections.push_back(ftxui::separator());
        sections.push_back(renderFocusedWindow(ftxui::text("Execute action"),
                                               ftxui::vbox({
                                                   ftxui::vbox(std::move(dialogBody)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::flex,
                                                   ftxui::separator(),
                                                   ftxui::vbox(std::move(dialogFooter)),
                                               }) | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 80),
                                               m_showActionDialog && m_focusArea == FocusArea::ActionDialog));
    }

    if (m_showConfigureDialog) {
        ftxui::Elements dialogBody;
        if (const api::ThingClass* thingClass = m_configureThingClassId.isNull() ? nullptr : m_thingManager.thingClassById(m_configureThingClassId); thingClass != nullptr) {
            dialogBody.push_back(ftxui::text("Thing class: " + thingClassLabel(*thingClass)));
            dialogBody.push_back(ftxui::separator());
        }

        switch (m_configureDialogMode) {
        case ConfigureDialogMode::AddChooseCreateMethod:
            for (int index = 0; index < static_cast<int>(m_configureCreateMethodOptions.size()); ++index) {
                const bool selected = index == m_configureCreateMethodIndex;
                auto row = ftxui::text(" " + createMethodLabel(m_configureCreateMethodOptions.at(index)) + " ");
                if (selected) {
                    row = row | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight) | ftxui::focus;
                }
                dialogBody.push_back(row);
            }
            break;
        case ConfigureDialogMode::AddManualParams: {
            dialogBody.push_back(renderTwoColumnRow("Name",
                                                    ftxui::text(m_configureThingName.empty() ? std::string("<empty>") : m_configureThingName),
                                                    m_configureParamSelectionIndex == 0,
                                                    m_focusArea == FocusArea::ConfigureDialog));
            for (int index = 0; index < static_cast<int>(m_configureParamTypes.size()); ++index) {
                const api::ParamType& paramType = m_configureParamTypes.at(index);
                dialogBody.push_back(renderTwoColumnRow(firstNonEmpty({paramType.displayName.toStdString(), paramType.name.toStdString(), "<param>"}),
                                                        renderActionDialogValueCell(paramType, m_configureParamValues.at(index)),
                                                        m_configureParamSelectionIndex == index + 1,
                                                        m_focusArea == FocusArea::ConfigureDialog));
            }
            break;
        }
        case ConfigureDialogMode::AddDiscoveryParams:
            if (m_configureParamTypes.empty()) {
                dialogBody.push_back(ftxui::text("No discovery params required."));
            } else {
                for (int index = 0; index < static_cast<int>(m_configureParamTypes.size()); ++index) {
                    const api::ParamType& paramType = m_configureParamTypes.at(index);
                    dialogBody.push_back(renderTwoColumnRow(firstNonEmpty({paramType.displayName.toStdString(), paramType.name.toStdString(), "<param>"}),
                                                            renderActionDialogValueCell(paramType, m_configureParamValues.at(index)),
                                                            m_configureParamSelectionIndex == index,
                                                            m_focusArea == FocusArea::ConfigureDialog));
                }
            }
            break;
        case ConfigureDialogMode::AddDiscoveryResults:
            for (int index = 0; index < static_cast<int>(m_configureThingDescriptors.size()); ++index) {
                const api::ThingDescriptor& descriptor = m_configureThingDescriptors.at(index);
                auto row = ftxui::text(" " + descriptorLabel(descriptor) + " ");
                if (index == m_configureThingDescriptorIndex) {
                    row = row | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight) | ftxui::focus;
                }
                dialogBody.push_back(row);
                if (!descriptor.description.isEmpty()) {
                    dialogBody.push_back(ftxui::paragraph("  " + descriptor.description.toStdString()) | ftxui::dim);
                }
            }
            break;
        case ConfigureDialogMode::AddPairingConfirmation: {
            if (!m_configurePairingDisplayMessage.empty()) {
                dialogBody.push_back(ftxui::paragraph(m_configurePairingDisplayMessage));
                dialogBody.push_back(ftxui::separator());
            }
            if (!m_configurePairingPin.empty()) {
                dialogBody.push_back(ftxui::text("PIN: " + m_configurePairingPin));
            }
            if (!m_configurePairingOauthUrl.empty()) {
                dialogBody.push_back(ftxui::paragraph("OAuth URL: " + m_configurePairingOauthUrl));
            }

            int fieldIndex = 0;
            if (m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword) {
                dialogBody.push_back(renderTwoColumnRow("Username",
                                                        ftxui::text(m_configurePairingUsername.empty() ? std::string("<empty>") : m_configurePairingUsername),
                                                        m_configureParamSelectionIndex == fieldIndex++,
                                                        m_focusArea == FocusArea::ConfigureDialog));
            }
            if (m_configureSetupMethod == api::SetupMethod::SetupMethodDisplayPin || m_configureSetupMethod == api::SetupMethod::SetupMethodEnterPin
                || m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword || m_configureSetupMethod == api::SetupMethod::SetupMethodOAuth) {
                const std::string label = m_configureSetupMethod == api::SetupMethod::SetupMethodOAuth
                                              ? "Redirect URL"
                                              : (m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword ? "Password" : "Secret");
                dialogBody.push_back(renderTwoColumnRow(label,
                                                        ftxui::text(m_configurePairingSecret.empty() ? std::string("<empty>") : m_configurePairingSecret),
                                                        m_configureParamSelectionIndex == fieldIndex,
                                                        m_focusArea == FocusArea::ConfigureDialog));
            } else if (m_configureSetupMethod == api::SetupMethod::SetupMethodPushButton) {
                dialogBody.push_back(ftxui::text("No additional input required."));
            }
            break;
        }
        case ConfigureDialogMode::RemoveThingConfirm:
            dialogBody.push_back(ftxui::paragraph("Remove thing " + uuidToStd(m_configureTargetThingId) + "?"));
            break;
        case ConfigureDialogMode::RenameThing:
            dialogBody.push_back(renderTwoColumnRow("Name",
                                                    ftxui::text(m_configureThingName.empty() ? std::string("<empty>") : m_configureThingName),
                                                    true,
                                                    m_focusArea == FocusArea::ConfigureDialog));
            break;
        case ConfigureDialogMode::ReconfigureThingInfo:
            dialogBody.push_back(ftxui::paragraph(m_configureDialogStatus));
            break;
        case ConfigureDialogMode::None:
            break;
        }

        ftxui::Elements dialogFooter;
        const std::string status = m_configureRequestPending ? busyIndicator(m_configurePendingStartedAt) + " " + m_configureDialogStatus : m_configureDialogStatus;
        if (!status.empty()) {
            dialogFooter.push_back(ftxui::text(status));
        }
        if (m_configureFlowComplete) {
            dialogFooter.push_back(ftxui::text("Enter or Esc closes this wizard.") | ftxui::dim);
        } else {
            dialogFooter.push_back(ftxui::text(m_configureRequestPending ? "Waiting for server reply..." : "Enter confirms, Esc closes.") | ftxui::dim);
        }

        if (!m_lastConfigureExecutionStatus.empty()) {
            dialogFooter.push_back(ftxui::separator());
            dialogFooter.push_back(ftxui::hbox({
                                       ftxui::text(" " + m_lastConfigureExecutionStatus),
                                       ftxui::filler(),
                                   })
                                   | ftxui::bold | ftxui::color(ftxui::Color::Black)
                                   | ftxui::bgcolor(m_configureRequestPending ? ftxui::Color::CyanLight
                                                                              : (m_lastConfigureExecutionStatusWarning ? ftxui::Color::YellowLight : ftxui::Color::GreenLight)));
        }

        sections.push_back(ftxui::separator());
        sections.push_back(renderFocusedWindow(ftxui::text(m_configureDialogTitle.empty() ? "Configure thing" : m_configureDialogTitle),
                                               ftxui::vbox({
                                                   ftxui::vbox(std::move(dialogBody)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::flex,
                                                   ftxui::separator(),
                                                   ftxui::vbox(std::move(dialogFooter)),
                                               }) | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 80),
                                               m_showConfigureDialog && m_focusArea == FocusArea::ConfigureDialog));
    }

    sections.push_back(ftxui::separator());
    sections.push_back(ftxui::text(keyHintLine) | ftxui::dim);

    return ftxui::vbox(std::move(sections)) | ftxui::border | ftxui::flex;
}

} // namespace nymea
