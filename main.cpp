#include "widget.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(true);

    Widget *w = new Widget;
    w->setAttribute(Qt::WA_DeleteOnClose);
    if (argc > 1) {
        if (!w->loadFile(argv[1])) {
            qWarning() << "Failed to load" << argv[1];
            return 1;
        }
    } else {
        if (!w->loadFile()) {
            return 1;
        }
    }
    w->show();

    return a.exec();
}
