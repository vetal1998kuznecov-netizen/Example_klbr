#include "tcpserver.h"

#include <QDateTime>

TcpServer::TcpServer(QObject *parent)
    : QTcpServer(parent), m_next_client_id(1) {
  if (!QMetaType::isRegistered(QMetaType::type("qintptr"))) {
    qRegisterMetaType<qintptr>("qintptr");
  }
  qRegisterMetaType<QList<QPair<QString, QString>>>();
}

bool TcpServer::Start() {
  if (!isListening()) {
    if (!listen(QHostAddress::Any, 12345)) {
      emit LogMessage("Ошибка запуска сервера: " + errorString());
      return false;
    }
    emit LogMessage("Сервер запущен на порту 12345");
    return true;
  }
  return false;
}

void TcpServer::Stop() {
  close();
  emit LogMessage("Сервер остановлен");
  for (auto socket : m_client_ids) {
    if (socket.first->isOpen()) {
      socket.first->disconnectFromHost();
    }
  }
}

void TcpServer::IncomingConnection(qintptr socketDescriptor) {
  QTcpSocket *clientSocket = new QTcpSocket(this);

  if (!clientSocket->setSocketDescriptor(socketDescriptor)) {
    delete clientSocket;
    emit LogMessage("Ошибка создания сокета для клиента");
    return;
  }

  connect(clientSocket, &QTcpSocket::readyRead, this,
          &TcpServer::ReadClientData);
  connect(clientSocket, &QTcpSocket::disconnected, this,
          &TcpServer::ClientDisconnected);

  SendConfirmation(clientSocket);
}

void TcpServer::SendConfirmation(QTcpSocket *clientSocket) {
  QJsonObject confirmation;
  confirmation["status"] = "connected";
  confirmation["message"] = "Connection confirmed";
  QJsonDocument doc(confirmation);
  clientSocket->write(doc.toJson() + "\n");
}

void TcpServer::ProcessJsonData(QTcpSocket *client,
                                const QJsonDocument &jsonDoc) {
  QJsonObject jsonObj = jsonDoc.object();
  QString dataType = jsonObj["type"].toString();
  QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
  QString clientId = jsonObj["client_name"].toString();
  if (clientId.isEmpty()) {
    return;
  }

  bool flag_append = true, flag_update = false;
  for (auto i = 0; i < m_client_ids.count(); i++) {
    if (m_client_ids[i].second == clientId) {
      if (m_client_ids[i].first != client) {
        m_client_ids[i].first = client;
        flag_update = true;
      }
      flag_append = false;
      break;
    }
  }

  if (flag_append) {
    m_client_ids.append(qMakePair(client, clientId));
    flag_update = true;
  }

  if (flag_update) {
    QList<QPair<QString, QString>> clients;
    for (auto it = m_client_ids.begin(); it != m_client_ids.end(); ++it) {
      QHostAddress address = it->first->peerAddress();

      // Превращаем адрес в строку для чтения
      QString ipString = address.toString();

      // Если адрес в формате IPv6-mapped IPv4, очищаем его
      if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        bool canConvert;
        quint32 ipv4 = address.toIPv4Address(&canConvert);
        if (canConvert) {
          ipString = QHostAddress(ipv4).toString();
        }
      }

      if (ipString == "::1") {
        ipString = "localhost";
      }

      clients.append(qMakePair(it->second, ipString));
    }
    emit ClientsUpdated(clients);
  }

  QString dataContent;

  if (dataType == "NetworkMetrics") {
    dataContent = QString("Bandwidth: %1, Latency: %2, Loss: %3")
                      .arg(jsonObj["bandwidth"].toDouble())
                      .arg(jsonObj["latency"].toDouble())
                      .arg(jsonObj["packet_loss"].toDouble());
  } else if (dataType == "DeviceStatus") {
    dataContent = QString("Uptime: %1s, CPU: %2%, Memory: %3%")
                      .arg(jsonObj["uptime"].toInt())
                      .arg(jsonObj["cpu_usage"].toInt())
                      .arg(jsonObj["memory_usage"].toInt());
  } else if (dataType == "Log") {
    dataContent = QString("%1 (Severity: %2)")
                      .arg(jsonObj["message"].toString())
                      .arg(jsonObj["severity"].toString());
  } else {
    dataContent = jsonDoc.toJson(QJsonDocument::Compact);
  }

  emit DataReceived(clientId, dataType, dataContent, timestamp);
  emit LogMessage(
      QString("Получены данные от %1: %2").arg(clientId).arg(dataType));
}

void TcpServer::ChangePackages(const QString &type, const QString &clientId) {
  for (auto i = 0; i < m_client_ids.count(); i++) {
    if (clientId == m_client_ids[i].second) {
      QJsonObject changePackages;
      changePackages["type"] = type;
      QJsonDocument doc(changePackages);
      m_client_ids[i].first->write(doc.toJson() + "\n");
      break;
    }
  }
}

void TcpServer::ReadClientData() {
  QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
  if (!client) {
    return;
  }

  QByteArray data;
  while (client->canReadLine()) {
    data.append(client->readLine());
  }
  QJsonParseError parseError;
  QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
  QString clientId;

  if (parseError.error != QJsonParseError::NoError) {
    emit LogMessage(
        QString("Ошибка парсинга JSON: %1").arg(parseError.errorString()));
    return;
  }

  ProcessJsonData(client, jsonDoc);
}

void TcpServer::ClientDisconnected() {
  QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());

  if (!client) {
    return;
  }

  for (auto i = 0; i < m_client_ids.count(); i++) {
    if (m_client_ids[i].first == client) {
      emit ClientsUpdated(QList<QPair<QString, QString>>()
                          << (qMakePair(m_client_ids[i].second, QString())));
      emit LogMessage(
          QString("Клиент отключен: %1").arg(m_client_ids[i].second));
      break;
    }
  }
}
