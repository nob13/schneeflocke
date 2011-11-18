#pragma once
#include <QStyledItemDelegate>

/**
 * An ItemDelegate which shows a Progress bar. This is used
 * in the view for the TransferList
 *
 * Its mostly copied from Qt' Documentation:
 * http://doc.trolltech.com/4.7/qabstractitemdelegate.html#details
 */
class ProgressItemDelegate : public QStyledItemDelegate {
public:
	ProgressItemDelegate (QObject * parent = 0) : QStyledItemDelegate (parent) {}

	// override
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};
