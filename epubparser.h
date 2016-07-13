#ifndef EPUBPARSER_H
#define EPUBPARSER_H

#include <QObject>

class KZip;
class KArchiveDirectory;
class KArchiveFile;

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
    const KArchiveFile *getFile(const QString &path);

    KZip *m_archive;
    const KArchiveDirectory *m_rootFolder;
};

#endif // EPUBPARSER_H
