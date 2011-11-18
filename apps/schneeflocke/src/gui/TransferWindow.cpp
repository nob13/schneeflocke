#include "TransferWindow.h"
#include "../modeladapters/TransferList.h"
#include "../types.h"
#include <schnee/tools/FileTools.h>
#include <QMenu>
#include <QDesktopServices>


TransferWindow::TransferWindow (Controller * controller) {
	mController = controller;
	mTransferWindowUi.setupUi (this);

	mOpenDirectoryAction = 0;
	mOpenFileAction = 0;
	mCancelTransferAction = 0;
	mClearTransferAction = 0;
	mTransferId = 0;
	mSeparator = 0;
	mIsOutgoing = false;
}

void TransferWindow::init () {
	// Actions
	mOpenDirectoryAction = new QAction (tr("Open Directory"), this);
	mOpenFileAction = new QAction (tr ("Open File"), this);
	mCancelTransferAction = new QAction (tr ("Cancel Transfer"), this);
	mClearTransferAction = new QAction (tr ("Clear Transfers"), this);
	mSeparator = new QAction (tr ("Separator"), this);
	mSeparator->setSeparator(true);

	connect (mOpenDirectoryAction, SIGNAL (triggered()), this, SLOT (onOpenDirectory()));
	connect (mOpenFileAction, SIGNAL (triggered()), this, SLOT (onOpenFile()));
	connect (mCancelTransferAction, SIGNAL (triggered()), this, SLOT (onCancelTransfer()));
	connect (mClearTransferAction, SIGNAL (triggered()), this, SLOT (onClearTransfers()));

	// Incoming
	mTransferWindowUi.incomingTransferTable->setModel (mController->incomingTransferList());
	mTransferWindowUi.incomingTransferTable->setItemDelegateForColumn (4, new ProgressItemDelegate (this));

	mTransferWindowUi.incomingTransferTable->setContextMenuPolicy (Qt::CustomContextMenu);
	connect (mTransferWindowUi.incomingTransferTable, SIGNAL (customContextMenuRequested(const QPoint &)), this, SLOT (onIncomingViewShowContextMenu(const QPoint&)));
	mTransferWindowUi.incomingTransferTable->horizontalHeader()->setResizeMode (QHeaderView::Stretch);

	// Outgoing
	mTransferWindowUi.outgoingTransferTable->setModel (mController->outgoingTransferList());
	mTransferWindowUi.outgoingTransferTable->setItemDelegateForColumn (4, new ProgressItemDelegate (this));
	mTransferWindowUi.outgoingTransferTable->setContextMenuPolicy (Qt::CustomContextMenu);
	connect (mTransferWindowUi.outgoingTransferTable, SIGNAL (customContextMenuRequested(const QPoint &)), this, SLOT (onOutgoingViewShowContextMenu(const QPoint&)));
	mTransferWindowUi.outgoingTransferTable->horizontalHeader()->setResizeMode (QHeaderView::Stretch);
}

void TransferWindow::onIncomingViewShowContextMenu (const QPoint & position) {
	QList<QAction*> actions;
	QModelIndex index = mTransferWindowUi.incomingTransferTable->indexAt (position);
	mIsOutgoing = false;
	if (index.isValid()){
		mTransferInfo = mController->incomingTransferList()->infoAt(index);
		mTransferId = mController->incomingTransferList()->idAt(index);
		if (!mTransferInfo.filename.empty()){
			if (mTransferInfo.type == sf::TransferInfo::FILE_TRANSFER) {
				actions.append(mOpenFileAction);
				actions.append(mOpenDirectoryAction);
			}
			if (mTransferInfo.type == sf::TransferInfo::DIR_TRANSFER) {
				actions.append(mOpenDirectoryAction);
			}
			if (mTransferInfo.state != sf::TransferInfo::FINISHED &&
					mTransferInfo.state != sf::TransferInfo::CANCELED &&
					mTransferInfo.state != sf::TransferInfo::ERROR){
				actions.append (mSeparator);
				actions.append (mCancelTransferAction);
			}
		}
	} else {
		mTransferId = 0;
	}
	actions.append (mClearTransferAction);
	if (actions.count() > 0){
		QMenu::exec (actions, QCursor::pos());
	}
}

void TransferWindow::onOutgoingViewShowContextMenu (const QPoint & position) {
	QList<QAction*> actions;
	QModelIndex index = mTransferWindowUi.outgoingTransferTable->indexAt (position);
	mIsOutgoing = true;
	if (index.isValid()){
		mTransferInfo = mController->outgoingTransferList()->infoAt(index);
		mTransferId = mController->outgoingTransferList()->idAt(index);
		actions.append (mCancelTransferAction);
		actions.append (mSeparator);
	} else {
		mTransferId = 0;
	}
	actions.append (mClearTransferAction);
	if (actions.count() > 0){
		QMenu::exec (actions, QCursor::pos());
	}
}


void TransferWindow::onOpenDirectory () {
	if (!mTransferInfo.filename.empty()){
		if (mTransferInfo.type == sf::TransferInfo::FILE_TRANSFER) {
			QUrl url = QUrl::fromLocalFile (qtString (sf::parentPath(mTransferInfo.filename)));
			QDesktopServices::openUrl (url);
		}
		if (mTransferInfo.type == sf::TransferInfo::DIR_TRANSFER) {
			QUrl url = QUrl::fromLocalFile (qtString (mTransferInfo.filename));
			QDesktopServices::openUrl (url);
		}
	}
}

void TransferWindow::onOpenFile() {
	if (!mTransferInfo.filename.empty() && mTransferInfo.type == sf::TransferInfo::FILE_TRANSFER){
		QDesktopServices::openUrl (QUrl::fromLocalFile (qtString(mTransferInfo.filename)));
	}
}

void TransferWindow::onCancelTransfer() {
	if (mTransferId != 0) {
		if (mIsOutgoing)
			mController->cancelOutgoingTransfer(mTransferId);
		else
			mController->cancelIncomingTransfer(mTransferId);
	}
}

void TransferWindow::onClearTransfers() {
	if (mIsOutgoing)
		mController->clearOutgoingTransfers();
	else
		mController->clearIncomingTransfers();
}

