#ifndef EPUBDOCUMENT_H
#define EPUBDOCUMENT_H

#include "epubcontainer.h"
#include <QObject>
#include <QTextDocument>
#include <QImage>


class EPubContainer;

class EPubDocument : public QTextDocument
{
    Q_OBJECT

public:
    explicit EPubDocument();
    virtual ~EPubDocument();

    bool loaded() { return m_loaded; }

    void openDocument(const QString &path);
    void clearCache() { m_renderedSvgs.clear(); }

signals:
    void loadCompleted();

protected:
    virtual QVariant loadResource(int type, const QUrl &url) override;

private slots:
    void loadDocument();

private:
    void fixImages(QDomDocument &newDocument);
    const QImage &getSvgImage(const QString &id);

    QHash<QString, QByteArray> m_svgs;
    QHash<QString, QImage> m_renderedSvgs;

    QString m_documentPath;
    EPubContainer *m_container;
    EpubItem m_currentItem;

    bool m_loaded;
};

#endif // EPUBDOCUMENT_H
