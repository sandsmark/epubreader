#include "epubdocument.h"
#include "epubcontainer.h"
#include <QIODevice>
#include <QDebug>
#include <QDir>

EPubDocument::EPubDocument(EPubContainer *container, QObject *parent) : QTextDocument(parent),
    m_container(container)
{
    setUndoRedoEnabled(false);
}

void EPubDocument::setChapter(const QString &id)
{
    m_currentItem = m_container->getEpubItem(id);
    if (m_currentItem.path.isEmpty()) {
        return;
    }

    QSharedPointer<QIODevice> ioDevice = m_container->getIoDevice(m_currentItem.path);
    if (!ioDevice) {
        qWarning() << "Unable to get iodevice for chapter" << id;
        return;
    }

    setHtml(QString::fromUtf8(ioDevice->readAll()));
}

QVariant EPubDocument::loadResource(int type, const QUrl &name)
{
    QString path = name.path();

    QString contentFileFolder = m_currentItem.path;
    int separatorIndex = contentFileFolder.lastIndexOf('/');
    if (separatorIndex > 0) {
        contentFileFolder = contentFileFolder.left(separatorIndex);
    }

    path = QDir::cleanPath(contentFileFolder + '/' + path);
    QSharedPointer<QIODevice> ioDevice = m_container->getIoDevice(path);
    if (!ioDevice) {
        qWarning() << "Unable to get io device for" << path;
        return QVariant();
    }
    return ioDevice->readAll();

//    qDebug()  << "normalized" << m_container->normalizePathForItem(m_currentItem, name.toString());
//    qDebug() << type << name.path() << name.toString();
//    return QVariant();
}
