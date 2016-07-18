#include "widget.h"

#include "epubdocument.h"
#include <QDebug>
#include <QPainter>
#include <QKeyEvent>

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      m_document(new EPubDocument()),
      m_currentChapter(0)
{
    setWindowFlags(Qt::Dialog);
    resize(600, 800);

    connect(m_document, &EPubDocument::loadCompleted, [&]() {
        update();
    });

    m_document->setPageSize(size());
    m_document->openDocument("test.epub");
}

Widget::~Widget()
{

}

void Widget::scroll(int amount)
{
    int offset = m_yOffset + amount;
    offset = qMin(int(m_document->size().height() - m_document->pageSize().height()), offset);
    m_yOffset = qMax(0, offset);
    update();
}

void Widget::scrollPage(int amount)
{
    int currentPage = m_yOffset / m_document->pageSize().height();
    currentPage += amount;
    int offset = currentPage * m_document->pageSize().height();
    offset = qMin(int(m_document->size().height() - m_document->pageSize().height()), offset);
    m_yOffset = qMax(0, offset);
    update();
}

void Widget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    if (!m_document->loaded()) {
        painter.fillRect(rect(), Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "Loading...");
        return;
    }

    painter.translate(0, -m_yOffset);
    QRect r(rect());
    r.translate(0, m_yOffset);

    m_document->drawContents(&painter, r);
}

void Widget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Up) {
        scroll(-20);
    } else if (event->key() == Qt::Key_Down) {
        scroll(20);
    } else if (event->key() == Qt::Key_PageUp) {
        scrollPage(-1);
    } else if (event->key() == Qt::Key_PageDown) {
        scrollPage(1);
    }
}

void Widget::resizeEvent(QResizeEvent *)
{
    m_document->clearCache();
    m_document->setPageSize(size());
    update();
}
