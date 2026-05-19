#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class TcpClient : public QObject {
  Q_OBJECT

 public:
  explicit TcpClient(QObject *parent = nullptr);
  void Start();

 private:
  enum enumDataType { Any = -1, LogWarning = 2 };
  QTcpSocket *m_socket;
  QTimer *m_connection_timer;
  QTimer *m_data_timer;
  QString m_client_id;
  enumDataType m_data_type;
  bool m_connected;
  bool m_confirmed;

  QJsonDocument GenerateNetworkMetrics();
  QJsonDocument GenerateDeviceStatus();
  QJsonDocument GenerateLog();
  void SsetupConnection();

 private slots:
  void ConnectToServer();
  void OnConnected();
  void OnReadyRead();
  void SendData();
  void OnDisconnected();
};
