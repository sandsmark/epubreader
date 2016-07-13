#include "epubparser.h"
#include <KZip>
#include <KArchiveDirectory>
#include <KArchiveFile>

#include <QDebug>
#include <QScopedPointer>
#include <QXmlStreamReader>
#include <QDomDocument>
#include <QDir>

#define METADATA_FOLDER "META-INF"
#define MIMETYPE_FILE "mimetype"
#define CONTAINER_FILE "META-INF/container.xml"

EPubParser::EPubParser(QObject *parent) : QObject(parent),
    m_archive(nullptr),
    m_rootFolder(nullptr)
{

}

EPubParser::~EPubParser()
{
    delete m_archive;
}



bool EPubParser::loadFile(const QString path)
{
    delete m_archive;

    m_archive = new KZip(path);

    if (!m_archive->open(QIODevice::ReadOnly)) {
        emit errorHappened(tr("Failed to open %1").arg(path));

        return false;
    }

    m_rootFolder = m_archive->directory();
    if (!m_rootFolder) {
        emit errorHappened(tr("Failed to read %1").arg(path));
        return false;
    }

    if (!parseMimetype()) {
        return false;
    }

    if (!parseContainer()) {
        return false;
    }

    return true;
}

bool EPubParser::parseMimetype()
{
    Q_ASSERT(m_rootFolder);

    const KArchiveFile *mimetypeFile = m_rootFolder->file(MIMETYPE_FILE);

    if (!mimetypeFile) {
        emit errorHappened(tr("Unable to find mimetype in file"));
        return false;
    }

    QScopedPointer<QIODevice> ioDevice(mimetypeFile->createDevice());
    QByteArray mimetype = ioDevice->readAll();
    if (mimetype != "application/epub+zip") {
        qWarning() << "Unexpected mimetype" << mimetype;
    }

    return true;
}

bool EPubParser::parseContainer()
{
    Q_ASSERT(m_rootFolder);

    const KArchiveFile *containerFile = getFile(CONTAINER_FILE);
    if (!containerFile) {
        qWarning() << "no container file";
        emit errorHappened(tr("Unable to find container information"));
        return false;
    }

    QScopedPointer<QIODevice> ioDevice(containerFile->createDevice());
    Q_ASSERT(ioDevice);

    QXmlStreamReader xmlReader(ioDevice.data());

    QString rootfilePath;
    while (!xmlReader.atEnd()) {
        if (!xmlReader.readNextStartElement()) {
            continue;
        }

        if (xmlReader.name() == QStringLiteral("rootfile")) {
            rootfilePath = xmlReader.attributes().value("full-path").toString();
            break;
        }
    }

    if (xmlReader.hasError()) {
        emit errorHappened(tr("Error while parsing container: %1").arg(xmlReader.errorString()));
        return false;
    }

    if (rootfilePath.isEmpty()) {
        emit errorHappened(tr("Unable to find root file in container"));
    }

    return parseContentFile(rootfilePath);
}

bool EPubParser::parseContentFile(const QString filepath)
{
    const KArchiveFile *rootFile = getFile(filepath);
    if (!rootFile) {
        emit errorHappened(tr("Malformed metadata, unable to get content metadata path"));
        return false;
    }
    QScopedPointer<QIODevice> ioDevice(rootFile->createDevice());
    QDomDocument document;
    document.setContent(ioDevice.data(), true); // turn on namespace processing

    QDomNodeList metadataNodeList = document.elementsByTagName("metadata");
    for (int i=0; i<metadataNodeList.count(); i++) {
        QDomNodeList metadataChildList = metadataNodeList.at(i).childNodes();
        for (int j=0; j<metadataChildList.count(); j++) {
            parseMetadataItem(metadataChildList.at(j));
        }
    }

    // Extract current path, for resolving relative paths
    QString contentFileFolder = filepath;
    int separatorIndex = filepath.lastIndexOf('/');
    if (separatorIndex > 0) {
        contentFileFolder = contentFileFolder.left(separatorIndex);
    }

    QDomNodeList manifestNodeList = document.elementsByTagName("manifest");
    for (int i=0; i<manifestNodeList.count(); i++) {
        QDomElement manifestElement = manifestNodeList.at(i).toElement();
        QDomNodeList manifestItemList = manifestElement.elementsByTagName("item");

        for (int j=0; j<manifestItemList.count(); j++) {
            parseManifestItem(manifestItemList.at(j), contentFileFolder);
        }
    }

    // Parse out the document order
    QDomNodeList spineNodeList = document.elementsByTagName("spine");
    for (int i=0; i<spineNodeList.count(); i++) {
        QDomElement spineElement = spineNodeList.at(i).toElement();
        m_indexItem = spineElement.attribute("toc");

        QDomNodeList spineItemList = spineElement.elementsByTagName("itemref");
        for (int j=0; j<spineItemList.count(); j++) {
            parseSpineItem(spineItemList.at(j));
        }
    }

    return true;
}

bool EPubParser::parseMetadataItem(const QDomNode &metadataNode)
{
    QDomElement metadataElement = metadataNode.toElement();
    QString tagName = metadataElement.tagName();

    QString metaName;
    QString metaValue;

    if (tagName == "meta") {
        metaName = metadataElement.attribute("name");
        metaValue = metadataElement.attribute("content");
    } else if (metadataElement.prefix() != "dc") {
        qWarning() << "Unsupported metadata tag" << tagName;
        return false;
    } else if (tagName == "date") {
        metaName = metadataElement.attribute("event");
        metaValue = metadataElement.text();
    } else {
        metaName = tagName;
        metaValue = metadataElement.text();
    }

    if (metaName.isEmpty() || metaValue.isEmpty()) {
        return false;
    }
    m_metadata[metaName] = metaValue;

    return true;
}

bool EPubParser::parseManifestItem(const QDomNode &manifestNode, const QString currentFolder)
{
    QDomElement manifestElement = manifestNode.toElement();
    QString id = manifestElement.attribute("id");
    QString path = manifestElement.attribute("href");
    QString type = manifestElement.attribute("media-type");

    if (id.isEmpty() || path.isEmpty()) {
        qWarning() << "Invalid item at line" << manifestElement.lineNumber();
        return false;
    }

    // Resolve relative paths
    path = QDir::cleanPath(currentFolder + '/' + path);

    EpubItem item;
    item.mimetype  = type;
    item.path = path;
    m_items[id] = item;

    static QSet<QString> documentTypes({"text/x-oeb1-document", "application/x-dtbook+xml", "application/xhtml+xml"});
    // All items not listed in the spine should be in this
    if (documentTypes.contains(type)) {
        m_unorderedItems.insert(id);
    }

    return true;
}

bool EPubParser::parseSpineItem(const QDomNode &spineNode)
{
    QDomElement spineElement = spineNode.toElement();

    if (spineElement.attribute("linear") == "no") {
        return true;
    }

    QString referenceName = spineElement.attribute("idref");
    if (referenceName.isEmpty()) {
        qWarning() << "Invalid spine item at line" << spineNode.lineNumber();
        return false;
    }

    if (!m_items.keys().contains(referenceName)) {
        qWarning() << "Unable to find" << referenceName << "in items";
        return false;
    }

    m_unorderedItems.remove(referenceName);
    m_orderedItems.append(referenceName);

    return true;
}

const KArchiveFile *EPubParser::getFile(const QString &path)
{
    QStringList pathParts = path.split('/');
    const KArchiveDirectory *folder = m_rootFolder;
    for (int i=0; i<pathParts.count() - 1; i++) {
        QString folderName = pathParts[i];
        qDebug() << "Entering folder" << folderName;
        const KArchiveEntry *entry = folder->entry(folderName);
        if (!entry->isDirectory()) {
            qWarning() << "Expected" << folderName << "to be a directory";
            return nullptr;
        }

        folder = dynamic_cast<const KArchiveDirectory*>(entry);
        Q_ASSERT(folder);
    }

    QString filename = pathParts.last();
    const KArchiveFile *file = folder->file(filename);
    if (!file) {
        qWarning() << "Unable to find file" << filename << "in" << folder->name();
    }
    return file;
}
