#include "widget.h"

#include "epubcontainer.h"
#include "epubdocument.h"
#include <QDebug>
#include <QPainter>
#include <QKeyEvent>

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      m_parser(new EPubContainer(this)),
      m_document(new EPubDocument(m_parser, this)),
      m_currentChapter(0)
{
    connect(m_parser, &EPubContainer::errorHappened, [](QString error) {
        qWarning().noquote() << error;
    });
    setWindowFlags(Qt::Dialog);
    resize(600, 800);
    if (!m_parser->openFile("Lorem Ipsum.epub")) {
        return;
    }

    QString coverId = m_parser->getMetadata("cover");
    if (!coverId.isEmpty()) {
        m_cover = m_parser->getImage(coverId);
    }
    QStringList chapters = m_parser->getItems();
    if (chapters.count() > 0) {
        m_document->setChapter(chapters.first());
    }
    m_document->setPageSize(size());
}

Widget::~Widget()
{

}

void Widget::setChapter(int chapter)
{
    QStringList chapters = m_parser->getItems();
    if (chapter < 0 || chapter >= chapters.count()) {
        return;
    }
    m_document->setChapter(chapters[chapter]);
    m_currentChapter = chapter;
    m_yOffset = 0;
    update();
}

void Widget::scroll(int amount)
{
    int offset = m_yOffset + amount;
    offset = qMin(int(m_document->size().height() - m_document->pageSize().height()), offset);
    m_yOffset = qMax(0, offset);
    update();
}

void Widget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.translate(0, -m_yOffset);
    QRect r(rect());
    r.translate(0, m_yOffset);

    m_document->drawContents(&painter, r);
}

void Widget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Right) {
        setChapter(m_currentChapter + 1);
    } else if (event->key() == Qt::Key_Left) {
        setChapter(m_currentChapter - 1);
    } else if (event->key() == Qt::Key_Up) {
        scroll(-20);
    } else if (event->key() == Qt::Key_Down) {
        scroll(20);
    } else if (event->key() == Qt::Key_PageUp) {
        scroll(-m_document->pageSize().height());
    } else if (event->key() == Qt::Key_PageDown) {
        scroll(m_document->pageSize().height());
    }
}
