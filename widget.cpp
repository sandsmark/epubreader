#include "widget.h"

#include "epubcontainer.h"
#include <QDebug>
#include <QPainter>

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      m_parser(new EPubContainer(this))
{
    connect(m_parser, &EPubContainer::errorHappened, [](QString error) {
        qWarning() << error;
    });
    setWindowFlags(Qt::Dialog);
    resize(600, 800);
    if (!m_parser->openFile("test.epub")) {
        return;
    }

    QString coverId = m_parser->getMetadata("cover");
    if (!coverId.isEmpty()) {
        m_cover = m_parser->getImage(coverId);
    }
}

Widget::~Widget()
{

}

void Widget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.drawImage(rect(), m_cover);
}
