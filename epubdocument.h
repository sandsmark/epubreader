#ifndef EPUBDOCUMENT_H
#define EPUBDOCUMENT_H

#include <QObject>
#include <QTextDocument>

#include "epubcontainer.h"

class EPubContainer;

class EPubDocument : public QTextDocument
{
public:
    explicit EPubDocument(EPubContainer *container, QObject *parent = nullptr);

    void setChapter(const QString &id);

protected:
    QVariant loadResource(int type, const QUrl &name) override;

private:
    EPubContainer *m_container;
    EpubItem m_currentItem;
};

#endif // EPUBDOCUMENT_H
