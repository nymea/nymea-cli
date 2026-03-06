#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QSslSocket>
#include <QString>

#include <optional>

namespace nymea {

class NymeaJsonRpcClient
{
public:
    enum class TransportSecurity {
        PlainTcp,
        SslTls
    };

    bool connectToHost(const QString& host, quint16 port, TransportSecurity security = TransportSecurity::PlainTcp, int timeoutMs = 5000);
    void disconnectFromHost();

    bool isConnected() const;
    bool isEncrypted() const;
    QString lastError() const;

    void setAuthToken(const QString& token);
    void clearAuthToken();
    QString authToken() const;

    int sendRequest(const QString& method, const QJsonObject& params = QJsonObject{});
    std::optional<QJsonObject> waitForMessage(int timeoutMs = 5000);

private:
    std::optional<QJsonObject> takeNextMessage();

    QSslSocket m_socket;
    QByteArray m_receiveBuffer;
    int m_nextRequestId = 1;
    QString m_authToken;
    QString m_lastError;
};

} // namespace nymea
