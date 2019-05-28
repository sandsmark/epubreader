#include "widget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //a.setQuitOnLastWindowClosed(true);
    Widget w;

    return a.exec();
}
