#include "epubdocument.h"
#include "epubcontainer.h"
#include <QIODevice>
#include <QDebug>
#include <QDir>
#include <QTextCursor>
#include <QThread>
#include <QElapsedTimer>

EPubDocument::EPubDocument() : QTextDocument(),
    m_container(nullptr),
    m_loaded(false)
{
    setUndoRedoEnabled(false);
}

EPubDocument::~EPubDocument()
{

}

void EPubDocument::openDocument(const QString &path)
{
    m_documentPath = path;
    loadDocument();
}

void EPubDocument::loadDocument()
{
    QElapsedTimer timer;
    timer.start();
    m_container = new EPubContainer(this);
    connect(m_container, &EPubContainer::errorHappened, [](QString error) {
        qWarning().noquote() << error;
    });

    if (!m_container->openFile(m_documentPath)) {
        return;
    }
    QTextCursor cursor(this);
    cursor.movePosition(QTextCursor::End);
    qDebug() << m_container->getItems().count();

    QStringList items = m_container->getItems();

    QString cover = m_container->getStandardPage(EpubPageReference::CoverPage);
    if (!cover.isEmpty()) {
        items.prepend(cover);
        qDebug() << cover;
    }

    for (const QString &chapter : items) {
        m_currentItem = m_container->getEpubItem(chapter);
        if (m_currentItem.path.isEmpty()) {
            continue;
        }

        QSharedPointer<QIODevice> ioDevice = m_container->getIoDevice(m_currentItem.path);
        if (!ioDevice) {
            qWarning() << "Unable to get iodevice for chapter" << chapter;
            continue;
        }
        cursor.insertHtml(ioDevice->readAll());

        QTextBlockFormat pageBreak;
        pageBreak.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
        cursor.insertBlock(pageBreak);
    }
    qDebug() << blockCount();
    m_loaded = true;

    emit loadCompleted();
    qDebug() << "Done in" << timer.elapsed() << "ms";
}

QVariant EPubDocument::loadResource(int type, const QUrl &name)
{
    Q_UNUSED(type);

    QString path = name.path();

    QString contentFileFolder;
    int separatorIndex = m_currentItem.path.lastIndexOf('/');
    if (separatorIndex > 0) {
        contentFileFolder = m_currentItem.path.left(separatorIndex + 1);
    }

    path = QDir::cleanPath(contentFileFolder + path);

    QSharedPointer<QIODevice> ioDevice = m_container->getIoDevice(path);
    if (!ioDevice) {
        qWarning() << "Unable to get io device for" << path;
        return QVariant();
    }
    QByteArray data = ioDevice->readAll();
    return data;
}

