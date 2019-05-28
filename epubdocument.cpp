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
#include <QRegularExpression>
#include <QFontDatabase>
#include <QTextDocumentFragment>

#ifdef DEBUG_CSS
#include <private/qcssparser_p.h>
#endif

EPubDocument::EPubDocument(QObject *parent) : QTextDocument(parent),
    m_container(nullptr),
    m_loaded(false)
{
    setUndoRedoEnabled(false);
}

EPubDocument::~EPubDocument()
{
    for (const int fontId : m_loadedFonts) {
        QFontDatabase::removeApplicationFont(fontId);
    }
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
    connect(m_container, &EPubContainer::errorHappened, this, [](QString error) {
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

    QDomDocument domDoc;
    QTextCursor textCursor(this);
    textCursor.movePosition(QTextCursor::End);

    QTextBlockFormat pageBreak;
    pageBreak.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
    //for (const QString &chapter : items) {
    while(!items.isEmpty()) {
        const QString &chapter = items.takeFirst();
        m_currentItem = m_container->getEpubItem(chapter);
        if (m_currentItem.path.isEmpty()) {
            continue;
        }

        QSharedPointer<QIODevice> ioDevice = m_container->getIoDevice(m_currentItem.path);
        if (!ioDevice) {
            qWarning() << "Unable to get iodevice for chapter" << chapter;
            continue;
        }

        domDoc.setContent(ioDevice.data());
        setBaseUrl(QUrl(m_currentItem.path));
        fixImages(domDoc);
        textCursor.insertFragment(QTextDocumentFragment::fromHtml(domDoc.toString()));
        textCursor.insertBlock(pageBreak);
    }
    qDebug() << "Base url:" << baseUrl();
    setBaseUrl(QUrl());
    m_loaded = true;

    emit loadCompleted();
    qDebug() << "Done in" << timer.elapsed() << "ms";
    adjustSize();
}

void EPubDocument::fixImages(QDomDocument &newDocument)
{
    // TODO: FIXME: replace this with not smushing all HTML together in one document
    { // Fix relative URLs, images are lazily loaded so the base URL might not
      // be correct when they are loaded
        QDomNodeList imageNodes = newDocument.elementsByTagName("img");
        for (int i=0; i<imageNodes.count(); i++) {
            QDomElement image = imageNodes.at(i).toElement();
            if (!image.hasAttribute("src")) {
                continue;
            }
            QUrl href = QUrl(image.attribute("src"));
            href = baseUrl().resolved(href);
            image.setAttribute("src", href.toString());
        }
    }

    { // QImage which QtSvg uses isn't able to read files from inside the archive, so embed image data inline
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
    }

    static int svgCounter = 0;

    // QTextDocument isn't fond of SVGs, so rip them out and store them separately, and give it <img> instead
    QDomNodeList svgNodes = newDocument.elementsByTagName("svg");
    for (int i=0; i<svgNodes.count(); i++) {
        QDomElement svgNode = svgNodes.at(i).toElement();

        // Serialize out the old SVG, store it
        QDomDocument tempDocument;
        tempDocument.appendChild(tempDocument.importNode(svgNode, true));
        QString svgId = QString::number(++svgCounter);
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
        if (svgSize.scaled(imageSize, Qt::KeepAspectRatio).isValid()) {
            svgSize.scale(imageSize, Qt::KeepAspectRatio);
        }
    } else {
        svgSize = imageSize;
    }

    QImage rendered(svgSize, QImage::Format_ARGB32);
    QPainter painter(&rendered);
    if (!painter.isActive()) {
        qWarning() << "Unable to activate painter" << svgSize;
        static const QImage dummy = QImage();
        return dummy;
    }
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

    if (url.scheme() == "data") {
        QByteArray data = url.path().toUtf8();
        const int start = data.indexOf(';');
        if (start == -1) {
            qWarning() << "unable to decode data:, no ;" << data.left(100);
            data = QByteArray();

            addResource(type, url, data);
            return data;
        }

        data = data.mid(start + 1);
        if (data.startsWith("base64,")) {
            data = QByteArray::fromBase64(data.mid(data.indexOf(',') + 1));
        } else {
            qWarning() << "unable to decode data:, unknown encoding" << data.left(100);
            data = QByteArray();
        }

        addResource(type, url, data);
        return data;
    }


    QSharedPointer<QIODevice> ioDevice = m_container->getIoDevice(url.path());
    if (!ioDevice) {
        qWarning() << "Unable to get io device for" << url.toString().left(100);
        qDebug() << url.scheme();
        return QVariant();
    }
    QByteArray data = ioDevice->readAll();

    if (type == QTextDocument::StyleSheetResource) {
        const QString cssData = QString::fromUtf8(data);

        // Extract embedded fonts
        static const QRegularExpression fontfaceRegex("@font-face\\s*{[^}]+}", QRegularExpression::MultilineOption);
        QRegularExpressionMatchIterator fontfaceIterator = fontfaceRegex.globalMatch(cssData);
        while (fontfaceIterator.hasNext()) {
            QString fontface = fontfaceIterator.next().captured();

            static const QRegularExpression urlExpression("url\\s*\\(([^\\)]+)\\)");
            QString fontPath = urlExpression.match(fontface).captured(1);
            // Resolve relative and whatnot shit
            fontPath = QDir::cleanPath(QFileInfo(baseUrl().path()).path() + '/' + fontPath);

            QSharedPointer<QIODevice> ioDevice = m_container->getIoDevice(fontPath);
            if (ioDevice) {
                m_loadedFonts.append(QFontDatabase::addApplicationFontFromData(ioDevice->readAll()));
                qDebug() << "Loaded font" << QFontDatabase::applicationFontFamilies(m_loadedFonts.last());
            } else {
                qWarning() << "Failed to load font from" << fontPath << baseUrl();
            }
        }

        data = cssData.toUtf8();


//#ifdef DEBUG_CSS
        QCss::Parser parser(cssData);
        QCss::StyleSheet stylesheet;
        qDebug() << "=====================";
        qDebug() << "Parse success?" << parser.parse(&stylesheet);
        qDebug().noquote() << parser.errorIndex << parser.errorSymbol().lexem();
//#endif
    }

    addResource(type, url, data);

    return data;
}

