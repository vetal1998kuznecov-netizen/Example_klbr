#include <QCoreApplication>
#include <clocale>

#include "tcpclient.h"

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  setlocale(LC_CTYPE, "rus");

  TcpClient client;
  client.Start();

  return app.exec();
}
