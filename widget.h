#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

class EPubParser;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);
    ~Widget();

private:
    EPubParser *m_parser;
};

#endif // WIDGET_H
