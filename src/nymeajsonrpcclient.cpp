#include "nymeajsonrpcclient.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonParseError>

namespace nymea {

bool NymeaJsonRpcClient::connectToHost(const QString& host, quint16 port, TransportSecurity security, int timeoutMs)
{
    disconnectFromHost();

    if (security == TransportSecurity::SslTls) {
        if (!QSslSocket::supportsSsl()) {
            m_lastError = QStringLiteral("SSL support is not available on this system.");
            return false;
        }

        // nymea servers are often configured with self-signed certificates.
        m_socket.setPeerVerifyMode(QSslSocket::VerifyNone);
        m_socket.connectToHostEncrypted(host, port);
        if (!m_socket.waitForEncrypted(timeoutMs)) {
            m_lastError = m_socket.errorString();
            return false;
        }
    } else {
        m_socket.setPeerVerifyMode(QSslSocket::AutoVerifyPeer);
        m_socket.connectToHost(host, port);
        if (!m_socket.waitForConnected(timeoutMs)) {
            m_lastError = m_socket.errorString();
            return false;
        }
    }

    m_receiveBuffer.clear();
    m_lastError.clear();
    return true;
}

void NymeaJsonRpcClient::disconnectFromHost()
{
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        m_socket.disconnectFromHost();
        m_socket.waitForDisconnected(1000);
    }
    m_receiveBuffer.clear();
}

bool NymeaJsonRpcClient::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

bool NymeaJsonRpcClient::isEncrypted() const
{
    return m_socket.isEncrypted();
}

QString NymeaJsonRpcClient::lastError() const
{
    return m_lastError;
}

void NymeaJsonRpcClient::setAuthToken(const QString& token)
{
    m_authToken = token;
}

void NymeaJsonRpcClient::clearAuthToken()
{
    m_authToken.clear();
}

QString NymeaJsonRpcClient::authToken() const
{
    return m_authToken;
}

int NymeaJsonRpcClient::sendRequest(const QString& method, const QJsonObject& params)
{
    if (!isConnected()) {
        m_lastError = QStringLiteral("Not connected");
        return -1;
    }

    const int requestId = m_nextRequestId++;

    QJsonObject request;
    request.insert(QStringLiteral("id"), requestId);
    request.insert(QStringLiteral("method"), method);
    request.insert(QStringLiteral("params"), params);
    if (!m_authToken.isEmpty()) {
        request.insert(QStringLiteral("token"), m_authToken);
    }

    const QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
    const qint64 written = m_socket.write(payload);
    if (written != payload.size()) {
        m_lastError = m_socket.errorString();
        return -1;
    }

    if (!m_socket.waitForBytesWritten(3000)) {
        m_lastError = m_socket.errorString();
        return -1;
    }

    return requestId;
}

std::optional<QJsonObject> NymeaJsonRpcClient::waitForMessage(int timeoutMs)
{
    if (auto message = takeNextMessage(); message.has_value()) {
        return message;
    }

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (!m_socket.waitForReadyRead(remaining)) {
            if (m_socket.error() != QAbstractSocket::SocketTimeoutError) {
                m_lastError = m_socket.errorString();
            }
            break;
        }

        m_receiveBuffer.append(m_socket.readAll());
        if (auto message = takeNextMessage(); message.has_value()) {
            return message;
        }
    }

    if (m_lastError.isEmpty()) {
        m_lastError = QStringLiteral("Timed out waiting for JSON-RPC reply");
    }
    return std::nullopt;
}

std::optional<QJsonObject> NymeaJsonRpcClient::takeNextMessage()
{
    while (true) {
        const int lineBreak = m_receiveBuffer.indexOf('\n');
        if (lineBreak < 0) {
            return std::nullopt;
        }

        QByteArray line = m_receiveBuffer.left(lineBreak).trimmed();
        m_receiveBuffer.remove(0, lineBreak + 1);
        if (line.isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            m_lastError = QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
            continue;
        }

        if (!document.isObject()) {
            m_lastError = QStringLiteral("Unexpected non-object JSON payload");
            continue;
        }

        m_lastError.clear();
        return document.object();
    }
}

} // namespace nymea
