#pragma once

#include <QtWidgets>
#include "Data/Board.h"

#define BOARDITEMPTR_ROLE      Qt::UserRole+1
#define LASTITEM_ROLE          Qt::UserRole+2

namespace owl
{

class BoardItemDelegate : public QStyledItemDelegate
{
    
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};
    
class BoardsModel : public QStandardItemModel
{
	Q_OBJECT
	
public:
    BoardsModel(QWidget* parent = nullptr);
	virtual ~BoardsModel();

    const static uint ITEMHEIGHT;

	// returns the parent item updated
	QStandardItem* updateForumItem(BoardPtr b, ForumPtr f);

    QStandardItem* addBoardItem(const BoardPtr& b, bool bThrowOnFail = false);
	QStandardItem* getBoardItem(BoardPtr b, bool bThrowOnFail = true);
	void removeBoardItem(BoardPtr b);

    QModelIndex getIndexByForumId(const QString&, QModelIndex);
    QString getIndexKey(BoardPtr b, BoardItemPtr bi);

	QStandardItem* getForumItem(ForumPtr f);

private: 
	void addForums(BoardPtr board, ForumPtr forum);
	QStandardItem* doGetForumItem(QStandardItem* parentItem, ForumPtr f);

	QHash<QString, QStandardItem*>      _index;
	QMutex                              _indexMutex;
};

} // owl
