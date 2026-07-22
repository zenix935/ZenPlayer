#ifndef CLICKABLESLIDER_H
#define CLICKABLESLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include <QStyle>

class ClickableSlider : public QSlider
{
    Q_OBJECT
public:
    explicit ClickableSlider(QWidget *parent=nullptr) : QSlider(parent) {}

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button()==Qt::LeftButton)
        {
            int val=0;
            if (orientation()==Qt::Horizontal)
            {
                double pos=event->position().x()/(double)width();
                val=minimum()+pos*(maximum()-minimum());
            }
            else
            {
                double pos=(height()-event->position().y())/(double)height();
                val=minimum()+pos*(maximum()-minimum());
            }
            setValue(val);
            emit sliderMoved(val);
            emit valueChanged(val);
            event->accept();
        }
        QSlider::mousePressEvent(event);
    }
};

#endif // CLICKABLESLIDER_H
