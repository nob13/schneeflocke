#pragma once
#include "ui_transferwindow.h"
#include "../Controller.h"
#include "ProgressItemDelegate.h"
#include <QUrl>


/// Shows current transfers between clients
class TransferWindow : public QWidget {
	Q_OBJECT
public:
	TransferWindow (Controller * controller);
	void init ();
private slots:
	void onIncomingViewShowContextMenu (const QPoint & position);
	void onOutgoingViewShowContextMenu (const QPoint & position);
	void onOpenDirectory ();
	void onOpenFile();
	void onCancelTransfer();
	void onClearTransfers();
private:
	Ui::TransferWindow  mTransferWindowUi;
	Controller * mController;
	sf::TransferInfo mTransferInfo; ///< Current transfer info
	sf::AsyncOpId mTransferId;		///< Current selected transfer id
	bool mIsOutgoing;				///< Current selected transfer id is outgoing


	QAction * mOpenDirectoryAction;
	QAction * mOpenFileAction;
	QAction * mCancelTransferAction;
	QAction * mClearTransferAction;
	QAction * mSeparator;
};
