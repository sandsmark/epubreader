#ifndef EPUBDOCUMENT_H
#define EPUBDOCUMENT_H

#include "epubcontainer.h"
#include <QObject>
#include <QTextDocument>


class EPubContainer;

class EPubDocument : public QTextDocument
{
    Q_OBJECT

public:
    explicit EPubDocument();
    virtual ~EPubDocument();

    bool loaded() { return m_loaded; }

    void openDocument(const QString &path);

signals:
    void loadCompleted();

protected:
    virtual QVariant loadResource(int type, const QUrl &name) override;

private slots:
    void loadDocument();

private:
    void fixImages(QDomDocument &newDocument);

    QHash<QString, QByteArray> m_svgs;

    QString m_documentPath;
    EPubContainer *m_container;
    EpubItem m_currentItem;

    bool m_loaded;
};

#endif // EPUBDOCUMENT_H
