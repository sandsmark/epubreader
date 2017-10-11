#include "epubcontainer.h"

#include <KZip>
#include <KArchiveDirectory>
#include <KArchiveFile>

#include <QDebug>
#include <QScopedPointer>
#include <QDomDocument>
#include <QDir>
#include <QImage>
#include <QImageReader>

#define METADATA_FOLDER "META-INF"
#define MIMETYPE_FILE "mimetype"
#define CONTAINER_FILE "META-INF/container.xml"

EPubContainer::EPubContainer(QObject *parent) : QObject(parent),
    m_archive(nullptr),
    m_rootFolder(nullptr)
{
}

EPubContainer::~EPubContainer()
{
    delete m_archive;
}

bool EPubContainer::openFile(const QString path)
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

QSharedPointer<QIODevice> EPubContainer::getIoDevice(const QString &path)
{
    const KArchiveFile *file = getFile(path);
    if (!file) {
        emit errorHappened(tr("Unable to open file %1").arg(path.left(100)));
        return QSharedPointer<QIODevice>();
    }

    return QSharedPointer<QIODevice>(file->createDevice());
}

QImage EPubContainer::getImage(const QString &id)
{
    if (!m_items.contains(id)) {
        qWarning() << "Asked for unknown item" << id;
        return QImage();
    }

    const EpubItem &item = m_items.value(id);

    if (!QImageReader::supportedMimeTypes().contains(item.mimetype)) {
        qWarning() << "Asked for unsupported type" << item.mimetype;
        return QImage();
    }

    QSharedPointer<QIODevice> ioDevice = getIoDevice(item.path);

    if (!ioDevice) {
        return QImage();
    }

    return QImage::fromData(ioDevice->readAll());
}

QString EPubContainer::getMetadata(const QString &key)
{
    return m_metadata.value(key);
}

bool EPubContainer::parseMimetype()
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

bool EPubContainer::parseContainer()
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

    // The only thing we need from this file is the path to the root file
    QDomDocument document;
    document.setContent(ioDevice.data());
    QDomNodeList rootNodes = document.elementsByTagName("rootfile");
    for (int i=0; i<rootNodes.count(); i++) {
        QDomElement rootElement = rootNodes.at(i).toElement();
        QString rootfilePath = rootElement.attribute("full-path");
        if (rootfilePath.isEmpty()) {
            qWarning() << "Invalid root file entry";
            continue;
        }
        if (parseContentFile(rootfilePath)) {
            return true;
        }
    }

    // Limitations:
    //  - We only read one rootfile
    //  - We don't read the following from META-INF/
    //     - manifest.xml (unknown contents, just reserved)
    //     - metadata.xml (unused according to spec, just reserved)
    //     - rights.xml (reserved for DRM, not standardized)
    //     - signatures.xml (signatures for files, standardized)

    emit errorHappened(tr("Unable to find and use any content files"));
    return false;
}

bool EPubContainer::parseContentFile(const QString filepath)
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
    QString contentFileFolder;
    int separatorIndex = filepath.lastIndexOf('/');
    if (separatorIndex > 0) {
        contentFileFolder = filepath.left(separatorIndex + 1);
    }

    // Parse out all the components/items in the epub
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

        QString tocId = spineElement.attribute("toc");
        if (!tocId.isEmpty() && m_items.keys().contains(tocId)) {
            EpubPageReference tocReference;
            tocReference.title = tr("Table of Contents");
            tocReference.target = tocId;
            m_standardReferences.insert(EpubPageReference::TableOfContents, tocReference);
        }

        QDomNodeList spineItemList = spineElement.elementsByTagName("itemref");
        for (int j=0; j<spineItemList.count(); j++) {
            parseSpineItem(spineItemList.at(j));
        }
    }

    // Parse out standard items
    QDomNodeList guideNodeList = document.elementsByTagName("guide");
    for (int i=0; i<guideNodeList.count(); i++) {
        QDomElement guideElement = guideNodeList.at(i).toElement();

        QDomNodeList guideItemList = guideElement.elementsByTagName("reference");
        for (int j=0; j<guideItemList.count(); j++) {
            parseGuideItem(guideItemList.at(j));
        }
    }

    return true;
}

bool EPubContainer::parseMetadataItem(const QDomNode &metadataNode)
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

bool EPubContainer::parseManifestItem(const QDomNode &manifestNode, const QString currentFolder)
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
    path = QDir::cleanPath(currentFolder + path);

    EpubItem item;
    item.mimetype  = type.toUtf8();
    item.path = path;
    m_items[id] = item;

    static QSet<QString> documentTypes({"text/x-oeb1-document", "application/x-dtbook+xml", "application/xhtml+xml"});
    // All items not listed in the spine should be in this
    if (documentTypes.contains(type)) {
        m_unorderedItems.insert(id);
    }

    return true;
}

bool EPubContainer::parseSpineItem(const QDomNode &spineNode)
{
    QDomElement spineElement = spineNode.toElement();

    // Ignore this for now
    if (spineElement.attribute("linear") == "no") {
//        return true;
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

bool EPubContainer::parseGuideItem(const QDomNode &guideItem)
{
    QDomElement guideElement = guideItem.toElement();
    QString target = guideElement.attribute("href");
    QString title = guideElement.attribute("title");
    QString type = guideElement.attribute("type");

    if (target.isEmpty() || title.isEmpty() || type.isEmpty()) {
        qWarning() << "Invalid guide item" << target << title << type;
        return false;
    }

    EpubPageReference reference;
    reference.target = target;
    reference.title = title;

    EpubPageReference::StandardType standardType = EpubPageReference::typeFromString(type);
    if (standardType == EpubPageReference::Other) {
        m_otherReferences[type] = reference;
    } else {
        m_standardReferences[standardType] = reference;
    }

    return true;
}

const KArchiveFile *EPubContainer::getFile(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
    }

    const KArchiveDirectory *folder = m_rootFolder;

    // Try to walk down the correct path
    QStringList pathParts = path.split('/', QString::SkipEmptyParts);
    for (int i=0; i<pathParts.count() - 1; i++) {
        QString folderName = pathParts[i];
        const KArchiveEntry *entry = folder->entry(folderName);
        if (!entry) {
            qWarning() << "Unable to find folder name" << folderName << "in" << path.left(100);
            const QStringList entries = folder->entries();
            for (const QString &folderEntry : entries) {
                if (folderEntry.compare(folderName, Qt::CaseInsensitive) == 0) {
                    entry = folder->entry(folderEntry);
                    break;
                }
            }

            if (!entry) {
                qWarning() << "Didn't even find with case-insensitive matching";
                return nullptr;
            }
        }

        if (!entry->isDirectory()) {
            qWarning() << "Expected" << folderName << "to be a directory in path" << path;
            return nullptr;
        }

        folder = dynamic_cast<const KArchiveDirectory*>(entry);
        Q_ASSERT(folder);
    }

    QString filename;
    if (pathParts.isEmpty()) {
        filename = path;
    } else {
        filename = pathParts.last();
    }

    const KArchiveFile *file = folder->file(filename);
    if (!file) {
        qWarning() << "Unable to find file" << filename << "in" << folder->name();

        const QStringList entries = folder->entries();
        for (const QString &folderEntry : entries) {
            if (folderEntry.compare(filename, Qt::CaseInsensitive) == 0) {
                file = folder->file(folderEntry);
                break;
            }
        }

        if (!file) {
            qWarning() << "Unable to find file" << filename << "in" << folder->name() << "with case-insensitive matching" << entries;
        }

    }
    return file;
}

EpubPageReference::StandardType EpubPageReference::typeFromString(const QString &name) {
    if (name == "cover") {
        return CoverPage;
    } else if (name == "title-page") {
        return TitlePage;
    } else if (name == "toc") {
        return TableOfContents;
    } else if (name == "index") {
        return Index;
    } else if (name == "glossary") {
        return Glossary;
    } else if (name == "acknowledgements") {
        return Acknowledgements;
    } else if (name == "bibliography") {
        return Bibliography;
    } else if (name == "colophon") {
        return Colophon;
    } else if (name == "copyright-page") {
        return CopyrightPage;
    } else if (name == "dedication") {
        return Dedication;
    } else if (name == "epigraph") {
        return Epigraph;
    } else if (name == "foreword") {
        return Foreword;
    } else if (name == "loi") {
        return ListOfIllustrations;
    } else if (name == "lot") {
        return ListOfTables;
    } else if (name == "notes") {
        return Notes;
    } else if (name == "preface") {
        return Preface;
    } else if (name == "text") {
        return Text;
    } else {
        return Other;
    }
}
