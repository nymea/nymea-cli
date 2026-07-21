// SPDX-License-Identifier: GPL-3.0-or-later

#include "engineinternal.h"

namespace nymea {

void Engine::ensureModbusRtuLoaded()
{
    if (!m_client.isConnected() || (m_isAuthenticationRequired && !m_isAuthenticated)) {
        return;
    }

    if (!m_modbusRtuMastersLoaded && !m_modbusRtuMastersPending) {
        m_modbusRtuMastersPending = true;
        observeReply(m_client.sendRequest(api::ModbusRtuGetModbusRtuMastersMethod::methodName(), QJsonObject{}),
                     [this](const QJsonObject& message, const QString& transportError) { handleFetchModbusRtuMastersReply(message, transportError); });
    }

    if (!m_modbusRtuSerialPortsLoaded && !m_modbusRtuSerialPortsPending) {
        m_modbusRtuSerialPortsPending = true;
        observeReply(m_client.sendRequest(api::ModbusRtuGetSerialPortsMethod::methodName(), QJsonObject{}),
                     [this](const QJsonObject& message, const QString& transportError) { handleFetchModbusRtuSerialPortsReply(message, transportError); });
    }
}

const api::ModbusRtuMaster* Engine::selectedModbusRtuMaster() const
{
    const int index = m_settingsDetailsLineIndex - modbusRtuMasterListStartLineIndex;
    if (index < 0 || index >= static_cast<int>(m_modbusRtuMasters.size())) {
        return nullptr;
    }
    return &m_modbusRtuMasters.at(index);
}

void Engine::clampModbusRtuSelection()
{
    if (m_modbusRtuMasters.empty()) {
        m_settingsDetailsLineIndex = modbusRtuMasterListStartLineIndex;
        return;
    }

    const int firstLineIndex = modbusRtuMasterListStartLineIndex;
    const int lastLineIndex = firstLineIndex + static_cast<int>(m_modbusRtuMasters.size()) - 1;
    if (m_settingsDetailsLineIndex < firstLineIndex) {
        m_settingsDetailsLineIndex = firstLineIndex;
    } else if (m_settingsDetailsLineIndex > lastLineIndex) {
        m_settingsDetailsLineIndex = lastLineIndex;
    }
}

void Engine::openAddModbusRtuDialog()
{
    m_previousFocusArea = m_focusArea;
    m_focusArea = FocusArea::ModbusRtuDialog;
    m_modbusRtuDialogMode = ModbusRtuDialogMode::Add;
    m_modbusRtuDialogFieldIndex = 0;
    m_modbusRtuRequestPending = false;
    m_modbusRtuDialogUuid = QUuid();
    m_modbusRtuDialogSerialPort = m_modbusRtuSerialPorts.empty() ? std::string() : m_modbusRtuSerialPorts.front().systemLocation.toStdString();
    m_modbusRtuDialogTimeout = "1000";
    m_modbusRtuDialogRetries = "3";
    m_modbusRtuDialogBaudrateIndex = optionIndex(baudrateOptions(), static_cast<quint64>(9600), 3);
    m_modbusRtuDialogDataBitsIndex = optionIndex(dataBitsOptions(), api::SerialPortDataBits::SerialPortDataBitsData8, 3);
    m_modbusRtuDialogParityIndex = optionIndex(parityOptions(), api::SerialPortParity::SerialPortParityNoParity, 0);
    m_modbusRtuDialogStopBitsIndex = optionIndex(stopBitsOptions(), api::SerialPortStopBits::SerialPortStopBitsOneStop, 0);
    m_modbusRtuStatus = "Add Modbus RTU master.";
}

void Engine::openEditModbusRtuDialog()
{
    const api::ModbusRtuMaster* master = selectedModbusRtuMaster();
    if (master == nullptr) {
        m_modbusRtuStatus = "No Modbus RTU master selected.";
        return;
    }

    m_previousFocusArea = m_focusArea;
    m_focusArea = FocusArea::ModbusRtuDialog;
    m_modbusRtuDialogMode = ModbusRtuDialogMode::Edit;
    m_modbusRtuDialogFieldIndex = 0;
    m_modbusRtuRequestPending = false;
    m_modbusRtuDialogUuid = master->modbusUuid;
    m_modbusRtuDialogSerialPort = master->serialPort.toStdString();
    m_modbusRtuDialogTimeout = QString::number(master->timeout).toStdString();
    m_modbusRtuDialogRetries = QString::number(master->numberOfRetries).toStdString();
    m_modbusRtuDialogBaudrateIndex = optionIndex(baudrateOptions(), master->baudrate, 3);
    m_modbusRtuDialogDataBitsIndex = optionIndex(dataBitsOptions(), master->dataBits, 3);
    m_modbusRtuDialogParityIndex = optionIndex(parityOptions(), master->parity, 0);
    m_modbusRtuDialogStopBitsIndex = optionIndex(stopBitsOptions(), master->stopBits, 0);
    m_modbusRtuStatus = "Edit Modbus RTU master " + master->modbusUuid.toString(QUuid::WithoutBraces).toStdString() + ".";
}

void Engine::openRemoveModbusRtuDialog()
{
    const api::ModbusRtuMaster* master = selectedModbusRtuMaster();
    if (master == nullptr) {
        m_modbusRtuStatus = "No Modbus RTU master selected.";
        return;
    }

    m_previousFocusArea = m_focusArea;
    m_focusArea = FocusArea::ModbusRtuDialog;
    m_modbusRtuDialogMode = ModbusRtuDialogMode::RemoveConfirm;
    m_modbusRtuRequestPending = false;
    m_modbusRtuDialogUuid = master->modbusUuid;
    m_modbusRtuDialogSerialPort = master->serialPort.toStdString();
    m_modbusRtuStatus = "Confirm removing Modbus RTU master.";
}

void Engine::closeModbusRtuDialog()
{
    m_modbusRtuDialogMode = ModbusRtuDialogMode::None;
    m_modbusRtuRequestPending = false;
    m_modbusRtuDialogFieldIndex = 0;
    m_focusArea = m_previousFocusArea == FocusArea::ModbusRtuDialog ? FocusArea::SettingsDetails : m_previousFocusArea;
}

bool Engine::submitModbusRtuDialog()
{
    if (m_modbusRtuRequestPending || m_modbusRtuDialogMode == ModbusRtuDialogMode::None) {
        return true;
    }

    if (m_modbusRtuDialogMode == ModbusRtuDialogMode::RemoveConfirm) {
        api::ModbusRtuRemoveModbusRtuMasterParams request;
        request.modbusUuid = m_modbusRtuDialogUuid;
        m_modbusRtuRequestPending = true;
        m_modbusRtuStatus = "Removing Modbus RTU master...";
        observeReply(m_client.sendRequest(api::ModbusRtuRemoveModbusRtuMasterMethod::methodName(), request.toJson()),
                     [this](const QJsonObject& message, const QString& transportError) { handleRemoveModbusRtuReply(message, transportError); });
        return true;
    }

    auto parseUint = [](const std::string& raw, const char* label, quint64 minimum, quint64& value, std::string& errorMessage) {
        bool ok = false;
        const quint64 parsed = QString::fromStdString(raw).trimmed().toULongLong(&ok);
        if (!ok || parsed < minimum) {
            errorMessage = std::string(label) + " must be an integer >= " + std::to_string(minimum) + ".";
            return false;
        }
        value = parsed;
        return true;
    };

    const QString serialPort = QString::fromStdString(m_modbusRtuDialogSerialPort).trimmed();
    if (serialPort.isEmpty()) {
        m_modbusRtuStatus = "Serial port must not be empty.";
        return true;
    }

    quint64 timeout = 0;
    quint64 retries = 0;
    std::string validationError;
    if (!parseUint(m_modbusRtuDialogTimeout, "Timeout", 10, timeout, validationError) || !parseUint(m_modbusRtuDialogRetries, "Retries", 0, retries, validationError)) {
        m_modbusRtuStatus = validationError;
        return true;
    }

    const quint64 baudrate = baudrateOptions().at(m_modbusRtuDialogBaudrateIndex);

    if (m_modbusRtuDialogMode == ModbusRtuDialogMode::Add) {
        api::ModbusRtuAddModbusRtuMasterParams request;
        request.serialPort = serialPort;
        request.baudrate = baudrate;
        request.timeout = timeout;
        request.numberOfRetries = retries;
        request.dataBits = dataBitsOptions().at(m_modbusRtuDialogDataBitsIndex);
        request.parity = parityOptions().at(m_modbusRtuDialogParityIndex);
        request.stopBits = stopBitsOptions().at(m_modbusRtuDialogStopBitsIndex);
        m_modbusRtuRequestPending = true;
        m_modbusRtuStatus = "Adding Modbus RTU master...";
        observeReply(m_client.sendRequest(api::ModbusRtuAddModbusRtuMasterMethod::methodName(), request.toJson()),
                     [this](const QJsonObject& message, const QString& transportError) { handleAddModbusRtuReply(message, transportError); });
        return true;
    }

    api::ModbusRtuReconfigureModbusRtuMasterParams request;
    request.modbusUuid = m_modbusRtuDialogUuid;
    request.serialPort = serialPort;
    request.baudrate = baudrate;
    request.timeout = timeout;
    request.numberOfRetries = retries;
    request.dataBits = dataBitsOptions().at(m_modbusRtuDialogDataBitsIndex);
    request.parity = parityOptions().at(m_modbusRtuDialogParityIndex);
    request.stopBits = stopBitsOptions().at(m_modbusRtuDialogStopBitsIndex);
    m_modbusRtuRequestPending = true;
    m_modbusRtuStatus = "Updating Modbus RTU master...";
    observeReply(m_client.sendRequest(api::ModbusRtuReconfigureModbusRtuMasterMethod::methodName(), request.toJson()),
                 [this](const QJsonObject& message, const QString& transportError) { handleReconfigureModbusRtuReply(message, transportError); });
    return true;
}

void Engine::handleFetchModbusRtuMastersReply(const QJsonObject& message, const QString& transportError)
{
    m_modbusRtuMastersPending = false;
    if (!transportError.isEmpty()) {
        m_modbusRtuStatus = "Failed to load Modbus RTU masters: " + transportError.toStdString();
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
        m_modbusRtuStatus = "Modbus RTU master request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_modbusRtuStatus = "Modbus RTU master request returned an error.";
        return;
    }

    const api::ModbusRtuGetModbusRtuMastersResponse response = api::ModbusRtuGetModbusRtuMastersResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.modbusError != api::ModbusRtuError::ModbusRtuErrorNoError) {
        m_modbusRtuStatus = "Modbus RTU master request failed: " + api::toString(response.modbusError).toStdString();
        return;
    }

    m_modbusRtuMasters.clear();
    if (response.modbusRtuMasters.has_value()) {
        for (const api::ModbusRtuMaster& master : *response.modbusRtuMasters) {
            m_modbusRtuMasters.push_back(master);
        }
    }
    m_modbusRtuMastersLoaded = true;
    m_modbusRtuStatus = "Loaded " + std::to_string(m_modbusRtuMasters.size()) + " Modbus RTU masters.";
    clampModbusRtuSelection();
}

void Engine::handleFetchModbusRtuSerialPortsReply(const QJsonObject& message, const QString& transportError)
{
    m_modbusRtuSerialPortsPending = false;
    if (!transportError.isEmpty()) {
        m_modbusRtuStatus = "Failed to load serial ports: " + transportError.toStdString();
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
        m_modbusRtuStatus = "Serial port request was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_modbusRtuStatus = "Serial port request returned an error.";
        return;
    }

    const api::ModbusRtuGetSerialPortsResponse response = api::ModbusRtuGetSerialPortsResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    m_modbusRtuSerialPorts.clear();
    for (const api::SerialPort& serialPort : response.serialPorts) {
        m_modbusRtuSerialPorts.push_back(serialPort);
    }
    m_modbusRtuSerialPortsLoaded = true;
}

void Engine::handleAddModbusRtuReply(const QJsonObject& message, const QString& transportError)
{
    m_modbusRtuRequestPending = false;
    if (!transportError.isEmpty()) {
        m_modbusRtuStatus = "Adding Modbus RTU master failed: " + transportError.toStdString();
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
        m_modbusRtuStatus = "Adding Modbus RTU master was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_modbusRtuStatus = "Adding Modbus RTU master returned an error.";
        return;
    }

    const api::ModbusRtuAddModbusRtuMasterResponse response = api::ModbusRtuAddModbusRtuMasterResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.modbusError != api::ModbusRtuError::ModbusRtuErrorNoError) {
        m_modbusRtuStatus = "Adding Modbus RTU master failed: " + api::toString(response.modbusError).toStdString();
        return;
    }

    m_modbusRtuStatus = "Added Modbus RTU master.";
    closeModbusRtuDialog();
    m_modbusRtuMastersLoaded = false;
    ensureModbusRtuLoaded();
}

void Engine::handleReconfigureModbusRtuReply(const QJsonObject& message, const QString& transportError)
{
    m_modbusRtuRequestPending = false;
    if (!transportError.isEmpty()) {
        m_modbusRtuStatus = "Updating Modbus RTU master failed: " + transportError.toStdString();
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
        m_modbusRtuStatus = "Updating Modbus RTU master was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_modbusRtuStatus = "Updating Modbus RTU master returned an error.";
        return;
    }

    const api::ModbusRtuReconfigureModbusRtuMasterResponse response = api::ModbusRtuReconfigureModbusRtuMasterResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.modbusError != api::ModbusRtuError::ModbusRtuErrorNoError) {
        m_modbusRtuStatus = "Updating Modbus RTU master failed: " + api::toString(response.modbusError).toStdString();
        return;
    }

    m_modbusRtuStatus = "Updated Modbus RTU master.";
    closeModbusRtuDialog();
    m_modbusRtuMastersLoaded = false;
    ensureModbusRtuLoaded();
}

void Engine::handleRemoveModbusRtuReply(const QJsonObject& message, const QString& transportError)
{
    m_modbusRtuRequestPending = false;
    if (!transportError.isEmpty()) {
        m_modbusRtuStatus = "Removing Modbus RTU master failed: " + transportError.toStdString();
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
        m_modbusRtuStatus = "Removing Modbus RTU master was unauthorized.";
        return;
    }
    if (status == QStringLiteral("error")) {
        m_modbusRtuStatus = "Removing Modbus RTU master returned an error.";
        return;
    }

    const api::ModbusRtuRemoveModbusRtuMasterResponse response = api::ModbusRtuRemoveModbusRtuMasterResponse::fromJson(message.value(QStringLiteral("params")).toObject());
    if (response.modbusError != api::ModbusRtuError::ModbusRtuErrorNoError) {
        m_modbusRtuStatus = "Removing Modbus RTU master failed: " + api::toString(response.modbusError).toStdString();
        return;
    }

    m_modbusRtuStatus = "Removed Modbus RTU master.";
    closeModbusRtuDialog();
    m_modbusRtuMastersLoaded = false;
    ensureModbusRtuLoaded();
}

} // namespace nymea
