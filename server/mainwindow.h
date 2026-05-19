#pragma once

#include <QHBoxLayout>
#include <QMainWindow>
#include <QPushButton>
#include <QScopedPointer>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TcpServer;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

 private:
  void SetupUI();

  Ui::MainWindow *ui;
  QScopedPointer<TcpServer> m_server;
  QTableWidget *m_clients_table;
  QTableWidget *m_data_table;
  QTextEdit *m_log_widget;
  QPushButton *m_start_stop_button;
  QVBoxLayout *m_main_layout;
  QThread *m_server_thread;

 signals:
  void ChangePackages(const QString &type, const QString &clientId);

 private slots:
  void OnStartStopClicked();
  void UpdateClientsTable(const QList<QPair<QString, QString>> &clients);
  void UpdateDataTable(const QString &clientId, const QString &dataType,
                       const QString &data, const QString &timestamp);
  void LogEvent(const QString &message);
  //! контекстное меню для таблицы с клиентами
  void ContextMenuClientsId(const QPoint &pos);
};
