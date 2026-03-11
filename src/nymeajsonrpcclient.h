// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace nymea {
class JsonRpcRequest
{
public:
    JsonRpcRequest() = default;
    JsonRpcRequest(int requestId, QString method, QJsonObject params = QJsonObject{});

    int requestId() const;
    const QString& method() const;
    const QJsonObject& params() const;
    QJsonObject toJson(const QString& authToken) const;

private:
    int m_requestId = -1;
    QString m_method;
    QJsonObject m_params;
};

class JsonRpcReply : public QObject
{
    Q_OBJECT

public:
    explicit JsonRpcReply(JsonRpcRequest request, QObject* parent = nullptr);

    int requestId() const;
    QString method() const;
    const JsonRpcRequest& request() const;
    bool isFinished() const;
    bool isTransportError() const;
    QString transportError() const;
    QJsonObject message() const;
    QString status() const;
    QJsonObject params() const;

signals:
    void finished();

public:
    void finishWithMessage(const QJsonObject& message);
    void finishWithTransportError(const QString& errorString);

private:
    JsonRpcRequest m_request;
    QJsonObject m_message;
    QString m_transportError;
    bool m_finished = false;
};

class NymeaJsonRpcClient
{
public:
    using NotificationHandler = std::function<void(const QJsonObject&)>;

    enum class TransportSecurity {
        PlainTcp,
        SslTls
    };

    NymeaJsonRpcClient();
    ~NymeaJsonRpcClient();

    bool connectToHost(const QString& host, quint16 port, TransportSecurity security = TransportSecurity::PlainTcp, int timeoutMs = 5000);
    void disconnectFromHost();

    bool isConnected() const;
    bool isEncrypted() const;
    QString peerCertificateFingerprint() const;
    QString lastError() const;

    void setAuthToken(const QString& token);
    void clearAuthToken();
    QString authToken() const;

    JsonRpcReply* sendRequest(const QString& method, const QJsonObject& params = QJsonObject{});
    void setNotificationHandler(NotificationHandler handler);

private:
    class NymeaJsonRpcClientWorker;

    struct State
    {
        bool isConnected = false;
        bool isEncrypted = false;
        QString peerCertificateFingerprint;
        QString authToken;
        QString lastError;
    };

    State stateSnapshot() const;
    void updateState(std::function<void(State&)> updater);

    NymeaJsonRpcClientWorker* m_worker = nullptr;
    class QThread* m_thread = nullptr;
    mutable std::mutex m_stateMutex;
    State m_state;
    mutable std::mutex m_notificationHandlerMutex;
    NotificationHandler m_notificationHandler;
};
} // namespace nymea
