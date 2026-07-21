// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

namespace nymea {

bool Engine::editDialogTextField(std::string& value, const ftxui::Event& event, bool digitsOnly)
{
    if (event == ftxui::Event::Backspace && !value.empty()) {
        value.pop_back();
        return true;
    }
    if (event.is_character()) {
        if (!digitsOnly || std::all_of(event.character().begin(), event.character().end(), [](const char ch) { return ch >= '0' && ch <= '9'; })) {
            value += event.character();
            return true;
        }
    }
    return false;
}

bool Engine::handleEvent(const ftxui::Event& event, ftxui::ScreenInteractive& screen)
{
    auto applyMainMenuSelection = [this](MainMenuEntry entry) {
        m_selectedMainMenuEntry = entry;
        switch (entry) {
        case MainMenuEntry::Things:
            m_mainView = MainView::Things;
            break;
        case MainMenuEntry::ConfigureThings:
            m_mainView = MainView::ConfigureThings;
            if (!m_haveAllThingClasses && !m_fetchAllThingClassesPending) {
                fetchAllThingClasses();
            }
            break;
        case MainMenuEntry::ApiBrowser:
            m_mainView = MainView::ApiBrowser;
            m_apiBrowserJsonLineIndex = 0;
            ensureApiBrowserLoaded();
            clampApiBrowserSelection();
            clampApiBrowserReferenceSelection();
            break;
        case MainMenuEntry::Settings:
            m_mainView = MainView::Settings;
            m_settingsDetailsLineIndex = 0;
            ensureSystemCapabilitiesLoaded();
            ensureSystemTimeLoaded();
            ensureSystemUpdateStatusLoaded();
            if (m_settingsView == SettingsView::Timezone) {
                ensureSystemTimeZonesLoaded();
            } else if (m_settingsView == SettingsView::Update) {
                ensureSystemPackagesLoaded();
            } else if (m_settingsView == SettingsView::LoggingCategories) {
                ensureLoggingCategoriesLoaded();
            } else if (m_settingsView == SettingsView::ServerInterfaces) {
                ensureServerInterfacesLoaded();
            } else if (m_settingsView == SettingsView::ModbusRtu) {
                ensureModbusRtuLoaded();
            }
            break;
        case MainMenuEntry::Logout:
            m_mainView = MainView::Logout;
            break;
        case MainMenuEntry::About:
            m_mainView = MainView::About;
            break;
        }
    };
    auto syncMainMenuSelectionToCurrentView = [this]() {
        switch (m_mainView) {
        case MainView::Things:
            m_selectedMainMenuEntry = MainMenuEntry::Things;
            break;
        case MainView::ConfigureThings:
            m_selectedMainMenuEntry = MainMenuEntry::ConfigureThings;
            break;
        case MainView::ApiBrowser:
            m_selectedMainMenuEntry = MainMenuEntry::ApiBrowser;
            break;
        case MainView::Settings:
            m_selectedMainMenuEntry = MainMenuEntry::Settings;
            break;
        case MainView::Logout:
            m_selectedMainMenuEntry = MainMenuEntry::Logout;
            break;
        case MainView::About:
            m_selectedMainMenuEntry = MainMenuEntry::About;
            break;
        case MainView::Help:
            break;
        }
    };

    if (event == ftxui::Event::Custom) {
        drainUiTasks();
        return true;
    }

    if (handleMouseWheel(event)) {
        return true;
    }

    if (m_connectionLost) {
        if (event == ftxui::Event::Character("c")) {
            connectToServer();
            if (m_client.isConnected()) {
                runHandshakeAndLoadThings();
            }
            return true;
        }
        if (event == ftxui::Event::Character("q") || event == ftxui::Event::Escape) {
            m_client.disconnectFromHost();
            screen.ExitLoopClosure()();
            return true;
        }
        return true;
    }

    if (m_logView.visible) {
        if (event == ftxui::Event::Escape || event == ftxui::Event::Character("q")) {
            closeLogView();
            return true;
        }
        if (event == ftxui::Event::Character("h")) {
            setLogViewRange(LogViewRange::Hour);
            return true;
        }
        if (event == ftxui::Event::Character("d")) {
            setLogViewRange(LogViewRange::Day);
            return true;
        }
        if (event == ftxui::Event::Character("w")) {
            setLogViewRange(LogViewRange::Week);
            return true;
        }
        if (event == ftxui::Event::Character("m")) {
            setLogViewRange(LogViewRange::Month);
            return true;
        }
        if (event == ftxui::Event::Character("y")) {
            setLogViewRange(LogViewRange::Year);
            return true;
        }
        if (event == ftxui::Event::ArrowLeft) {
            m_logView.followLatest = false;
            stepLogViewWindow(-1);
            return true;
        }
        if (event == ftxui::Event::ArrowRight) {
            stepLogViewWindow(1);
            return true;
        }
        if (event == ftxui::Event::Character("x")) {
            toggleLogViewLogging();
            return true;
        }
        if (event == ftxui::Event::Character("f")) {
            m_logView.followLatest = !m_logView.followLatest;
            if (m_logView.followLatest) {
                m_logView.windowEnd = QDateTime::currentDateTime();
                if (!m_logView.listEntries.empty()) {
                    m_logView.listSelectionIndex = static_cast<int>(m_logView.listEntries.size()) - 1;
                }
                fetchLogViewData();
            }
            return true;
        }
        if (!m_logView.listEntries.empty()) {
            if (event == ftxui::Event::ArrowUp) {
                m_logView.followLatest = false;
                m_logView.listSelectionIndex = std::max(0, m_logView.listSelectionIndex - 1);
                return true;
            }
            if (event == ftxui::Event::ArrowDown) {
                m_logView.listSelectionIndex = std::min(static_cast<int>(m_logView.listEntries.size()) - 1, m_logView.listSelectionIndex + 1);
                return true;
            }
        }
        return true;
    }

    if (m_showActionDialog) {
        if (m_actionExecutionPending) {
            return true;
        }
        if (event == ftxui::Event::Escape) {
            closeActionDialog();
            return true;
        }
        if (event == ftxui::Event::Return) {
            return executeCurrentAction();
        }
        if (event == ftxui::Event::ArrowUp && !m_actionDialogParamTypes.empty()) {
            m_actionDialogSelectedParamIndex = (m_actionDialogSelectedParamIndex + static_cast<int>(m_actionDialogParamTypes.size()) - 1)
                                               % static_cast<int>(m_actionDialogParamTypes.size());
            m_actionDialogRangeEditIndex.reset();
            return true;
        }
        if (event == ftxui::Event::ArrowDown && !m_actionDialogParamTypes.empty()) {
            m_actionDialogSelectedParamIndex = (m_actionDialogSelectedParamIndex + 1) % static_cast<int>(m_actionDialogParamTypes.size());
            m_actionDialogRangeEditIndex.reset();
            return true;
        }
        if (!m_actionDialogParamTypes.empty() && m_actionDialogSelectedParamIndex >= 0 && m_actionDialogSelectedParamIndex < static_cast<int>(m_actionDialogParamValues.size())) {
            const api::ParamType& currentParamType = m_actionDialogParamTypes.at(m_actionDialogSelectedParamIndex);
            std::string& currentValue = m_actionDialogParamValues.at(m_actionDialogSelectedParamIndex);
            bool isRangeTextEditing = m_actionDialogRangeEditIndex.has_value() && *m_actionDialogRangeEditIndex == m_actionDialogSelectedParamIndex;
            if (handleParamValueEditEvent(event, currentParamType, currentValue, isRangeTextEditing)) {
                if (actionParamUsesRangeInput(currentParamType) && isRangeTextEditing) {
                    m_actionDialogRangeEditIndex = m_actionDialogSelectedParamIndex;
                } else {
                    m_actionDialogRangeEditIndex.reset();
                }
                return true;
            }
        }

        return true;
    }

    if (m_showConfigureDialog) {
        if (m_configureRequestPending) {
            return true;
        }

        if (event == ftxui::Event::Escape) {
            closeConfigureDialog();
            return true;
        }

        if (m_configureFlowComplete) {
            if (event == ftxui::Event::Return) {
                closeConfigureDialog();
            }
            return true;
        }

        if (event == ftxui::Event::Return) {
            return submitConfigureDialog();
        }

        auto editText = [&](std::string& value) {
            if (event == ftxui::Event::Backspace && !value.empty()) {
                value.pop_back();
                return true;
            }
            if (event.is_character()) {
                value += event.character();
                return true;
            }
            return false;
        };

        switch (m_configureDialogMode) {
        case ConfigureDialogMode::AddChooseCreateMethod:
            if (event == ftxui::Event::ArrowUp && !m_configureCreateMethodOptions.empty()) {
                m_configureCreateMethodIndex = (m_configureCreateMethodIndex + static_cast<int>(m_configureCreateMethodOptions.size()) - 1)
                                               % static_cast<int>(m_configureCreateMethodOptions.size());
                return true;
            }
            if (event == ftxui::Event::ArrowDown && !m_configureCreateMethodOptions.empty()) {
                m_configureCreateMethodIndex = (m_configureCreateMethodIndex + 1) % static_cast<int>(m_configureCreateMethodOptions.size());
                return true;
            }
            break;
        case ConfigureDialogMode::AddManualParams: {
            const int rowCount = 1 + static_cast<int>(m_configureParamTypes.size());
            if (event == ftxui::Event::ArrowUp && rowCount > 0) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + rowCount - 1) % rowCount;
                m_configureRangeEditIndex.reset();
                return true;
            }
            if (event == ftxui::Event::ArrowDown && rowCount > 0) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + 1) % rowCount;
                m_configureRangeEditIndex.reset();
                return true;
            }
            if (m_configureParamSelectionIndex == 0) {
                m_configureRangeEditIndex.reset();
                return editText(m_configureThingName);
            }

            if (m_configureParamSelectionIndex - 1 >= 0 && m_configureParamSelectionIndex - 1 < static_cast<int>(m_configureParamTypes.size())) {
                const api::ParamType& currentParamType = m_configureParamTypes.at(m_configureParamSelectionIndex - 1);
                std::string& currentValue = m_configureParamValues.at(m_configureParamSelectionIndex - 1);
                const int currentParamIndex = m_configureParamSelectionIndex - 1;
                bool isRangeTextEditing = m_configureRangeEditIndex.has_value() && *m_configureRangeEditIndex == currentParamIndex;
                if (handleParamValueEditEvent(event, currentParamType, currentValue, isRangeTextEditing)) {
                    if (actionParamUsesRangeInput(currentParamType) && isRangeTextEditing) {
                        m_configureRangeEditIndex = currentParamIndex;
                    } else {
                        m_configureRangeEditIndex.reset();
                    }
                    return true;
                }
            }
            return true;
        }
        case ConfigureDialogMode::AddDiscoveryParams:
            if (event == ftxui::Event::ArrowUp && !m_configureParamTypes.empty()) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + static_cast<int>(m_configureParamTypes.size()) - 1)
                                                 % static_cast<int>(m_configureParamTypes.size());
                m_configureRangeEditIndex.reset();
                return true;
            }
            if (event == ftxui::Event::ArrowDown && !m_configureParamTypes.empty()) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + 1) % static_cast<int>(m_configureParamTypes.size());
                m_configureRangeEditIndex.reset();
                return true;
            }
            if (!m_configureParamTypes.empty()) {
                const api::ParamType& currentParamType = m_configureParamTypes.at(m_configureParamSelectionIndex);
                std::string& currentValue = m_configureParamValues.at(m_configureParamSelectionIndex);
                bool isRangeTextEditing = m_configureRangeEditIndex.has_value() && *m_configureRangeEditIndex == m_configureParamSelectionIndex;
                if (handleParamValueEditEvent(event, currentParamType, currentValue, isRangeTextEditing)) {
                    if (actionParamUsesRangeInput(currentParamType) && isRangeTextEditing) {
                        m_configureRangeEditIndex = m_configureParamSelectionIndex;
                    } else {
                        m_configureRangeEditIndex.reset();
                    }
                    return true;
                }
            }
            return true;
        case ConfigureDialogMode::AddDiscoveryResults:
            if (event == ftxui::Event::ArrowUp && !m_configureThingDescriptors.empty()) {
                m_configureThingDescriptorIndex = (m_configureThingDescriptorIndex + static_cast<int>(m_configureThingDescriptors.size()) - 1)
                                                  % static_cast<int>(m_configureThingDescriptors.size());
                return true;
            }
            if (event == ftxui::Event::ArrowDown && !m_configureThingDescriptors.empty()) {
                m_configureThingDescriptorIndex = (m_configureThingDescriptorIndex + 1) % static_cast<int>(m_configureThingDescriptors.size());
                return true;
            }
            return true;
        case ConfigureDialogMode::AddPairingConfirmation: {
            int fieldCount = 0;
            const bool needsUsername = m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword;
            const bool needsSecret = m_configureSetupMethod == api::SetupMethod::SetupMethodDisplayPin || m_configureSetupMethod == api::SetupMethod::SetupMethodEnterPin
                                     || m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword || m_configureSetupMethod == api::SetupMethod::SetupMethodOAuth;
            if (needsUsername) {
                ++fieldCount;
            }
            if (needsSecret) {
                ++fieldCount;
            }
            if (fieldCount > 0 && event == ftxui::Event::ArrowUp) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + fieldCount - 1) % fieldCount;
                return true;
            }
            if (fieldCount > 0 && event == ftxui::Event::ArrowDown) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + 1) % fieldCount;
                return true;
            }
            if (needsUsername && m_configureParamSelectionIndex == 0) {
                return editText(m_configurePairingUsername);
            }
            if (needsSecret && ((!needsUsername && m_configureParamSelectionIndex == 0) || (needsUsername && m_configureParamSelectionIndex == 1))) {
                return editText(m_configurePairingSecret);
            }
            return true;
        }
        case ConfigureDialogMode::RenameThing:
            return editText(m_configureThingName);
        case ConfigureDialogMode::RemoveThingConfirm:
        case ConfigureDialogMode::ReconfigureThingInfo:
        case ConfigureDialogMode::None:
            return true;
        }
    }

    if (event == ftxui::Event::Escape && m_focusArea == FocusArea::ThingSearch && !m_thingSearch.empty()) {
        const QUuid selectedId = selectedThingId();
        m_thingSearch.clear();
        clampThingSelection(selectedId);
        resetThingDetailSelection();
        return true;
    }

    if (m_showLogoutConfirm) {
        if (m_logoutRequestPending) {
            return true;
        }
        if (event == ftxui::Event::Escape) {
            m_showLogoutConfirm = false;
            m_logoutStatus.clear();
            return true;
        }
        if (event == ftxui::Event::Return) {
            revokeCurrentTokenAndLogout();
            return true;
        }
        return true;
    }

    if (m_showSystemActionConfirm) {
        if (m_systemActionRequestPending) {
            return true;
        }
        if (event == ftxui::Event::Escape || event == ftxui::Event::ArrowLeft) {
            closePowerActionConfirmDialog();
            m_systemActionStatus.clear();
            return true;
        }
        if (event == ftxui::Event::Return) {
            executePowerAction();
            return true;
        }
        return true;
    }

    if (m_serverInterfaceDialogMode != ServerInterfaceDialogMode::None) {
        if (m_serverInterfaceRequestPending) {
            return true;
        }
        if (event == ftxui::Event::Escape) {
            closeServerInterfaceDialog();
            return true;
        }
        if (event == ftxui::Event::Return) {
            return submitServerInterfaceDialog();
        }
        if (m_serverInterfaceDialogMode == ServerInterfaceDialogMode::RemoveConfirm) {
            return true;
        }

        auto defaultPort = [](ServerInterfaceType type) {
            switch (type) {
            case ServerInterfaceType::Tcp:
                return std::string("2223");
            case ServerInterfaceType::WebSocket:
                return std::string("4444");
            case ServerInterfaceType::WebServer:
                return std::string("80");
            case ServerInterfaceType::TunnelProxy:
                return std::string("443");
            }
            return std::string("0");
        };
        auto cycleType = [&](int delta) {
            const int nextIndex = cycledIndex(static_cast<int>(m_serverInterfaceDialogType), 4, delta);
            m_serverInterfaceDialogType = static_cast<ServerInterfaceType>(nextIndex);
            if (m_serverInterfaceDialogMode == ServerInterfaceDialogMode::Add) {
                m_serverInterfaceDialogPort = defaultPort(m_serverInterfaceDialogType);
            }
            m_serverInterfaceDialogFieldIndex = std::min(m_serverInterfaceDialogFieldIndex, std::max(0, serverInterfaceDialogFieldCount() - 1));
        };

        const int fieldCount = serverInterfaceDialogFieldCount();
        if (event == ftxui::Event::ArrowUp && fieldCount > 0) {
            m_serverInterfaceDialogFieldIndex = cycledIndex(m_serverInterfaceDialogFieldIndex, fieldCount, -1);
            return true;
        }
        if (event == ftxui::Event::ArrowDown && fieldCount > 0) {
            m_serverInterfaceDialogFieldIndex = cycledIndex(m_serverInterfaceDialogFieldIndex, fieldCount, 1);
            return true;
        }

        const int offset = m_serverInterfaceDialogMode == ServerInterfaceDialogMode::Add ? 1 : 0;
        if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight || event == ftxui::Event::Character(" ")) {
            const int delta = event == ftxui::Event::ArrowLeft ? -1 : 1;
            if (m_serverInterfaceDialogMode == ServerInterfaceDialogMode::Add && m_serverInterfaceDialogFieldIndex == 0) {
                cycleType(delta);
                return true;
            }
            if (m_serverInterfaceDialogFieldIndex == offset + 2) {
                m_serverInterfaceDialogSslEnabled = !m_serverInterfaceDialogSslEnabled;
                return true;
            }
            if (m_serverInterfaceDialogFieldIndex == offset + 3) {
                m_serverInterfaceDialogAuthenticationEnabled = !m_serverInterfaceDialogAuthenticationEnabled;
                return true;
            }
            if (m_serverInterfaceDialogType == ServerInterfaceType::TunnelProxy && m_serverInterfaceDialogFieldIndex == offset + 4) {
                m_serverInterfaceDialogIgnoreSslErrors = !m_serverInterfaceDialogIgnoreSslErrors;
                return true;
            }
        }

        if (m_serverInterfaceDialogFieldIndex == offset) {
            return editDialogTextField(m_serverInterfaceDialogAddress, event, false);
        }
        if (m_serverInterfaceDialogFieldIndex == offset + 1) {
            return editDialogTextField(m_serverInterfaceDialogPort, event, true);
        }
        if (m_serverInterfaceDialogType == ServerInterfaceType::WebServer && m_serverInterfaceDialogFieldIndex == offset + 4) {
            return editDialogTextField(m_serverInterfaceDialogPublicFolder, event, false);
        }
        return true;
    }

    if (m_modbusRtuDialogMode != ModbusRtuDialogMode::None) {
        if (m_modbusRtuRequestPending) {
            return true;
        }
        if (event == ftxui::Event::Escape) {
            closeModbusRtuDialog();
            return true;
        }
        if (event == ftxui::Event::Return) {
            return submitModbusRtuDialog();
        }

        if (m_modbusRtuDialogMode == ModbusRtuDialogMode::RemoveConfirm) {
            return true;
        }

        if (event == ftxui::Event::ArrowUp) {
            m_modbusRtuDialogFieldIndex = cycledIndex(m_modbusRtuDialogFieldIndex, 7, -1);
            return true;
        }
        if (event == ftxui::Event::ArrowDown) {
            m_modbusRtuDialogFieldIndex = cycledIndex(m_modbusRtuDialogFieldIndex, 7, 1);
            return true;
        }
        if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight || event == ftxui::Event::Character(" ")) {
            const int delta = event == ftxui::Event::ArrowLeft ? -1 : 1;
            if (m_modbusRtuDialogFieldIndex == 0 && !m_modbusRtuSerialPorts.empty()) {
                int currentIndex = 0;
                for (int index = 0; index < static_cast<int>(m_modbusRtuSerialPorts.size()); ++index) {
                    if (m_modbusRtuSerialPorts.at(index).systemLocation.toStdString() == m_modbusRtuDialogSerialPort) {
                        currentIndex = index;
                        break;
                    }
                }
                currentIndex = cycledIndex(currentIndex, static_cast<int>(m_modbusRtuSerialPorts.size()), delta);
                m_modbusRtuDialogSerialPort = m_modbusRtuSerialPorts.at(currentIndex).systemLocation.toStdString();
                return true;
            }
            if (m_modbusRtuDialogFieldIndex == 1) {
                m_modbusRtuDialogBaudrateIndex = cycledIndex(m_modbusRtuDialogBaudrateIndex, static_cast<int>(baudrateOptions().size()), delta);
                return true;
            }
            if (m_modbusRtuDialogFieldIndex == 2) {
                m_modbusRtuDialogDataBitsIndex = cycledIndex(m_modbusRtuDialogDataBitsIndex, static_cast<int>(dataBitsOptions().size()), delta);
                return true;
            }
            if (m_modbusRtuDialogFieldIndex == 3) {
                m_modbusRtuDialogParityIndex = cycledIndex(m_modbusRtuDialogParityIndex, static_cast<int>(parityOptions().size()), delta);
                return true;
            }
            if (m_modbusRtuDialogFieldIndex == 4) {
                m_modbusRtuDialogStopBitsIndex = cycledIndex(m_modbusRtuDialogStopBitsIndex, static_cast<int>(stopBitsOptions().size()), delta);
                return true;
            }
            if (event == ftxui::Event::Character(" ")) {
                return true;
            }
        }

        if (m_modbusRtuDialogFieldIndex == 0) {
            editDialogTextField(m_modbusRtuDialogSerialPort, event, false);
            return true;
        }
        if (m_modbusRtuDialogFieldIndex == 5) {
            editDialogTextField(m_modbusRtuDialogTimeout, event, true);
            return true;
        }
        if (m_modbusRtuDialogFieldIndex == 6) {
            editDialogTextField(m_modbusRtuDialogRetries, event, true);
            return true;
        }
        return true;
    }

    if (m_mainView == MainView::Settings && m_systemActionRequestPending) {
        return true;
    }

    if (m_mainView == MainView::Help) {
        if (event == ftxui::Event::Escape) {
            closeHelpView();
            return true;
        }
        if (event == ftxui::Event::ArrowUp) {
            const int lineCount = helpLineCount();
            if (lineCount > 0) {
                m_helpLineIndex = (m_helpLineIndex + lineCount - 1) % lineCount;
            }
            return true;
        }
        if (event == ftxui::Event::ArrowDown) {
            const int lineCount = helpLineCount();
            if (lineCount > 0) {
                m_helpLineIndex = (m_helpLineIndex + 1) % lineCount;
            }
            return true;
        }
        if (event == ftxui::Event::Character("q")) {
            m_client.disconnectFromHost();
            screen.ExitLoopClosure()();
            return true;
        }
        return true;
    }

    if (event == ftxui::Event::Character("q") || event == ftxui::Event::Escape) {
        m_client.disconnectFromHost();
        screen.ExitLoopClosure()();
        return true;
    }

    if (m_showLoginForm) {
        if (event == ftxui::Event::Return) {
            if (m_pushButtonAuthAvailable) {
                requestPushButtonAuth();
            } else {
                authenticate(m_username, m_password);
            }
            return true;
        }
        if (!m_pushButtonAuthAvailable) {
            if (event == ftxui::Event::ArrowUp) {
                m_loginSelectedInputIndex = std::max(0, m_loginSelectedInputIndex - 1);
            } else if (event == ftxui::Event::ArrowDown) {
                m_loginSelectedInputIndex = std::min(1, m_loginSelectedInputIndex + 1);
            }
        }
        if (m_pushButtonAuthAvailable) {
            return true;
        }
        if (m_loginForm->OnEvent(event)) {
            return true;
        }
        return true;
    }

    if (m_mainView == MainView::ApiBrowser) {
        const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
        const std::vector<ApiBrowserItem> filteredItems = filterApiBrowserItems(items, QString::fromStdString(m_apiBrowserSearch));
        const int selectedVisibleIndex = apiBrowserItemIndex(filteredItems, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
        const int currentIndex = apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
        const ApiBrowserItem* currentItem = currentIndex >= 0 ? &items.at(currentIndex) : nullptr;

        auto followSelectedReference = [&]() {
            if (currentItem == nullptr || currentItem->references.empty()) {
                return true;
            }
            if (m_apiBrowserSelectedReferenceIndex < 0 || m_apiBrowserSelectedReferenceIndex >= static_cast<int>(currentItem->references.size())) {
                return true;
            }

            const QString referenceName = currentItem->references.at(m_apiBrowserSelectedReferenceIndex).second;
            const ApiBrowserItem* targetItem = nullptr;
            for (const ApiBrowserItem& item : items) {
                if ((item.kind == ApiBrowserKind::Type || item.kind == ApiBrowserKind::Enum) && item.name == referenceName) {
                    targetItem = &item;
                    break;
                }
            }
            if (targetItem == nullptr) {
                for (const ApiBrowserItem& item : items) {
                    if (item.name == referenceName) {
                        targetItem = &item;
                        break;
                    }
                }
            }

            if (targetItem == nullptr) {
                m_apiBrowserStatus = "Referenced item not found: " + referenceName.toStdString();
                return true;
            }

            selectApiBrowserItem(targetItem->section, targetItem->name, true);
            clampApiBrowserReferenceSelection();
            return true;
        };

        if (event == ftxui::Event::Return) {
            if (m_focusArea == FocusArea::ApiBrowserSearch) {
                m_focusArea = FocusArea::ApiBrowserList;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences) {
                return followSelectedReference();
            }
            return true;
        }

        if (event == ftxui::Event::ArrowRight) {
            if (m_focusArea == FocusArea::ApiBrowserSearch) {
                m_focusArea = FocusArea::ApiBrowserList;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserList) {
                m_focusArea = FocusArea::ApiBrowserReferences;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences) {
                m_focusArea = FocusArea::ApiBrowserSearch;
                return true;
            }
        }

        if (event == ftxui::Event::ArrowLeft) {
            if (!m_apiBrowserHistory.empty()) {
                apiBrowserGoBack();
                clampApiBrowserReferenceSelection();
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences) {
                m_focusArea = FocusArea::ApiBrowserList;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserList) {
                m_focusArea = FocusArea::ApiBrowserSearch;
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserSearch) {
                syncMainMenuSelectionToCurrentView();
                m_focusArea = FocusArea::MainMenu;
                return true;
            }
        }

        if (event == ftxui::Event::ArrowUp) {
            if (m_focusArea == FocusArea::ApiBrowserList && !filteredItems.empty()) {
                int index = selectedVisibleIndex;
                if (index < 0) {
                    index = static_cast<int>(filteredItems.size()) - 1;
                } else {
                    index = (index + static_cast<int>(filteredItems.size()) - 1) % static_cast<int>(filteredItems.size());
                }
                selectApiBrowserItem(filteredItems.at(index).section, filteredItems.at(index).name, false);
                clampApiBrowserReferenceSelection();
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences && currentItem != nullptr && !currentItem->references.empty()) {
                m_apiBrowserSelectedReferenceIndex = (m_apiBrowserSelectedReferenceIndex + static_cast<int>(currentItem->references.size()) - 1)
                                                     % static_cast<int>(currentItem->references.size());
                return true;
            }
        }

        if (event == ftxui::Event::ArrowDown) {
            if (m_focusArea == FocusArea::ApiBrowserList && !filteredItems.empty()) {
                int index = selectedVisibleIndex;
                if (index < 0) {
                    index = 0;
                } else {
                    index = (index + 1) % static_cast<int>(filteredItems.size());
                }
                selectApiBrowserItem(filteredItems.at(index).section, filteredItems.at(index).name, false);
                clampApiBrowserReferenceSelection();
                return true;
            }
            if (m_focusArea == FocusArea::ApiBrowserReferences && currentItem != nullptr && !currentItem->references.empty()) {
                m_apiBrowserSelectedReferenceIndex = (m_apiBrowserSelectedReferenceIndex + 1) % static_cast<int>(currentItem->references.size());
                return true;
            }
        }

        if (m_focusArea == FocusArea::ApiBrowserSearch) {
            if (event == ftxui::Event::Backspace && !m_apiBrowserSearch.empty()) {
                m_apiBrowserSearch.pop_back();
                return true;
            }
            if (event.is_character()) {
                m_apiBrowserSearch += event.character();
                return true;
            }
        }
    }

    if (m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsDetails && m_settingsView == SettingsView::LoggingCategories) {
        auto selectedLoggingCategory = [&]() -> std::optional<api::LoggingCategory> {
            if (!m_loggingCategoriesLoaded) {
                return std::nullopt;
            }
            const std::vector<api::LoggingCategory> filteredCategories = filteredLoggingCategories();
            const int categoryIndex = m_settingsDetailsLineIndex - loggingCategoryListStartLineIndex;
            if (categoryIndex < 0 || categoryIndex >= static_cast<int>(filteredCategories.size())) {
                return std::nullopt;
            }
            return filteredCategories.at(categoryIndex);
        };

        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown) {
            const int filteredCount = m_loggingCategoriesLoaded ? static_cast<int>(filteredLoggingCategories().size()) : 0;
            m_settingsDetailsLineIndex = nextFilterListDetailsLineIndex(m_settingsDetailsLineIndex,
                                                                        event == ftxui::Event::ArrowDown ? 1 : -1,
                                                                        loggingCategorySearchLineIndex,
                                                                        loggingCategoryListStartLineIndex,
                                                                        filteredCount);
            return true;
        }

        if (m_settingsDetailsLineIndex == loggingCategorySearchLineIndex) {
            if (event == ftxui::Event::Backspace && !m_loggingCategorySearch.empty()) {
                m_loggingCategorySearch.pop_back();
                clampSettingsDetailsSelection();
                return true;
            }
            if (event.is_character()) {
                m_loggingCategorySearch += event.character();
                clampSettingsDetailsSelection();
                return true;
            }
        }

        if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight || event == ftxui::Event::Character(" ")) {
            if (m_systemActionRequestPending) {
                return true;
            }
            const std::optional<api::LoggingCategory> category = selectedLoggingCategory();
            if (!category.has_value()) {
                if (event == ftxui::Event::Character(" ")) {
                    return true;
                }
            } else {
                const int delta = event == ftxui::Event::ArrowLeft ? -1 : 1;
                const api::LoggingLevel nextLevel = cycleLoggingLevel(category->level, delta);
                api::DebugSetLoggingCategoryLevelParams request;
                request.name = category->name;
                request.level = nextLevel;
                m_systemActionRequestPending = true;
                m_loggingCategoryStatus = "Setting logging category " + category->name.toStdString() + " to " + loggingLevelLabel(nextLevel) + "...";
                observeReply(m_client.sendRequest(api::DebugSetLoggingCategoryLevelMethod::methodName(), request.toJson()),
                             [this, categoryName = category->name, nextLevel](const QJsonObject& message, const QString& transportError) {
                                 handleSetLoggingCategoryLevelReply(message, transportError, categoryName, nextLevel);
                             });
                return true;
            }
        }
    }

    if (m_mainView == MainView::Settings && (m_focusArea == FocusArea::SettingsDetails || m_focusArea == FocusArea::SettingsMenu || m_focusArea == FocusArea::MainMenu)
        && m_settingsView == SettingsView::ServerInterfaces) {
        if (m_focusArea != FocusArea::SettingsDetails && (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown)) {
            // Let the settings/main menu keep normal navigation until the details pane is focused.
        } else if (event == ftxui::Event::Character("a")) {
            openAddServerInterfaceDialog();
            return true;
        } else if (event == ftxui::Event::Character("e")) {
            openEditServerInterfaceDialog();
            return true;
        } else if (event == ftxui::Event::Character("d")) {
            openRemoveServerInterfaceDialog();
            return true;
        } else if (event == ftxui::Event::Character("r")) {
            m_serverInterfacesLoaded = false;
            m_serverInterfacesPending = false;
            m_serverInterfaceStatus = "Refreshing server interfaces...";
            ensureServerInterfacesLoaded();
            return true;
        }
    }

    if (m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsDetails && m_settingsView == SettingsView::ServerInterfaces) {
        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown) {
            const int count = std::max(1, serverInterfaceCount());
            m_settingsDetailsLineIndex = cycledIndex(std::max(0, m_settingsDetailsLineIndex), count, event == ftxui::Event::ArrowDown ? 1 : -1);
            return true;
        }
    }

    if (m_mainView == MainView::Settings && (m_focusArea == FocusArea::SettingsDetails || m_focusArea == FocusArea::SettingsMenu || m_focusArea == FocusArea::MainMenu)
        && m_settingsView == SettingsView::ModbusRtu) {
        if (m_focusArea != FocusArea::SettingsDetails && (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown)) {
            // Let the settings/main menu keep normal navigation until the details pane is focused.
        } else if (event == ftxui::Event::Character("a")) {
            openAddModbusRtuDialog();
            return true;
        } else if (event == ftxui::Event::Character("e")) {
            openEditModbusRtuDialog();
            return true;
        } else if (event == ftxui::Event::Character("d")) {
            openRemoveModbusRtuDialog();
            return true;
        } else if (event == ftxui::Event::Character("r")) {
            m_modbusRtuMastersLoaded = false;
            m_modbusRtuMastersPending = false;
            m_modbusRtuSerialPortsLoaded = false;
            m_modbusRtuSerialPortsPending = false;
            m_modbusRtuStatus = "Refreshing Modbus RTU resources...";
            ensureModbusRtuLoaded();
            return true;
        }
    }

    if (m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsDetails && m_settingsView == SettingsView::ModbusRtu) {
        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown) {
            const int count = std::max(1, static_cast<int>(m_modbusRtuMasters.size()));
            const int currentIndex = std::max(0, m_settingsDetailsLineIndex - modbusRtuMasterListStartLineIndex);
            const int nextIndex = cycledIndex(currentIndex, count, event == ftxui::Event::ArrowDown ? 1 : -1);
            m_settingsDetailsLineIndex = modbusRtuMasterListStartLineIndex + nextIndex;
            return true;
        }
    }

    if (event == ftxui::Event::ArrowLeft) {
        if (m_focusArea == FocusArea::ThingDetails) {
            m_focusArea = FocusArea::ThingList;
            m_showThingDetailInspector = false;
            return true;
        }
        if (m_focusArea == FocusArea::ThingList) {
            m_focusArea = FocusArea::ThingSearch;
            return true;
        }
        if (m_focusArea == FocusArea::ThingSearch) {
            syncMainMenuSelectionToCurrentView();
            m_focusArea = FocusArea::MainMenu;
            return true;
        }
        if (m_focusArea == FocusArea::ConfigureThingClassList) {
            m_focusArea = FocusArea::ConfigureThingClassSearch;
            return true;
        }
        if (m_focusArea == FocusArea::ConfigureThingSelection) {
            m_focusArea = FocusArea::ConfigureThingSelectionSearch;
            return true;
        }
        if (m_focusArea == FocusArea::ConfigureThingClassSearch || m_focusArea == FocusArea::ConfigureThingSelectionSearch || m_focusArea == FocusArea::ConfigureMenu) {
            syncMainMenuSelectionToCurrentView();
            m_focusArea = FocusArea::MainMenu;
            return true;
        }
        if (m_focusArea == FocusArea::SettingsMenu) {
            syncMainMenuSelectionToCurrentView();
            m_focusArea = FocusArea::MainMenu;
            return true;
        }
        if (m_focusArea == FocusArea::SettingsDetails) {
            m_focusArea = FocusArea::SettingsMenu;
            return true;
        }
        m_focusArea = FocusArea::MainMenu;
        syncMainMenuSelectionToCurrentView();
        return true;
    }

    if (event == ftxui::Event::ArrowRight) {
        if (m_mainView == MainView::Things) {
            if (m_focusArea == FocusArea::MainMenu) {
                m_focusArea = FocusArea::ThingSearch;
            } else if (m_focusArea == FocusArea::ThingSearch) {
                m_focusArea = FocusArea::ThingList;
            } else if (m_focusArea == FocusArea::ThingList && thingDetailEntryCount() > 0) {
                selectInitialThingDetailSection();
                m_focusArea = FocusArea::ThingDetails;
            } else {
                m_focusArea = FocusArea::ThingList;
            }
        } else if (m_mainView == MainView::ConfigureThings) {
            if (!m_haveAllThingClasses && !m_fetchAllThingClassesPending) {
                fetchAllThingClasses();
            }
            if (m_focusArea == FocusArea::ConfigureMenu) {
                m_focusArea = m_configureThingsView == ConfigureThingsView::AddThing ? FocusArea::ConfigureThingClassSearch : FocusArea::ConfigureThingSelectionSearch;
            } else if (m_focusArea == FocusArea::ConfigureThingClassSearch) {
                m_focusArea = FocusArea::ConfigureThingClassList;
            } else if (m_focusArea == FocusArea::ConfigureThingSelectionSearch) {
                m_focusArea = FocusArea::ConfigureThingSelection;
            } else {
                m_focusArea = FocusArea::ConfigureMenu;
            }
        } else if (m_mainView == MainView::ApiBrowser) {
            if (m_focusArea == FocusArea::MainMenu) {
                m_focusArea = FocusArea::ApiBrowserSearch;
            } else if (m_focusArea == FocusArea::ApiBrowserSearch) {
                m_focusArea = FocusArea::ApiBrowserList;
            } else if (m_focusArea == FocusArea::ApiBrowserList) {
                m_focusArea = FocusArea::ApiBrowserReferences;
            } else {
                m_focusArea = FocusArea::ApiBrowserSearch;
            }
        } else if (m_mainView == MainView::Settings) {
            if (m_focusArea == FocusArea::SettingsMenu) {
                m_focusArea = FocusArea::SettingsDetails;
                clampSettingsDetailsSelection();
            } else if (m_focusArea != FocusArea::SettingsDetails) {
                m_focusArea = FocusArea::SettingsMenu;
            }
        }
        return true;
    }

    if (m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsDetails && m_settingsView == SettingsView::Timezone
        && (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown)) {
        const int filteredCount = m_systemTimeZonesLoaded ? static_cast<int>(filteredSystemTimeZones().size()) : 0;
        m_settingsDetailsLineIndex = nextFilterListDetailsLineIndex(m_settingsDetailsLineIndex,
                                                                    event == ftxui::Event::ArrowDown ? 1 : -1,
                                                                    timezoneSearchLineIndex,
                                                                    timezoneListStartLineIndex,
                                                                    filteredCount);
        return true;
    }

    if (event == ftxui::Event::ArrowUp) {
        if (m_focusArea == FocusArea::ThingDetails && thingDetailEntryCount() > 0) {
            m_selectedThingDetailIndex = (m_selectedThingDetailIndex + thingDetailEntryCount() - 1) % thingDetailEntryCount();
            return true;
        }

        if (m_focusArea == FocusArea::ThingList && !filteredThings().empty()) {
            const int count = static_cast<int>(filteredThings().size());
            m_selectedThingIndex = (m_selectedThingIndex + count - 1) % count;
            resetThingDetailSelection();
            return true;
        }

        if (m_mainView == MainView::Settings) {
            if (m_focusArea == FocusArea::SettingsMenu) {
                if (m_settingsView == SettingsView::ServerInfo) {
                    m_settingsView = SettingsView::Reboot;
                } else {
                    m_settingsView = static_cast<SettingsView>(static_cast<int>(m_settingsView) - 1);
                }
                m_settingsDetailsLineIndex = 0;
                ensureSystemCapabilitiesLoaded();
                ensureSystemTimeLoaded();
                ensureSystemUpdateStatusLoaded();
                if (m_settingsView == SettingsView::Timezone) {
                    ensureSystemTimeZonesLoaded();
                } else if (m_settingsView == SettingsView::Update) {
                    ensureSystemPackagesLoaded();
                } else if (m_settingsView == SettingsView::LoggingCategories) {
                    ensureLoggingCategoriesLoaded();
                } else if (m_settingsView == SettingsView::ServerInterfaces) {
                    ensureServerInterfacesLoaded();
                } else if (m_settingsView == SettingsView::ModbusRtu) {
                    ensureModbusRtuLoaded();
                }
                return true;
            }
            if (m_focusArea == FocusArea::SettingsDetails) {
                const int lineCount = settingsDetailsLineCount();
                if (lineCount > 0) {
                    m_settingsDetailsLineIndex = (m_settingsDetailsLineIndex + lineCount - 1) % lineCount;
                }
                return true;
            }
        }

        if (m_focusArea == FocusArea::ConfigureMenu && m_mainView == MainView::ConfigureThings) {
            if (m_configureThingsView == ConfigureThingsView::AddThing) {
                m_configureThingsView = ConfigureThingsView::RenameThing;
            } else {
                m_configureThingsView = static_cast<ConfigureThingsView>(static_cast<int>(m_configureThingsView) - 1);
            }
            if (m_configureThingsView == ConfigureThingsView::AddThing && !m_haveAllThingClasses && !m_fetchAllThingClassesPending) {
                fetchAllThingClasses();
            }
            return true;
        }

        if (m_focusArea == FocusArea::ConfigureThingClassList && !filteredConfigThingClasses().empty()) {
            const int count = static_cast<int>(filteredConfigThingClasses().size());
            m_selectedConfigureThingClassIndex = (m_selectedConfigureThingClassIndex + count - 1) % count;
            return true;
        }

        if (m_focusArea == FocusArea::ConfigureThingSelection && !filteredConfigureThings().empty()) {
            const int count = static_cast<int>(filteredConfigureThings().size());
            m_selectedConfigureThingIndex = (m_selectedConfigureThingIndex + count - 1) % count;
            return true;
        }

        if (m_focusArea == FocusArea::ApiBrowserReferences) {
            const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
            const int currentIndex = apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
            if (currentIndex >= 0) {
                const ApiBrowserItem& currentItem = items.at(currentIndex);
                if (!currentItem.references.empty()) {
                    m_apiBrowserSelectedReferenceIndex = (m_apiBrowserSelectedReferenceIndex + static_cast<int>(currentItem.references.size()) - 1)
                                                         % static_cast<int>(currentItem.references.size());
                }
            }
            return true;
        }

        if (m_focusArea == FocusArea::MainMenu) {
            switch (m_selectedMainMenuEntry) {
            case MainMenuEntry::Things:
                applyMainMenuSelection(MainMenuEntry::About);
                break;
            case MainMenuEntry::ConfigureThings:
                applyMainMenuSelection(MainMenuEntry::Things);
                break;
            case MainMenuEntry::ApiBrowser:
                applyMainMenuSelection(MainMenuEntry::ConfigureThings);
                break;
            case MainMenuEntry::Settings:
                applyMainMenuSelection(MainMenuEntry::ApiBrowser);
                break;
            case MainMenuEntry::Logout:
                applyMainMenuSelection(MainMenuEntry::Settings);
                break;
            case MainMenuEntry::About:
                applyMainMenuSelection(MainMenuEntry::Logout);
                break;
            }
            return true;
        }
    }

    if (event == ftxui::Event::ArrowDown) {
        if (m_focusArea == FocusArea::ThingDetails && thingDetailEntryCount() > 0) {
            m_selectedThingDetailIndex = (m_selectedThingDetailIndex + 1) % thingDetailEntryCount();
            return true;
        }

        if (m_focusArea == FocusArea::ThingList && !filteredThings().empty()) {
            const int count = static_cast<int>(filteredThings().size());
            m_selectedThingIndex = (m_selectedThingIndex + 1) % count;
            resetThingDetailSelection();
            return true;
        }

        if (m_mainView == MainView::Settings) {
            if (m_focusArea == FocusArea::SettingsMenu) {
                if (m_settingsView == SettingsView::Reboot) {
                    m_settingsView = SettingsView::ServerInfo;
                } else {
                    m_settingsView = static_cast<SettingsView>(static_cast<int>(m_settingsView) + 1);
                }
                m_settingsDetailsLineIndex = 0;
                ensureSystemCapabilitiesLoaded();
                ensureSystemTimeLoaded();
                ensureSystemUpdateStatusLoaded();
                if (m_settingsView == SettingsView::Timezone) {
                    ensureSystemTimeZonesLoaded();
                } else if (m_settingsView == SettingsView::Update) {
                    ensureSystemPackagesLoaded();
                } else if (m_settingsView == SettingsView::LoggingCategories) {
                    ensureLoggingCategoriesLoaded();
                } else if (m_settingsView == SettingsView::ServerInterfaces) {
                    ensureServerInterfacesLoaded();
                } else if (m_settingsView == SettingsView::ModbusRtu) {
                    ensureModbusRtuLoaded();
                }
                return true;
            }
            if (m_focusArea == FocusArea::SettingsDetails) {
                const int lineCount = settingsDetailsLineCount();
                if (lineCount > 0) {
                    m_settingsDetailsLineIndex = (m_settingsDetailsLineIndex + 1) % lineCount;
                }
                return true;
            }
        }

        if (m_focusArea == FocusArea::ConfigureMenu && m_mainView == MainView::ConfigureThings) {
            if (m_configureThingsView == ConfigureThingsView::RenameThing) {
                m_configureThingsView = ConfigureThingsView::AddThing;
            } else {
                m_configureThingsView = static_cast<ConfigureThingsView>(static_cast<int>(m_configureThingsView) + 1);
            }
            if (m_configureThingsView == ConfigureThingsView::AddThing && !m_haveAllThingClasses && !m_fetchAllThingClassesPending) {
                fetchAllThingClasses();
            }
            return true;
        }

        if (m_focusArea == FocusArea::ConfigureThingClassList && !filteredConfigThingClasses().empty()) {
            const int count = static_cast<int>(filteredConfigThingClasses().size());
            m_selectedConfigureThingClassIndex = (m_selectedConfigureThingClassIndex + 1) % count;
            return true;
        }

        if (m_focusArea == FocusArea::ConfigureThingSelection && !filteredConfigureThings().empty()) {
            const int count = static_cast<int>(filteredConfigureThings().size());
            m_selectedConfigureThingIndex = (m_selectedConfigureThingIndex + 1) % count;
            return true;
        }

        if (m_focusArea == FocusArea::ApiBrowserList) {
            const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
            const std::vector<ApiBrowserItem> filteredItems = filterApiBrowserItems(items, QString::fromStdString(m_apiBrowserSearch));
            const int visibleIndex = apiBrowserItemIndex(filteredItems, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
            if (!filteredItems.empty()) {
                const int nextIndex = visibleIndex < 0 ? 0 : (visibleIndex + 1) % static_cast<int>(filteredItems.size());
                selectApiBrowserItem(filteredItems.at(nextIndex).section, filteredItems.at(nextIndex).name, false);
                clampApiBrowserReferenceSelection();
            }
            return true;
        }

        if (m_focusArea == FocusArea::MainMenu) {
            switch (m_selectedMainMenuEntry) {
            case MainMenuEntry::Things:
                applyMainMenuSelection(MainMenuEntry::ConfigureThings);
                break;
            case MainMenuEntry::ConfigureThings:
                applyMainMenuSelection(MainMenuEntry::ApiBrowser);
                break;
            case MainMenuEntry::ApiBrowser:
                applyMainMenuSelection(MainMenuEntry::Settings);
                break;
            case MainMenuEntry::Settings:
                applyMainMenuSelection(MainMenuEntry::Logout);
                break;
            case MainMenuEntry::Logout:
                applyMainMenuSelection(MainMenuEntry::About);
                break;
            case MainMenuEntry::About:
                applyMainMenuSelection(MainMenuEntry::Things);
                break;
            }
            return true;
        }
    }

    if (m_focusArea == FocusArea::MainMenu && event == ftxui::Event::Return) {
        if (m_selectedMainMenuEntry == MainMenuEntry::Logout) {
            if (m_client.isConnected() && m_isAuthenticated && !m_client.authToken().isEmpty()) {
                logout();
            }
            return true;
        }
        return true;
    }

    if (event == ftxui::Event::Return && m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsMenu) {
        m_focusArea = FocusArea::SettingsDetails;
        clampSettingsDetailsSelection();
        return true;
    }

    if (event == ftxui::Event::Return && m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsDetails) {
        switch (m_settingsView) {
        case SettingsView::ServerInfo:
            return true;
        case SettingsView::Timezone: {
            if (!m_systemTimeZonesLoaded || m_systemActionRequestPending) {
                return true;
            }
            const QStringList filteredTimeZones = filteredSystemTimeZones();
            const int zoneCount = static_cast<int>(filteredTimeZones.size());
            const int zoneIndex = m_settingsDetailsLineIndex - timezoneListStartLineIndex;
            if (zoneIndex < 0 || zoneIndex >= zoneCount) {
                return true;
            }
            const QString timeZone = filteredTimeZones.at(zoneIndex);
            api::ConfigurationSetTimeZoneParams request;
            request.timeZone = timeZone;
            m_systemActionRequestPending = true;
            m_systemActionStatus = "Setting time zone to " + timeZone.toStdString() + "...";
            observeReply(m_client.sendRequest(api::ConfigurationSetTimeZoneMethod::methodName(), request.toJson()),
                         [this](const QJsonObject& message, const QString& transportError) { handleSetTimeZoneReply(message, transportError); });
            return true;
        }
        case SettingsView::Update: {
            if (systemUpdateInteractionBusy() || m_systemActionRequestPending || m_settingsDetailsLineIndex < 0 || m_settingsDetailsLineIndex >= updateActionCount()) {
                return true;
            }
            if (m_settingsDetailsLineIndex == 0) {
                m_systemActionRequestPending = true;
                m_systemActionStatus = "Checking for updates...";
                observeReply(m_client.sendRequest(api::SystemCheckForUpdatesMethod::methodName(), QJsonObject{}),
                             [this](const QJsonObject& message, const QString& transportError) { handleCheckForUpdatesReply(message, transportError); });
                return true;
            }
            if (m_settingsDetailsLineIndex == 1) {
                if (!m_systemPackagesLoaded) {
                    return true;
                }
                const std::vector<const api::Package*> updatePackages = updateAvailablePackages();
                if (updatePackages.empty()) {
                    m_systemActionStatus = "No packages have updates available.";
                    return true;
                }
                QList<QString> packageIds;
                for (const api::Package* package : updatePackages) {
                    packageIds.append(package->id);
                }
                if (packageIds.isEmpty()) {
                    m_systemActionStatus = "No packages have updates available.";
                    return true;
                }

                api::SystemUpdatePackagesParams request;
                request.packageIds = packageIds;
                m_systemActionRequestPending = true;
                m_systemActionStatus = "Starting package update...";
                observeReply(m_client.sendRequest(api::SystemUpdatePackagesMethod::methodName(), request.toJson()),
                             [this](const QJsonObject& message, const QString& transportError) { handleUpdatePackagesReply(message, transportError); });
                return true;
            }
            return true;
        }
        case SettingsView::LoggingCategories:
            return true;
        case SettingsView::ServerInterfaces:
            openEditServerInterfaceDialog();
            return true;
        case SettingsView::ModbusRtu:
            openEditModbusRtuDialog();
            return true;
        case SettingsView::Shutdown:
            openPowerActionConfirmDialog(PowerAction::Shutdown);
            return true;
        case SettingsView::Restart:
            openPowerActionConfirmDialog(PowerAction::Restart);
            return true;
        case SettingsView::Reboot:
            openPowerActionConfirmDialog(PowerAction::Reboot);
            return true;
        }
    }

    if (event == ftxui::Event::Character(" ")) {
        if (m_focusArea == FocusArea::ThingDetails && thingDetailEntryCount() > 0) {
            m_showThingDetailInspector = !m_showThingDetailInspector;
            return true;
        }
    }

    if (event == ftxui::Event::Character("l") && m_focusArea == FocusArea::ThingDetails) {
        return openSelectedLogView();
    }

    if (event == ftxui::Event::Return) {
        if (m_focusArea == FocusArea::ThingDetails) {
            return openSelectedActionDialog();
        }
        if (m_focusArea == FocusArea::ConfigureThingClassList && m_mainView == MainView::ConfigureThings && m_configureThingsView == ConfigureThingsView::AddThing) {
            openAddThingDialog();
            return true;
        }
        if (m_focusArea == FocusArea::ConfigureThingSelection && m_mainView == MainView::ConfigureThings) {
            switch (m_configureThingsView) {
            case ConfigureThingsView::RemoveThing:
                openRemoveThingDialog();
                break;
            case ConfigureThingsView::ReconfigureThing:
                openReconfigureThingDialog();
                break;
            case ConfigureThingsView::RenameThing:
                openRenameThingDialog();
                break;
            case ConfigureThingsView::AddThing:
                break;
            }
            return true;
        }
    }

    if (m_mainView == MainView::Settings && m_focusArea == FocusArea::SettingsDetails && m_settingsView == SettingsView::Timezone) {
        if (m_settingsDetailsLineIndex == timezoneSearchLineIndex) {
            if (event == ftxui::Event::Backspace && !m_systemTimeZoneSearch.empty()) {
                m_systemTimeZoneSearch.pop_back();
                clampSettingsDetailsSelection();
                return true;
            }
            if (event.is_character()) {
                m_systemTimeZoneSearch += event.character();
                clampSettingsDetailsSelection();
                return true;
            }
        }
    }

    if (m_focusArea == FocusArea::ThingSearch && m_mainView == MainView::Things) {
        if (event == ftxui::Event::Backspace && !m_thingSearch.empty()) {
            const QUuid selectedId = selectedThingId();
            m_thingSearch.pop_back();
            clampThingSelection(selectedId);
            resetThingDetailSelection();
            return true;
        }
        if (event.is_character()) {
            const QUuid selectedId = selectedThingId();
            m_thingSearch += event.character();
            clampThingSelection(selectedId);
            resetThingDetailSelection();
            return true;
        }
    }

    if (m_focusArea == FocusArea::ConfigureThingClassSearch && m_mainView == MainView::ConfigureThings && m_configureThingsView == ConfigureThingsView::AddThing) {
        if (event == ftxui::Event::Backspace && !m_configureThingSearch.empty()) {
            m_configureThingSearch.pop_back();
            clampConfigureThingClassSelection();
            return true;
        }
        if (event.is_character()) {
            m_configureThingSearch += event.character();
            clampConfigureThingClassSelection();
            return true;
        }
    }

    if (m_focusArea == FocusArea::ConfigureThingSelectionSearch && m_mainView == MainView::ConfigureThings) {
        if (event == ftxui::Event::Backspace && !m_configureThingSelectionSearch.empty()) {
            m_configureThingSelectionSearch.pop_back();
            m_selectedConfigureThingIndex = 0;
            clampConfigureThingSelection();
            return true;
        }
        if (event.is_character()) {
            m_configureThingSelectionSearch += event.character();
            m_selectedConfigureThingIndex = 0;
            clampConfigureThingSelection();
            return true;
        }
    }

    if (event == ftxui::Event::Character("s") && m_mainView == MainView::Things) {
        cycleThingSortMode();
        return true;
    }

    if (event == ftxui::Event::Character("f") && m_mainView == MainView::Things) {
        cycleThingCategoryFilter();
        return true;
    }

    if (event == ftxui::Event::Character("c")) {
        connectToServer();
        if (m_client.isConnected()) {
            runHandshakeAndLoadThings();
        }
        return true;
    }

    if (event == ftxui::Event::Character("h") || event == ftxui::Event::Character("?")) {
        if (m_focusArea != FocusArea::ThingSearch && m_focusArea != FocusArea::ConfigureThingClassSearch && m_focusArea != FocusArea::ConfigureThingSelectionSearch
            && m_focusArea != FocusArea::ApiBrowserSearch) {
            openHelpView();
            return true;
        }
    }

    if (event == ftxui::Event::Character("t")) {
        fetchThings();
        return true;
    }

    return false;
}

} // namespace nymea
