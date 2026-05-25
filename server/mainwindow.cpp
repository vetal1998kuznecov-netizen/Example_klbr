#include "mainwindow.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>

#include "tcpserver.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  SetupUI();

  m_server_thread = new QThread(this);
  m_server.reset(new TcpServer());
  m_server->moveToThread(m_server_thread);

  connect(m_server.data(), &TcpServer::ClientsUpdated, this,
          &MainWindow::UpdateClientsTable);
  connect(m_server.data(), &TcpServer::DataReceived, this,
          &MainWindow::UpdateDataTable);
  connect(m_server.data(), &TcpServer::LogMessage, this, &MainWindow::LogEvent);

  qDebug() << "Поток для GUI:" << QThread::currentThread();

  connect(m_server_thread, &QThread::started, [server = m_server.data()]() {
    qDebug() << "Поток для TcpServer" << QThread::currentThread() << "запущен";
    server->Start();
  });

  connect(m_server_thread, &QThread::finished, [server = m_server.data()]() {
    server->Stop();
    qDebug() << "Поток для TcpServer" << QThread::currentThread()
             << "остановлен";
  });

  m_clients_table->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_clients_table, &QTableWidget::customContextMenuRequested, this,
          &MainWindow::ContextMenuClientsId);
  connect(this, &MainWindow::ChangePackages, m_server.data(),
          &TcpServer::ChangePackages);
}

MainWindow::~MainWindow() {
  delete ui;
  if (m_server_thread->isRunning()) {
    m_server_thread->quit();
    m_server_thread->wait();
  }
  m_server_thread->deleteLater();
}

void MainWindow::SetupUI() {
  setWindowTitle("Сервер");
  setMinimumSize(800, 600);

  QWidget *centralWidget = new QWidget(this);
  m_main_layout = new QVBoxLayout(centralWidget);

  // Кнопка старт/стоп
  m_start_stop_button = new QPushButton("Запустить сервер", this);
  connect(m_start_stop_button, &QPushButton::clicked, this,
          &MainWindow::OnStartStopClicked);
  m_main_layout->addWidget(m_start_stop_button);

  // Таблица клиентов
  m_clients_table = new QTableWidget(0, 3, this);
  m_clients_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_clients_table->setHorizontalHeaderLabels(
      {"ID клиента", "IP-адрес", "Статус"});
  m_clients_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  m_main_layout->addWidget(new QLabel("Подключенные клиенты:", this));
  m_main_layout->addWidget(m_clients_table);

  // Таблица данных
  m_data_table = new QTableWidget(0, 4, this);
  m_data_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_data_table->setHorizontalHeaderLabels(
      {"ID клиента", "Тип данных", "Содержимое", "Время"});
  m_data_table->horizontalHeader()->setStretchLastSection(true);
  m_main_layout->addWidget(new QLabel("Полученные данные:", this));
  m_main_layout->addWidget(m_data_table);

  // Лог событий
  m_log_widget = new QTextEdit(this);
  m_log_widget->setReadOnly(true);
  m_main_layout->addWidget(new QLabel("Лог событий:", this));
  m_main_layout->addWidget(m_log_widget);

  setCentralWidget(centralWidget);
}

void MainWindow::OnStartStopClicked() {
  if (m_server_thread->isRunning()) {
    // Останавливаем поток
    m_server_thread->quit();
    m_server_thread->wait();
    m_start_stop_button->setText("Запустить сервер");
    for (auto key : m_clientLog.keys()) {
      m_clientLog[key] = Not;
    }
  } else {
    // Запускаем поток
    m_server_thread->start();
    if (m_server_thread->isRunning())
      m_start_stop_button->setText("Остановить сервер");
    else
      LogEvent("Ошибка при запуске потиока");
  }
}

void MainWindow::UpdateClientsTable(
    const QList<QPair<QString, QString>> &clients) {
  for (int i = 0; i < clients.size(); i++) {
    bool flag_append = true;
    for (auto j = 0; j < m_clients_table->rowCount(); j++) {
      if (m_clients_table->item(j, 0)->text() == clients[i].first) {
        m_clients_table->item(j, 0)->setText((clients[i].first));
        if (clients[i].second.isEmpty()) {
          m_clients_table->item(j, 2)->setText("Отключен");
        } else {
          m_clients_table->item(j, 1)->setText(clients[i].second);
          m_clients_table->item(j, 2)->setText("Подключен");
        }
        flag_append = false;
        break;
      }
    }
    if (flag_append) {
      auto row = m_clients_table->rowCount();
      m_clients_table->setRowCount(row + 1);
      m_clients_table->setItem(row, 0, new QTableWidgetItem(clients[i].first));
      m_clients_table->setItem(row, 1, new QTableWidgetItem(clients[i].second));
      m_clients_table->setItem(row, 2, new QTableWidgetItem("Подключен"));
    }
  }
}

void MainWindow::UpdateDataTable(const QString &clientId,
                                 const QString &dataType, const QString &data,
                                 const QString &timestamp) {
  int row = m_data_table->rowCount();
  m_data_table->insertRow(row);
  m_data_table->setItem(row, 0, new QTableWidgetItem(clientId));
  m_data_table->setItem(row, 1, new QTableWidgetItem(dataType));
  m_data_table->setItem(row, 2, new QTableWidgetItem(data));
  m_data_table->setItem(row, 3, new QTableWidgetItem(timestamp));
}

void MainWindow::LogEvent(const QString &message) {
  m_log_widget->append(
      QString("[%1] %2")
          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
          .arg(message));
}

void MainWindow::ContextMenuClientsId(const QPoint &pos) {
  auto index = m_clients_table->indexAt(pos);
  if (!index.isValid()) {
    return;
  }
  if (m_clients_table->item(index.row(), 2)->text() == "Отключен") {
    return;
  }

  QMenu *menu = new QMenu(this);
  auto *action_log = new QAction("Получать только логи вариннг", this);
  auto *action_any = new QAction("Получать любые пакеты", this);
  auto *action_not = new QAction("Остановить пакеты", this);
  menu->addAction(action_log);
  menu->addAction(action_any);
  menu->addAction(action_not);
  auto clientId = m_clients_table->item(index.row(), 0)->text();
  switch (m_clientLog.value(clientId)) {
    case LogWarning:
      action_log->setEnabled(false);
      break;
    case Any:
      action_any->setEnabled(false);
      break;
    case Not:
    default:
      action_not->setEnabled(false);
      break;
  }
  connect(action_log, &QAction::triggered, [this, clientId]() {
    emit ChangePackages("LogWarning", clientId);
    m_clientLog.insert(clientId, LogWarning);
  });
  connect(action_any, &QAction::triggered, [this, clientId]() {
    emit ChangePackages("Any", clientId);
    m_clientLog.insert(clientId, Any);
  });
  connect(action_not, &QAction::triggered, [this, clientId]() {
    emit ChangePackages("Not", clientId);
    m_clientLog.insert(clientId, Not);
  });
  menu->popup(m_clients_table->viewport()->mapToGlobal(pos));
}
