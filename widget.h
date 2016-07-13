#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

class EPubContainer;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);
    ~Widget();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    EPubContainer *m_parser;
    QImage m_cover;
};

#endif // WIDGET_H