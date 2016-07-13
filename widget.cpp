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
    QStringList chapters = m_parser->getItems();
    if (chapters.count() > 0) {
        m_document->setChapter(chapters.first());
    }
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
    update();
}

void Widget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    m_document->drawContents(&painter, rect());
}

void Widget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Right) {
        setChapter(m_currentChapter + 1);
    } else if (event->key() == Qt::Key_Left) {
        setChapter(m_currentChapter - 1);
    }
}
