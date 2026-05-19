#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

class TcpServer : public QTcpServer {
  Q_OBJECT

 public:
  explicit TcpServer(QObject *parent = nullptr);
  ~TcpServer() { Stop(); };
  bool Start();
  void Stop();

 protected:
  virtual void IncomingConnection(qintptr socketDescriptor) override;

 private:
  void SendConfirmation(QTcpSocket *clientSocket);
  void ProcessJsonData(QTcpSocket *client, const QJsonDocument &jsonDoc);

  QList<QPair<QTcpSocket *, QString>> m_client_ids;
  int m_next_client_id;

 signals:
  void ClientsUpdated(const QList<QPair<QString, QString>> &clients);
  void DataReceived(const QString &clientId, const QString &dataType,
                    const QString &data, const QString &timestamp);
  void LogMessage(const QString &message);

 public slots:
  void ChangePackages(const QString &type, const QString &clientId);

 private slots:
  void ReadClientData();
  void ClientDisconnected();
};
