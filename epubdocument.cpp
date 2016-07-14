#include "epubdocument.h"
#include "epubcontainer.h"
#include <QIODevice>
#include <QDebug>
#include <QDir>
#include <QTextCursor>
#include <QThread>
#include <QElapsedTimer>
#include <QDomDocument>

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

    QStringList items = m_container->getItems();

    QString cover = m_container->getStandardPage(EpubPageReference::CoverPage);
    if (!cover.isEmpty()) {
        items.prepend(cover);
        qDebug() << cover;
    }

    QDomDocument newDocument;
    bool isFirst = true;

    QDomNode bodyElement;
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

        QByteArray data = ioDevice->readAll();

        if (isFirst) {
            newDocument.setContent(data);
            isFirst = false;
            QDomNodeList bodies = newDocument.elementsByTagName("body");
            if (bodies.count() != 1) {
                qWarning() << "Invalid number of bodies";
                return;
            }
            bodyElement = bodies.at(0);
            continue;
        }

        QDomDocument oldDocument;
        oldDocument.setContent(data);
        QDomNodeList bodies = oldDocument.elementsByTagName("body");
        if (bodies.count() != 1) {
            qWarning() << "Invalid number of bodies in old";
            continue;
        }
        QDomNode newBodyNode = newDocument.importNode(bodies.at(0), true);
        QDomNodeList contents = newBodyNode.childNodes();
        for (int i=0; i<contents.count(); i++) {
            bodyElement.appendChild(contents.at(i));
        }
    }

    fixImages(newDocument);

    setHtml(newDocument.toString(-1));

    qDebug() << blockCount();
    m_loaded = true;

    emit loadCompleted();
    qDebug() << "Done in" << timer.elapsed() << "ms";
}

void EPubDocument::fixImages(QDomDocument &newDocument)
{
    // QImage which QtSvg uses isn't able to read files from inside the archive, so embed image data inline
    QDomNodeList imageNodes = newDocument.elementsByTagName("image"); // SVG images
    for (int i=0; i<imageNodes.count(); i++) {
        QDomElement image = imageNodes.at(i).toElement();
        if (!image.hasAttribute("xlink:href")) {
            continue;
        }
        QString path = image.attribute("xlink:href");
        QByteArray fileData = loadResource(0, QUrl(path)).toByteArray();
        QByteArray data = "data:image/jpeg;base64," +fileData.toBase64();
        image.setAttribute("xlink:href", QString::fromLatin1(data));
    }

    int svgCounter = 0;

    // QTextDocument isn't fond of SVGs, so rip them out and store them separately, and give it <img> instead
    QDomNodeList svgNodes = newDocument.elementsByTagName("svg");
    for (int i=0; i<svgNodes.count(); i++) {
        QDomElement svgNode = svgNodes.at(i).toElement();

        // Serialize out the old SVG, store it
        QDomDocument tempDocument;
        tempDocument.appendChild(tempDocument.importNode(svgNode, true));
        QString svgPlaceholderPath = "__SVG_PLACEHOLDER_" + QString::number(++svgCounter) + ".svg";
        m_svgs.insert(svgPlaceholderPath, tempDocument.toByteArray());

        // For now these are the only properties we keep
        QString width = svgNode.attribute("width");
        QString height = svgNode.attribute("height");

        // We can't handle percentages
        if (width.endsWith("%")) {
            width.chop(1);
            qDebug() << width;
            width = QString::number(pageSize().width() * width.toInt() / 100);
            qDebug() << width;
        }
        if (height.endsWith("%")) {
            height.chop(1);
            qDebug() << height;
            height = QString::number(pageSize().height() * height.toInt() / 100);
            qDebug() << height;
        }

        // Create <img> node pointing to our SVG image
        QDomElement imageElement = newDocument.createElement("img");
        imageElement.setAttribute("width", width);
        imageElement.setAttribute("height", height);
        imageElement.setAttribute("src", svgPlaceholderPath);

        // Replace <svg> node with our <img> node
        QDomNode parent = svgNodes.at(i).parentNode();
        parent.replaceChild(imageElement, svgNode);
    }
}

QVariant EPubDocument::loadResource(int type, const QUrl &name)
{
    Q_UNUSED(type);

    QString path = name.path();
    if (m_svgs.contains(path)) {
        return m_svgs.value(path);
    }

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

