#include "epubparser.h"
#include <KZip>
#include <KArchiveDirectory>
#include <KArchiveFile>

#include <QDebug>
#include <QScopedPointer>
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

    QDomDocument doc;
    doc.setContent(ioDevice.data());

    QDomNodeList nodes = doc.elementsByTagName("rootfile");
    if (nodes.count() != 1){
        emit errorHappened(tr("Malformed metadata, unable to find content metadata"));
        return false;
    }

    QDomNode pathAttribute = nodes.at(0).attributes().namedItem("full-path");
    QString rootfilePath = QDir::cleanPath(pathAttribute.nodeValue());
    if (rootfilePath.isEmpty()) {
        emit errorHappened(tr("Malformed metadata, unable to get content metadata path"));
        return false;
    }


    const KArchiveFile *rootFile = getFile(rootfilePath);
    if (!rootFile) {
        emit errorHappened(tr("Malformed metadata, unable to get content metadata path"));
        return false;
    }
    QScopedPointer<QIODevice> rootDevice(rootFile->createDevice());


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
