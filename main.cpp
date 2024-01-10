#include <QCoreApplication>
#include <QDebug>
#include "controller.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    new Controller;
    return a.exec();
}
