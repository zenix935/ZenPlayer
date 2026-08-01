#ifndef LISTITEMDELEGATE_H
#define LISTITEMDELEGATE_H

#include <QIcon>
#include <QEvent>
#include <QCursor>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QAbstractItemView>
#include <QStyledItemDelegate>

class ListItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ListItemDelegate(QObject* parent=nullptr) : QStyledItemDelegate(parent) {}

    // Compute the 3-dot menu button rect (right side of visible area)
    static QRect menuBtnRect(const QRect& fullRect, int visibleRight=-1)
    {
        int menuBtnWidth=22;
        int menuBtnHeight=22;
        int rightPadding=6;
        int right=(visibleRight > 0) ? visibleRight : fullRect.right();
        return QRect(right-rightPadding-menuBtnWidth,
                     fullRect.top()+(fullRect.height()-menuBtnHeight)/2,
                     menuBtnWidth, menuBtnHeight);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();

        QStyleOptionViewItem opt=option;
        initStyleOption(&opt, index);

        QRect fullRect=option.rect;

        // Determine viewport widget for accurate coordinate mapping
        const QWidget* viewport=nullptr;
        if (const QAbstractItemView* view=qobject_cast<const QAbstractItemView*>(opt.widget))
            viewport=view->viewport();
        else
            viewport=opt.widget;

        int vpWidth=viewport ? viewport->width() : fullRect.right();
        QRect visibleRect=fullRect;
        if (visibleRect.right() > vpWidth)
            visibleRect.setRight(vpWidth);
        QRect menuRect=menuBtnRect(fullRect, vpWidth);

        QPoint mousePos=viewport ? viewport->mapFromGlobal(QCursor::pos()) : QPoint(-1, -1);
        bool isHovered=(opt.state & QStyle::State_MouseOver) || fullRect.contains(mousePos);

        if (isHovered)
            opt.state |= QStyle::State_MouseOver;

        // Draw item row and panel background
        QStyle* style=opt.widget ? opt.widget->style() : QApplication::style();
        QStyleOptionViewItem clampedOpt=opt;
        clampedOpt.rect=visibleRect;
        style->drawPrimitive(QStyle::PE_PanelItemViewRow, &clampedOpt, painter, opt.widget);
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &clampedOpt, painter, opt.widget);

        // Lighten up background on hover if not selected
        if (isHovered && !(opt.state & QStyle::State_Selected)) 
        {
            QColor hoverOverlay=QColor(57, 57, 57, 255);
            painter->setBrush(hoverOverlay);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(visibleRect, 8, 8);
        }

        if (isHovered)
        {
            painter->setRenderHint(QPainter::Antialiasing, true);

            // Draw 3-dot menu button on the right side
            bool isMenuHovered=menuRect.contains(mousePos);
            QColor menuBg=isMenuHovered ? QColor(100, 100, 100, 200) : QColor(80, 80, 80, 120);
            painter->setBrush(menuBg);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(menuRect, 4, 4);

            // Draw 3 vertical dots
            painter->setBrush(QColor(220, 220, 220));
            int dotRadius=2;
            int centerX=menuRect.center().x()+1;
            int centerY=menuRect.center().y()+1;
            int dotSpacing=5;
            painter->drawEllipse(QPoint(centerX, centerY-dotSpacing), dotRadius, dotRadius);
            painter->drawEllipse(QPoint(centerX, centerY), dotRadius, dotRadius);
            painter->drawEllipse(QPoint(centerX, centerY+dotSpacing), dotRadius, dotRadius);
        }

        // Draw item text with room for menu button on hover
        int leftPadding=6;
        QRect textRect=visibleRect;
        textRect.setLeft(fullRect.left()+leftPadding);
        if (isHovered)
            textRect.setRight(menuRect.left()-3);

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
                const QWidget* vp=nullptr;
                if (const QAbstractItemView* v=qobject_cast<const QAbstractItemView*>(option.widget))
                    vp=v->viewport();
                int vpW=vp ? vp->width() : option.rect.right();
                QRect menuRect=menuBtnRect(option.rect, vpW);
                if (menuRect.contains(mouseEvent->pos()))
                {
                    if (event->type()==QEvent::MouseButtonRelease)
                        emit menuClicked(index);
                    return true;
                }
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

signals:
    void menuClicked(const QModelIndex &index);
};

#endif // LISTITEMDELEGATE_H
