#include "epubdocument.h"
#include "epubcontainer.h"
#include <QIODevice>
#include <QDebug>
#include <QDir>
#include <QTextCursor>
#include <QThread>
#include <QElapsedTimer>
#include <QDomDocument>
#include <QSvgRenderer>
#include <QPainter>
#include <QTextBlock>

#ifdef DEBUG_CSS
#include <private/qcssparser_p.h>
#endif

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

    QTextBlockFormat pageBreak;
    pageBreak.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
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
        if (data.isEmpty()) {
            qWarning() << "Got an empty document";
            continue;
        }
        setBaseUrl(QUrl(m_currentItem.path));
        QDomDocument newDocument;
        newDocument.setContent(data);
        fixImages(newDocument);
        cursor.insertHtml(newDocument.toString());

        cursor.insertBlock(pageBreak);
    }
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

    static int svgCounter = 0;

    // QTextDocument isn't fond of SVGs, so rip them out and store them separately, and give it <img> instead
    QDomNodeList svgNodes = newDocument.elementsByTagName("svg");
    for (int i=0; i<svgNodes.count(); i++) {
        QDomElement svgNode = svgNodes.at(i).toElement();

        // Serialize out the old SVG, store it
        QDomDocument tempDocument;
        tempDocument.appendChild(tempDocument.importNode(svgNode, true));
        QString svgId = QString::number(++svgCounter);// + ".svg";
        m_svgs.insert(svgId, tempDocument.toByteArray());

        // Create <img> node pointing to our SVG image
        QDomElement imageElement = newDocument.createElement("img");
        imageElement.setAttribute("src", "svgcache:" + svgId);

        // Replace <svg> node with our <img> node
        QDomNode parent = svgNodes.at(i).parentNode();
        parent.replaceChild(imageElement, svgNode);
    }
}

const QImage &EPubDocument::getSvgImage(const QString &id)
{
    if (m_renderedSvgs.contains(id)) {
        return m_renderedSvgs[id];
    }
    if (!m_svgs.contains(id)) {
        qWarning() << "Couldn't find SVG" << id;
        static QImage nullImg;
        return nullImg;
    }

    QSize imageSize(pageSize().width() - documentMargin() * 4,
                    pageSize().height() - documentMargin() * 4);


    QSvgRenderer renderer(m_svgs.value(id));
    QSize svgSize(renderer.viewBox().size());

    if (svgSize.isValid()) {
        svgSize.scale(imageSize, Qt::KeepAspectRatio);
    } else {
        svgSize = imageSize;
    }

    QImage rendered(svgSize, QImage::Format_ARGB32);
    QPainter painter(&rendered);
    renderer.render(&painter);
    painter.end();

    m_renderedSvgs.insert(id, rendered);
    return m_renderedSvgs[id];
}

QVariant EPubDocument::loadResource(int type, const QUrl &url)
{
    Q_UNUSED(type);

    if (url.scheme() == "svgcache") {
        return getSvgImage(url.path());
    }

    QSharedPointer<QIODevice> ioDevice = m_container->getIoDevice(url.path());
    if (!ioDevice) {
        qWarning() << "Unable to get io device for" << url;
        return QVariant();
    }
    QByteArray data = ioDevice->readAll();

    if (type == QTextDocument::StyleSheetResource) {
        QString cssData = QString::fromLocal8Bit(data);
        cssData.replace("@charset \"", "@charset\"");
        data = cssData.toLocal8Bit();

#ifdef DEBUG_CSS
//        QCss::Parser parser(cssData);
//        QCss::StyleSheet stylesheet;
//        qDebug() << "Parse success?" << parser.parse(&stylesheet);
//        qDebug().noquote() << parser.errorIndex << parser.errorSymbol().lexem();
#endif

    }

    addResource(type, url, data);

    return data;
}

