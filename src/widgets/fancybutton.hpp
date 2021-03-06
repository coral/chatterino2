#pragma once

#include "widgets/basewidget.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPoint>
#include <QTimer>
#include <QWidget>

namespace chatterino {
namespace widgets {

class FancyButton : public BaseWidget
{
    struct ClickEffect {
        double progress = 0.0;
        QPoint position;

        ClickEffect(QPoint _position)
            : position(_position)
        {
        }
    };

public:
    FancyButton(BaseWidget *parent);

    void setMouseEffectColor(QColor color);

protected:
    virtual void paintEvent(QPaintEvent *) override;
    virtual void enterEvent(QEvent *) override;
    virtual void leaveEvent(QEvent *) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;

    void fancyPaint(QPainter &painter);

private:
    bool selected = false;
    bool mouseOver = false;
    bool mouseDown = false;
    QPoint mousePos;
    double hoverMultiplier = 0.0;
    QTimer effectTimer;
    std::vector<ClickEffect> clickEffects;
    QColor mouseEffectColor = {255, 255, 255};

    void onMouseEffectTimeout();
};

}  // namespace widgets
}  // namespace chatterino
