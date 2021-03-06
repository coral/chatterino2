#pragma once

#include <QFlags>
#include <QLabel>
#include <QMouseEvent>
#include <QWidget>

class SignalLabel : public QLabel
{
    Q_OBJECT

public:
    explicit SignalLabel(QWidget *parent = 0, Qt::WindowFlags f = 0)
        : QLabel(parent, f)
    {
    }
    virtual ~SignalLabel() = default;

signals:
    void mouseDoubleClick(QMouseEvent *ev);

    void mouseDown();
    void mouseUp();

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent *ev) override
    {
        emit this->mouseDoubleClick(ev);
    }

    virtual void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            emit mouseDown();
        }

        event->ignore();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            emit mouseUp();
        }

        event->ignore();
    }

    virtual void mouseMoveEvent(QMouseEvent *event) override
    {
        event->ignore();
    }
};
