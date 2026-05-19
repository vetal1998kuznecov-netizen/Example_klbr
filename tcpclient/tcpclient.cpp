#include "tcpclient.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRandomGenerator>
#include <QUuid>

TcpClient::TcpClient(QObject *parent)
    : QObject(parent),
      m_data_type(Any),
      m_connected(false),
      m_confirmed(false) {
  m_socket = new QTcpSocket(this);
  m_connection_timer = new QTimer(this);
  m_data_timer = new QTimer(this);
  m_client_id = "Client_" + QUuid::createUuid().toString().mid(1, 8);
  qDebug() << "client name = " << m_client_id;

  SsetupConnection();
}

void TcpClient::Start() { ConnectToServer(); }

void TcpClient::SsetupConnection() {
  connect(m_socket, &QTcpSocket::connected, this, &TcpClient::OnConnected);
  connect(m_socket, &QTcpSocket::readyRead, this, &TcpClient::OnReadyRead);
  connect(m_socket, &QTcpSocket::disconnected, this,
          &TcpClient::OnDisconnected);
  connect(m_connection_timer, &QTimer::timeout, this,
          &TcpClient::ConnectToServer);
  connect(m_data_timer, &QTimer::timeout, this, &TcpClient::SendData);

  m_connection_timer->start(5000);  // Попытка каждые 5 секунд
}

void TcpClient::ConnectToServer() {
  if (!m_connected) {
    qDebug() << "Попытка подключения к серверу...";
    if (m_socket->state() != QAbstractSocket::ConnectingState) {
      m_socket->connectToHost("localhost", 12345);
    }
  }
}

void TcpClient::OnConnected() {
  m_connected = true;
  m_connection_timer->stop();
  qDebug() << "Подключено к серверу. Ожидание подтверждения...";

  // отправляем пакет авторизации
  QJsonObject obj;
  obj["type"] = "Authorization";
  obj["client_name"] = m_client_id;
  auto data = QJsonDocument(obj);
  if (m_socket->state() == QTcpSocket::ConnectedState) {
    m_socket->write(data.toJson() + "\n");
    qDebug() << "Отправлено:" << data.toJson(QJsonDocument::Compact);
  }
}

void TcpClient::OnReadyRead() {
  QByteArray data;
  while (m_socket->canReadLine()) {
    data.append(m_socket->readLine());
  }
  QJsonParseError parseError;
  QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    qDebug() << "Ошибка парсинга JSON:" << parseError.errorString();
    return;
  }

  QJsonObject jsonObj = jsonDoc.object();
  if (jsonObj["status"] == "connected") {
    m_confirmed = true;
    qDebug() << "Подтверждение подключения получено";
  }

  if (m_confirmed) {
    auto type = jsonObj["type"].toString();
    if (type == "Log") {
      m_data_type = LogWarning;
      if (!m_data_timer->isActive()) {
        qDebug() << "Отправляем только пакеты \"Log\", с статусом "
                    "\"Warning\"";
        m_data_timer->start(QRandomGenerator::global()->bounded(
            10, 100));  // Задержка 10–100 мс
      }
    } else if (type == "Any") {
      m_data_type = Any;
      if (!m_data_timer->isActive()) {
        qDebug() << "Начинаем отправлять любые пакеты";
        m_data_timer->start(QRandomGenerator::global()->bounded(
            10, 100));  // Задержка 10–100 мс
      }
    }
  }
}

void TcpClient::SendData() {
  int dataType;
  switch (m_data_type) {
    case LogWarning:
      dataType = m_data_type;
      break;
    default:
      dataType = QRandomGenerator::global()->bounded(3);
  }

  QJsonDocument data;

  switch (dataType) {
    case 0:
      data = GenerateNetworkMetrics();
      break;
    case 1:
      data = GenerateDeviceStatus();
      break;
    case 2:
      data = GenerateLog();
      break;
  }

  if (m_socket->state() == QTcpSocket::ConnectedState) {
    m_socket->write(data.toJson() + "\n");
  }
}

void TcpClient::OnDisconnected() {
  m_connected = m_confirmed = false;
  qDebug() << "Отключено от сервера. Переподключение...";
  m_data_timer->stop();
  m_connection_timer->start();
}

QJsonDocument TcpClient::GenerateNetworkMetrics() {
  QJsonObject obj;
  obj["client_name"] = m_client_id;
  obj["type"] = "NetworkMetrics";
  obj["bandwidth"] = QRandomGenerator::global()->bounded(200.) + 50.;
  obj["latency"] = QRandomGenerator::global()->bounded(50.) + 5;
  obj["packet_loss"] = QRandomGenerator::global()->bounded(10.) / 100;
  return QJsonDocument(obj);
}

QJsonDocument TcpClient::GenerateDeviceStatus() {
  QJsonObject obj;
  obj["client_name"] = m_client_id;
  obj["type"] = "DeviceStatus";
  obj["uptime"] = QDateTime::currentMSecsSinceEpoch() % 86400;
  obj["cpu_usage"] = QRandomGenerator::global()->bounded(10, 90);
  obj["memory_usage"] = QRandomGenerator::global()->bounded(20, 80);
  return QJsonDocument(obj);
}

QJsonDocument TcpClient::GenerateLog() {
  QJsonObject obj;
  obj["client_name"] = m_client_id;
  obj["type"] = "Log";

  static const QStringList kMessages = {
      "Interface eth0 restarted", "CPU temperature critical",
      "Memory usage high", "Network connection established",
      "System reboot initiated"};
  static const QStringList kSeverities = {"INFO", "WARNING", "ERROR"};

  obj["message"] =
      kMessages[QRandomGenerator::global()->bounded(kMessages.size())];
  QString str_severity;
  switch (m_data_type) {
    case LogWarning:
      str_severity = kSeverities[1];
      break;
    default:
      str_severity =
          kSeverities[QRandomGenerator::global()->bounded(kSeverities.size())];
  }
  obj["severity"] = str_severity;

  // Имитация разной длины логов
  if (QRandomGenerator::global()->bounded(3) == 0) {
    auto count_spam = QRandomGenerator::global()->bounded(0, 500);
    QString longMessage = obj["message"].toString() + ": ";
    longMessage.reserve(longMessage.count() + count_spam);
    for (int i = 0; i < count_spam; ++i) {
      longMessage += "x";
    }
    obj["message"] =
        longMessage + QString("count of \"x\" =%1").arg(count_spam);
  }

  return QJsonDocument(obj);
}
