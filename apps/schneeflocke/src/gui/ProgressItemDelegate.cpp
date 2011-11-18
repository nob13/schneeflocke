#include "ProgressItemDelegate.h"
#include <QApplication>

void ProgressItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
	float progress = index.data().toFloat();
	float progressPercent = progress * 100.0;
	QStyleOptionProgressBar progressBarOption;
	progressBarOption.rect = option.rect;
	progressBarOption.minimum = 0;
	progressBarOption.maximum = 100;
	progressBarOption.progress = (int) (progressPercent);
	progressBarOption.text = QString::number(progressPercent, 'f', 2) + "%";
	progressBarOption.textVisible = true;

	QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
}

