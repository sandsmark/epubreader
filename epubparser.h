#ifndef EPUBPARSER_H
#define EPUBPARSER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QVector>
#include <QDomNode>
#include <QMimeDatabase>

class KZip;
class KArchiveDirectory;
class KArchiveFile;
class QXmlStreamReader;

struct EpubItem {
    QString path;
    QString mimetype;
};

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
    bool parseMetadataItem(const QDomNode &metadataNode);
    bool parseManifestItem(const QDomNode &manifestNodes, const QString currentFolder);
    bool parseSpineItem(const QDomNode &spineNode);

    const KArchiveFile *getFile(const QString &path);

    KZip *m_archive;
    const KArchiveDirectory *m_rootFolder;

    QMap<QString, QString> m_metadata;
    QString m_indexItem;

    QMap<QString, EpubItem> m_items;
    QVector<QString> m_orderedItems;
    QSet<QString> m_unorderedItems;

    QMimeDatabase m_mimeDatabase;
};

#endif // EPUBPARSER_H
