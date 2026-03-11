#include "nymeajsonrpcclient.h"

#include <QCryptographicHash>
#include <QHash>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMetaObject>
#include <QSslCertificate>
#include <QSslSocket>
#include <QThread>
#include <QtGlobal>

#include <utility>

namespace nymea {
JsonRpcRequest::JsonRpcRequest(int requestId, QString method, QJsonObject params)
    : m_requestId(requestId)
    , m_method(std::move(method))
    , m_params(std::move(params))
{}

int JsonRpcRequest::requestId() const
{
    return m_requestId;
}

const QString& JsonRpcRequest::method() const
{
    return m_method;
}

const QJsonObject& JsonRpcRequest::params() const
{
    return m_params;
}

QJsonObject JsonRpcRequest::toJson(const QString& authToken) const
{
    QJsonObject request;
    request.insert(QStringLiteral("id"), m_requestId);
    request.insert(QStringLiteral("method"), m_method);
    if (!m_params.isEmpty()) {
        request.insert(QStringLiteral("params"), m_params);
    }
    if (!authToken.isEmpty()) {
        request.insert(QStringLiteral("token"), authToken);
    }
    return request;
}

JsonRpcReply::JsonRpcReply(JsonRpcRequest request, QObject* parent)
    : QObject(parent)
    , m_request(std::move(request))
{}

int JsonRpcReply::requestId() const
{
    return m_request.requestId();
}

QString JsonRpcReply::method() const
{
    return m_request.method();
}

const JsonRpcRequest& JsonRpcReply::request() const
{
    return m_request;
}

bool JsonRpcReply::isFinished() const
{
    return m_finished;
}

bool JsonRpcReply::isTransportError() const
{
    return !m_transportError.isEmpty();
}

QString JsonRpcReply::transportError() const
{
    return m_transportError;
}

QJsonObject JsonRpcReply::message() const
{
    return m_message;
}

QString JsonRpcReply::status() const
{
    return m_message.value(QStringLiteral("status")).toString();
}

QJsonObject JsonRpcReply::params() const
{
    return m_message.value(QStringLiteral("params")).toObject();
}

void JsonRpcReply::finishWithMessage(const QJsonObject& message)
{
    if (m_finished) {
        return;
    }

    m_finished = true;
    m_message = message;
    emit finished();
}

void JsonRpcReply::finishWithTransportError(const QString& errorString)
{
    if (m_finished) {
        return;
    }

    m_finished = true;
    m_transportError = errorString;
    emit finished();
}

class NymeaJsonRpcClient::NymeaJsonRpcClientWorker : public QObject
{
    Q_OBJECT

public:
    explicit NymeaJsonRpcClientWorker(QObject* parent = nullptr)
        : QObject(parent)
        , m_socket(new QSslSocket(this))
    {
        connect(m_socket, &QSslSocket::readyRead, this, &NymeaJsonRpcClientWorker::onReadyRead);
        connect(m_socket, &QSslSocket::disconnected, this, &NymeaJsonRpcClientWorker::onDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        connect(m_socket, &QSslSocket::errorOccurred, this, &NymeaJsonRpcClientWorker::onSocketErrorOccurred);
#else
        connect(m_socket, static_cast<void (QSslSocket::*)(QAbstractSocket::SocketError)>(&QSslSocket::error), this, &NymeaJsonRpcClientWorker::onSocketErrorOccurred);
#endif
        connect(m_socket, &QSslSocket::encrypted, this, &NymeaJsonRpcClientWorker::emitStateChanged);
    }

    bool connectToHost(const QString& host, quint16 port, NymeaJsonRpcClient::TransportSecurity security, int timeoutMs)
    {
        disconnectFromHost();

        if (security == NymeaJsonRpcClient::TransportSecurity::SslTls) {
            if (!QSslSocket::supportsSsl()) {
                m_lastError = QStringLiteral("SSL support is not available on this system.");
                emitStateChanged();
                return false;
            }

            m_socket->setPeerVerifyMode(QSslSocket::VerifyNone);
            m_socket->connectToHostEncrypted(host, port);
            if (!m_socket->waitForEncrypted(timeoutMs)) {
                m_lastError = m_socket->errorString();
                emitStateChanged();
                return false;
            }
        } else {
            m_socket->setPeerVerifyMode(QSslSocket::AutoVerifyPeer);
            m_socket->connectToHost(host, port);
            if (!m_socket->waitForConnected(timeoutMs)) {
                m_lastError = m_socket->errorString();
                emitStateChanged();
                return false;
            }
        }

        m_receiveBuffer.clear();
        m_lastError.clear();
        emitStateChanged();
        return true;
    }

    void disconnectFromHost()
    {
        failPendingReplies(QStringLiteral("Connection closed"));

        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->disconnectFromHost();
            m_socket->waitForDisconnected(1000);
        }

        m_receiveBuffer.clear();
        emitStateChanged();
    }

    void setAuthToken(const QString& token)
    {
        m_authToken = token;
        emitStateChanged();
    }

    void clearAuthToken()
    {
        m_authToken.clear();
        emitStateChanged();
    }

    JsonRpcReply* sendRequest(const QString& method, const QJsonObject& params)
    {
        const int requestId = m_nextRequestId++;
        JsonRpcReply* reply = new JsonRpcReply(JsonRpcRequest(requestId, method, params));
        reply->moveToThread(thread());

        if (m_socket->state() != QAbstractSocket::ConnectedState) {
            m_lastError = QStringLiteral("Not connected");
            reply->finishWithTransportError(m_lastError);
            emitStateChanged();
            return reply;
        }

        const QByteArray payload = QJsonDocument(reply->request().toJson(m_authToken)).toJson(QJsonDocument::Compact) + '\n';
        const qint64 written = m_socket->write(payload);
        if (written != payload.size()) {
            m_lastError = m_socket->errorString();
            reply->finishWithTransportError(m_lastError);
            emitStateChanged();
            return reply;
        }

        if (!m_socket->waitForBytesWritten(3000)) {
            m_lastError = m_socket->errorString();
            reply->finishWithTransportError(m_lastError);
            emitStateChanged();
            return reply;
        }

        m_pendingReplies.insert(reply->requestId(), reply);
        m_lastError.clear();
        emitStateChanged();
        return reply;
    }

    bool isConnected() const { return m_socket->state() == QAbstractSocket::ConnectedState; }

    bool isEncrypted() const { return m_socket->isEncrypted(); }

    QString peerCertificateFingerprint() const
    {
        if (!m_socket->isEncrypted()) {
            return {};
        }

        const QSslCertificate certificate = m_socket->peerCertificate();
        if (certificate.isNull()) {
            return {};
        }

        return certificate.digest(QCryptographicHash::Sha256).toHex(':').toUpper();
    }

    QString lastError() const { return m_lastError; }

    QString authToken() const { return m_authToken; }

signals:
    void stateChanged(bool connected, bool encrypted, const QString& peerCertificateFingerprint, const QString& authToken, const QString& lastError);
    void notificationReceived(const QJsonObject& message);

private:
    void onReadyRead()
    {
        m_receiveBuffer.append(m_socket->readAll());

        while (const std::optional<QJsonObject> message = takeNextMessage()) {
            if (message->contains(QStringLiteral("notification"))) {
                emit notificationReceived(*message);
                continue;
            }

            const int requestId = message->value(QStringLiteral("id")).toInt(-1);
            JsonRpcReply* reply = m_pendingReplies.take(requestId);
            if (reply == nullptr) {
                continue;
            }

            reply->finishWithMessage(*message);
        }

        emitStateChanged();
    }

    void onDisconnected()
    {
        m_lastError.clear();
        failPendingReplies(QStringLiteral("Connection closed"));
        emitStateChanged();
    }

    void onSocketErrorOccurred(QAbstractSocket::SocketError)
    {
        m_lastError = m_socket->errorString();
        failPendingReplies(m_lastError);
        emitStateChanged();
    }

    void failPendingReplies(const QString& errorString)
    {
        const auto pendingReplies = m_pendingReplies;
        m_pendingReplies.clear();
        for (JsonRpcReply* reply : pendingReplies) {
            if (reply != nullptr) {
                reply->finishWithTransportError(errorString);
            }
        }
    }

    std::optional<QJsonObject> takeNextMessage()
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

    void emitStateChanged() { emit stateChanged(isConnected(), isEncrypted(), peerCertificateFingerprint(), m_authToken, m_lastError); }

    QSslSocket* m_socket = nullptr;
    QByteArray m_receiveBuffer;
    QHash<int, JsonRpcReply*> m_pendingReplies;
    int m_nextRequestId = 1;
    QString m_authToken;
    QString m_lastError;
};

NymeaJsonRpcClient::NymeaJsonRpcClient()
    : m_thread(new QThread())
{
    m_worker = new NymeaJsonRpcClientWorker();
    m_worker->moveToThread(m_thread);
    QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    QObject::connect(m_worker,
                     &NymeaJsonRpcClientWorker::stateChanged,
                     [this](bool connected, bool encrypted, const QString& peerCertificateFingerprint, const QString& authToken, const QString& lastError) {
                         updateState([&](State& state) {
                             state.isConnected = connected;
                             state.isEncrypted = encrypted;
                             state.peerCertificateFingerprint = peerCertificateFingerprint;
                             state.authToken = authToken;
                             state.lastError = lastError;
                         });
                     });

    QObject::connect(m_worker, &NymeaJsonRpcClientWorker::notificationReceived, [this](const QJsonObject& message) {
        NotificationHandler handler;
        {
            std::lock_guard<std::mutex> lock(m_notificationHandlerMutex);
            handler = m_notificationHandler;
        }
        if (handler) {
            handler(message);
        }
    });

    m_thread->start();
}

NymeaJsonRpcClient::~NymeaJsonRpcClient()
{
    if (m_thread == nullptr) {
        return;
    }

    QMetaObject::invokeMethod(m_worker, [this]() { m_worker->disconnectFromHost(); }, Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait();
    m_worker = nullptr;
    delete m_thread;
    m_thread = nullptr;
}

bool NymeaJsonRpcClient::connectToHost(const QString& host, quint16 port, TransportSecurity security, int timeoutMs)
{
    bool connected = false;
    QMetaObject::invokeMethod(
        m_worker, [this, &connected, host, port, security, timeoutMs]() { connected = m_worker->connectToHost(host, port, security, timeoutMs); }, Qt::BlockingQueuedConnection);
    return connected;
}

void NymeaJsonRpcClient::disconnectFromHost()
{
    QMetaObject::invokeMethod(m_worker, [this]() { m_worker->disconnectFromHost(); }, Qt::BlockingQueuedConnection);
}

bool NymeaJsonRpcClient::isConnected() const
{
    return stateSnapshot().isConnected;
}

bool NymeaJsonRpcClient::isEncrypted() const
{
    return stateSnapshot().isEncrypted;
}

QString NymeaJsonRpcClient::peerCertificateFingerprint() const
{
    return stateSnapshot().peerCertificateFingerprint;
}

QString NymeaJsonRpcClient::lastError() const
{
    return stateSnapshot().lastError;
}

void NymeaJsonRpcClient::setAuthToken(const QString& token)
{
    QMetaObject::invokeMethod(m_worker, [this, token]() { m_worker->setAuthToken(token); }, Qt::BlockingQueuedConnection);
}

void NymeaJsonRpcClient::clearAuthToken()
{
    QMetaObject::invokeMethod(m_worker, [this]() { m_worker->clearAuthToken(); }, Qt::BlockingQueuedConnection);
}

QString NymeaJsonRpcClient::authToken() const
{
    return stateSnapshot().authToken;
}

JsonRpcReply* NymeaJsonRpcClient::sendRequest(const QString& method, const QJsonObject& params)
{
    JsonRpcReply* reply = nullptr;
    QMetaObject::invokeMethod(m_worker, [this, &reply, method, params]() { reply = m_worker->sendRequest(method, params); }, Qt::BlockingQueuedConnection);
    return reply;
}

void NymeaJsonRpcClient::setNotificationHandler(NotificationHandler handler)
{
    std::lock_guard<std::mutex> lock(m_notificationHandlerMutex);
    m_notificationHandler = std::move(handler);
}

NymeaJsonRpcClient::State NymeaJsonRpcClient::stateSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_state;
}

void NymeaJsonRpcClient::updateState(std::function<void(State&)> updater)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    updater(m_state);
}
} // namespace nymea

#include "nymeajsonrpcclient.moc"
