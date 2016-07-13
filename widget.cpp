#include "widget.h"

#include "epubparser.h"
#include <QDebug>

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      m_parser(new EPubParser(this))
{
    connect(m_parser, &EPubParser::errorHappened, [](QString error) {
        qWarning() << error;
    });
    setWindowFlags(Qt::Dialog);
    resize(600, 800);
    m_parser->loadFile("Lorem Ipsum.epub");
}

Widget::~Widget()
{

}
