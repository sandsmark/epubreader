#include "widget.h"

#include "epubdocument.h"

#include <QFileDialog>
#include <QSettings>
#include <QDebug>
#include <QPainter>
#include <QKeyEvent>
#include <QAbstractTextDocumentLayout>
#include <QApplication>

Widget::Widget(QWidget *parent)
    : QDialog(parent),
      m_document(new EPubDocument(this)),
      m_currentChapter(0)
{
    //setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_QuitOnClose, true);
    setWindowFlags(Qt::Dialog);
    resize(600, 800);
    m_document->setParent(this);

    connect(m_document, &EPubDocument::loadCompleted, this, [&]() {
        update();
    });

    m_document->setPageSize(size());

    QSettings settings;
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open epub"), settings.value("lastFile").toString(), tr("EPUB files (*.epub)"));
    if (!fileName.isEmpty()) {
        settings.setValue("lastFile", fileName);
        m_document->openDocument(fileName);
    }
    show();
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
    painter.fillRect(rect(), Qt::white);
    if (!m_document->loaded()) {
        painter.drawText(rect(), Qt::AlignCenter, "Loading...");
        return;
    }

    QAbstractTextDocumentLayout::PaintContext paintContext;
    paintContext.clip = rect();
    paintContext.clip.translate(0, m_yOffset);

    paintContext.palette = palette();
    for (int group = 0; group < 3; ++group) {
        paintContext.palette.setColor(QPalette::ColorGroup(group), QPalette::WindowText, Qt::black);
        paintContext.palette.setColor(QPalette::ColorGroup(group), QPalette::Light, Qt::black);
        paintContext.palette.setColor(QPalette::ColorGroup(group), QPalette::Text, Qt::black);
        paintContext.palette.setColor(QPalette::ColorGroup(group), QPalette::Base, Qt::black);

        paintContext.palette.setColor(QPalette::ColorGroup(group), QPalette::Background, Qt::white);
        paintContext.palette.setColor(QPalette::ColorGroup(group), QPalette::Window, Qt::white);
        paintContext.palette.setColor(QPalette::ColorGroup(group), QPalette::Button, Qt::white);
    }

    painter.translate(0, -m_yOffset);
    painter.setClipRect(paintContext.clip);
    m_document->documentLayout()->draw(&painter, paintContext);
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
    } else if (event->key() == Qt::Key_End) {
        m_yOffset = m_document->size().height() - m_document->pageSize().height();
        update();
    } else if (event->key() == Qt::Key_Escape) {
        close();
    }
}

void Widget::resizeEvent(QResizeEvent *)
{
    m_document->clearCache();
    m_document->setPageSize(size());
    update();
}
void Widget::closeEvent(QCloseEvent *event)
{
    qDebug() << event;
    // for some reason quitonlastwindowclosed doesn't work
    //qApp->quit();
    QDialog::closeEvent(event);

    QWidgetList list = QApplication::topLevelWidgets();
    bool lastWindowClosed = true;
    for (int i = 0; i < list.size(); ++i) {
        QWidget *w = list.at(i);
        qDebug() << w << w->isVisible() << w->parentWidget() << w->testAttribute(Qt::WA_QuitOnClose);
        if (!w->isVisible() || w->parentWidget() || !w->testAttribute(Qt::WA_QuitOnClose))
            continue;
        lastWindowClosed = false;
        break;
    }
    qDebug() << "last closed?" << lastWindowClosed;
    event->accept();
}
