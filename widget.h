#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

class EPubDocument;
class EPubContainer;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);
    ~Widget();

    void openPage(int page);

    void setChapter(int chapter);
    void scroll(int amount);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    EPubContainer *m_parser;
    QImage m_cover;
    EPubDocument *m_document;
    int m_currentChapter;
    int m_yOffset;
};

#endif // WIDGET_H
