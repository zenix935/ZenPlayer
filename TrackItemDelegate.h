#ifndef TRACKITEMDELEGATE_H
#define TRACKITEMDELEGATE_H

#include <QIcon>
#include <QEvent>
#include <QCursor>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QAbstractItemView>
#include <QStyledItemDelegate>

class TrackItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit TrackItemDelegate(QObject* parent=nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();

        QStyleOptionViewItem opt=option;
        initStyleOption(&opt, index);

        int btnWidth=22;
        int btnHeight=22;
        int leftPadding=6;
        int spacing=3;

        QRect fullRect=option.rect;
        QRect btnRect(fullRect.left()+leftPadding, fullRect.top()+(fullRect.height()-btnHeight)/2, btnWidth, btnHeight);

        // Determine viewport widget for accurate coordinate mapping
        const QWidget* viewport=nullptr;
        if (const QAbstractItemView* view=qobject_cast<const QAbstractItemView*>(opt.widget))
            viewport=view->viewport();
        else
            viewport=opt.widget;

        QPoint mousePos=viewport ? viewport->mapFromGlobal(QCursor::pos()) : QPoint(-1, -1);
        bool isHovered=(opt.state & QStyle::State_MouseOver) || fullRect.contains(mousePos);

        if (isHovered)
            opt.state |= QStyle::State_MouseOver;

        // Draw item row and panel background (handles item selection & native style hover background)
        QStyle* style=opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, painter, opt.widget);
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        // Lighten up background on hover if not selected
        if (isHovered && !(opt.state & QStyle::State_Selected)) 
        {
            QColor hoverOverlay=QColor(57, 57, 57, 255);
            painter->setBrush(hoverOverlay);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(fullRect, 8 ,8);
        }

        if (isHovered) 
        {
            bool isBtnHovered=btnRect.contains(mousePos);

            painter->setRenderHint(QPainter::Antialiasing, true);

            // Circular background for play button
            QColor circleBg=isBtnHovered ? QColor(100, 0, 0, 230) : QColor(100, 0, 0, 140);
            painter->setBrush(circleBg);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(btnRect, 4, 4);

            // Draw play icon inside circle
            QIcon playIcon(":/pics/pics/play.png");
            QRect iconRect=btnRect.adjusted(3, 3, -3, -3);
            playIcon.paint(painter, iconRect, Qt::AlignCenter);
        }

        // Draw track name text (shifted right so play button doesn't overlap)
        QRect textRect=fullRect;
        textRect.setLeft(fullRect.left()+leftPadding + btnWidth+spacing);

        QString text=index.data(Qt::DisplayRole).toString();

        QColor textColor=opt.palette.text().color();
        if (opt.state & QStyle::State_Selected)
            textColor=opt.palette.highlightedText().color();

        painter->setPen(textColor);
        painter->setFont(opt.font);

        QString elidedText=painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()-8);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedText);

        painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize sz=QStyledItemDelegate::sizeHint(option, index);
        sz.setHeight(qMax(sz.height(), 32));
        return sz;
    }
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        if (event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonRelease) 
        {
            QMouseEvent* mouseEvent=static_cast<QMouseEvent*>(event);
            if (mouseEvent->button()==Qt::LeftButton) 
            {
                int btnWidth=22;
                int btnHeight=22;
                int leftPadding=8;
                QRect btnRect(option.rect.left()+leftPadding, option.rect.top()+(option.rect.height()-btnHeight)/2, btnWidth, btnHeight);

                if (btnRect.contains(mouseEvent->pos())) 
                {
                    if (event->type() == QEvent::MouseButtonRelease)
                        emit playClicked(index);
                    return true;
                }
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

signals:
    void playClicked(const QModelIndex &index);
};

#endif // TRACKITEMDELEGATE_H
