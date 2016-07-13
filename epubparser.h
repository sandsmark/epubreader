#ifndef EPUBPARSER_H
#define EPUBPARSER_H

#include <QObject>
#include <QMap>
#include <QDomNode>

class KZip;
class KArchiveDirectory;
class KArchiveFile;
class QXmlStreamReader;

class EPubParser : public QObject
{
    Q_OBJECT
public:
    explicit EPubParser(QObject *parent = 0);
    ~EPubParser();

    bool loadFile(const QString path);

signals:
    void errorHappened(const QString &error);

public slots:

private:
    bool parseMimetype();
    bool parseContainer();
    bool parseContentFile(const QString filepath);
    bool parseMetadata(const QDomNode &metadataNode);

    const KArchiveFile *getFile(const QString &path);

    KZip *m_archive;
    const KArchiveDirectory *m_rootFolder;
    QMap<QString, QString> m_metadata;
};

#endif // EPUBPARSER_H
