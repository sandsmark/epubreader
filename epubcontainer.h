#ifndef EPUBCONTAINER_H
#define EPUBCONTAINER_H

#include <QObject>
#include <QHash>
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
    QByteArray mimetype;
};

struct EpubPageReference {
    enum StandardType {
        CoverPage,
        TitlePage,
        TableOfContents,
        Index,
        Glossary,
        Acknowledgements,
        Bibliography,
        Colophon,
        CopyrightPage,
        Dedication,
        Epigraph,
        Foreword,
        ListOfIllustrations,
        ListOfTables,
        Notes,
        Preface,
        Text,
        Other
    };

    static StandardType typeFromString(const QString &name);

    QString target;
    QString title;
};

class EPubContainer : public QObject
{
    Q_OBJECT
public:
    explicit EPubContainer(QObject *parent);
    ~EPubContainer();

    bool openFile(const QString path);

    EpubItem getEpubItem(const QString &id) const { return m_items.value(id); }

    QSharedPointer<QIODevice> getIoDevice(const QString &path);
    QImage getImage(const QString &id);
    QString getMetadata(const QString &key);
    QStringList getItems() { return m_orderedItems; }

    QString getStandardPage(EpubPageReference::StandardType type) { return m_standardReferences.value(type).target; }

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
    bool parseGuideItem(const QDomNode &guideItem);

    const KArchiveFile *getFile(const QString &path);

    KZip *m_archive;
    const KArchiveDirectory *m_rootFolder;

    QHash<QString, QString> m_metadata;

    QHash<QString, EpubItem> m_items;
    QStringList m_orderedItems;
    QSet<QString> m_unorderedItems;

    QHash<EpubPageReference::StandardType, EpubPageReference> m_standardReferences;
    QHash<QString, EpubPageReference> m_otherReferences;
    QMimeDatabase m_mimeDatabase;
};

#endif // EPUBCONTAINER_H
