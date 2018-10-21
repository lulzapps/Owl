// Owl - www.owlclient.com
// Copyright (c) 2012-2017, Adalid Claure <aclaure@gmail.com>

#include <QFile>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include "BoardManager.h"

namespace owl
{

BoardManager::BoardManager()
	: _encryptor(new CRijndael()),
	_mutex(QMutex::Recursive)
{
	_encryptor->MakeKey(DBPASSWORD_KEY, DBPASSWORD_SEED);
}	
	
BoardManager::~BoardManager()
{
	//_instance = 0;
}

size_t BoardManager::getBoardCount() const
{
	return _boardList.size();
}
    
void BoardManager::init()
{
	QMutexLocker locker(&_mutex);
    QSqlDatabase db = QSqlDatabase::database(OWL_DATABASE_NAME);

    if (!db.isOpen())
    {
        logger()->trace("datbase is not open, trying to open");
        db.open();
    }
    
    QSqlQuery query = db.exec("SELECT * FROM boards");
    QSqlRecord rec = query.record();
    _boardList.clear();

	int id = rec.indexOf("boardid");
    int iName = rec.indexOf("name");
    int iUrl = rec.indexOf("url");
	int iServiceUrl = rec.indexOf("serviceUrl");
    int iParser = rec.indexOf("parser");

	int iUsername = rec.indexOf("username");
	int iPassword = rec.indexOf("password");

	int iEnabledIdx = rec.indexOf("enabled");
	int iAutoLogin = rec.indexOf("autologin");
	int iIcon = rec.indexOf("icon");
	int iLastUpdate = rec.indexOf("lastupdate");

	if (query.isActive())
	{
		while (query.next())
		{
			BoardPtr b = BoardPtr(new Board());

            auto retrievedId = query.value(id).toUInt();
            const auto it = std::find_if(_boardList.begin(), _boardList.end(),
                 [&retrievedId](const owl::BoardPtr other)
                 {
                     return (retrievedId == other->getDBId());
                 });
            
            if (it != _boardList.end())
            {
                continue;
            }
            
			b->setDBId(retrievedId);
			b->setName(query.value(iName).toString());
			b->setUrl(query.value(iUrl).toString());
			b->setServiceUrl(query.value(iServiceUrl).toString());
			b->setProtocolName(query.value(iParser).toString());

			b->setUsername(query.value(iUsername).toString());

			QString rawPassword(query.value(iPassword).toString());
			b->setPassword(_encryptor->Decrypt(rawPassword).toLatin1());

			b->setEnabled(query.value(iEnabledIdx).toBool());
			b->setAutoLogin(query.value(iAutoLogin).toBool());
			b->setFavIcon(query.value(iIcon).toString());

			QString updateStr = query.value(iLastUpdate).toString();
			QDateTime lastUpdate = QDateTime::fromString(updateStr, Qt::ISODate);
			if (!lastUpdate.isValid())
			{
				logger()->warn("Could not parser last update for boardId '%1' with date value '%2'", 
					b->getDBId(),
					updateStr
				);
            
				// Addy's birthday easter egg
                b->setLastUpdate(QDateTime(QDate(1975,3,24), QTime(15,55)));
			}
            else
            {
                b->setLastUpdate(lastUpdate);
            }

			loadBoardOptions(b);
			retrieveBoardForums(b);

			_boardList.push_back(b);
			logger()->trace("Loaded '%1', last updated '%2'", b->getName(), b->getLastUpdate().toString());
		}

		qSort(_boardList.begin(), _boardList.end(), &BoardManager::boardDisplayOrderLessThan);
	}

	logger()->info("%1 board(s) loaded", (int)getBoardCount());
}

owl::BoardPtr BoardManager::loadBoard(int boardId)
{
	QMutexLocker locker(&_mutex);

	BoardPtr b(new Board());
	QSqlDatabase db = QSqlDatabase::database(OWL_DATABASE_NAME);

	if (!db.isOpen())
	{
		logger()->trace("datbase is not open, trying to open");
		db.open();
	}

    QSqlQuery query(db);
    query.prepare("SELECT * FROM boards WHERE boardid = :boardid");
	query.bindValue(":boardid", boardId);
    
    if (query.exec())
    {
        QSqlRecord rec = query.record();

        int id = rec.indexOf("boardid");
        int iName = rec.indexOf("name");
        int iUrl = rec.indexOf("url");
        int iServiceUrl = rec.indexOf("serviceUrl");
        int iParser = rec.indexOf("parser");

        int iUsername = rec.indexOf("username");
        int iPassword = rec.indexOf("password");

        int iEnabledIdx = rec.indexOf("enabled");
        int iAutoLogin = rec.indexOf("autologin");
        int iIcon = rec.indexOf("icon");
        int iLastUpdate = rec.indexOf("lastupdate");

        if (query.next())
        {
            b->setDBId(query.value(id).toUInt());
            b->setName(query.value(iName).toString());
            b->setUrl(query.value(iUrl).toString());
            b->setServiceUrl(query.value(iServiceUrl).toString());
            b->setProtocolName(query.value(iParser).toString());

            b->setUsername(query.value(iUsername).toString());

			QString rawPassword(query.value(iPassword).toString());
			b->setPassword(_encryptor->Decrypt(rawPassword).toLatin1());

            b->setEnabled(query.value(iEnabledIdx).toBool());
            b->setAutoLogin(query.value(iAutoLogin).toBool());
            b->setFavIcon(query.value(iIcon).toString());

            QString updateStr = query.value(iLastUpdate).toString();
            QDateTime lastUpdate = QDateTime::fromString(updateStr, Qt::ISODate);
            if (!lastUpdate.isValid())
            {
                logger()->warn("Could not parser last update for boardId '%1' with date value '%2'", 
                    b->getDBId(),
                    updateStr
                    );
                
                // Addy's birthday easter egg
                b->setLastUpdate(QDateTime(QDate(1975,3,24), QTime(15,55)));
            }
            else
            {
                b->setLastUpdate(lastUpdate);
            }

            loadBoardOptions(b);
            retrieveBoardForums(b);

            _boardList.push_back(b);
            logger()->trace("Loaded '%1', last updated '%2'", b->getName(), b->getLastUpdate().toString());
        }
    }

	logger()->debug("Successfully loaded board '%1'", (int)getBoardCount());
    query.finish();
	return b;
}

void BoardManager::loadBoardOptions(const BoardPtr& board)
{
    QSqlDatabase db = QSqlDatabase::database(OWL_DATABASE_NAME);

    if (!db.isOpen())
    {
        logger()->trace("datbase is not open, trying to open");
        db.open();
    }
    
	QSqlQuery query(db);

	query.prepare("SELECT * FROM boardvars WHERE boardid=:boardid");
	query.bindValue(":boardid", board->getDBId());
	
	if (query.exec())
	{
		QSqlRecord rec = query.record();
		int iName = rec.indexOf("name");
		int iValue = rec.indexOf("value");

		while (query.next())
		{
			board->getOptions()->add(
				query.value(iName).toString(),
				query.value(iValue).toString());
		}
	}
	else
	{
		logger()->error("updateBoard() failed: %1", query.lastError().text());
		logger()->debug("executed query: %1", query.lastQuery());
	}
}

void BoardManager::reload()
{
	_boardList.clear();
	init();
}
    
void BoardManager::sort()
{
    QMutexLocker locker(&_mutex);
    qSort(_boardList.begin(), _boardList.end(), &BoardManager::boardDisplayOrderLessThan);
}
    
void BoardManager::firstTimeInit()
{
 //   QSqlDatabase db = QSqlDatabase::database(OWL_DATABASE_NAME);

	//QFile file(":sql/owl.sql");

 //   if(!file.open(QIODevice::ReadOnly)) 
	//{
	//	throw OWL_EXCEPTION("Could not load owl.sql file");   
	//}

	//QTextStream in(&file);
	//QString sqlFile = in.readAll();
	//file.close();

	//BOOST_FOREACH(QString statement, sqlFile.split(';'))
	//{
	//	statement = statement.trimmed();

	//	if (!statement.isEmpty())
	//	{
	//		QSqlQuery query(db);

	//		if (!query.exec(statement))
	//		{
	//		   logger()->fatal("Query failed: '%1'", statement);
	//		   logger()->fatal("Last error: %1", query.lastError().text());
	//		}
	//	}
	//}
}

void BoardManager::createForumVars(ForumPtr forum)
{
	QSqlDatabase	db = QSqlDatabase::database(OWL_DATABASE_NAME);
	QSqlQuery		query(db);

	query.prepare("INSERT INTO forumvars "
		"(forumsid, name, value) "
		"VALUES (:forumsid, :name, :value)");	

	query.bindValue(":forumsid", forum->getDBId());

    for (const auto& p : forum->getVars())
	{
        query.bindValue(":name", p.first);
        query.bindValue(":value", p.second);

		if (!query.exec())
		{
			logger()->error("createForumVars() failed: %1", query.lastError().text());
			logger()->debug("executed query: %1", query.lastQuery());
		}
	}

	db.commit();
}

void BoardManager::createForumEntries( ForumPtr forum, BoardPtr board )
{
	QSqlDatabase	db = QSqlDatabase::database(OWL_DATABASE_NAME);
	QSqlQuery		query(db);

	query.prepare("INSERT INTO forums "
		"(boardId, forumId, parentId, forumName, forumType, forumOrder) "
		"VALUES (:boardId, :forumId, :parentId, :forumName, :forumType, :forumOrder)");	

	query.bindValue(":boardId", board->getDBId());
	query.bindValue(":forumId", forum->getId());
	
	QString rootId = board->getOptions()->getText("rootId");
	query.bindValue(":parentId", forum->getParent() != NULL ? forum->getParent()->getId() : rootId);
	
	query.bindValue(":forumName", forum->getName());
	query.bindValue(":forumType", forum->getForumTypeString());
	query.bindValue(":forumOrder", forum->getDisplayOrder());

	if (query.exec())
	{
		db.commit();
		forum->setDBId(query.lastInsertId().toInt());

		createForumVars(forum);
	}
	else
	{
		logger()->error("createForumEntries() failed: %1", query.lastError().text());
		logger()->debug("executed query: %1", query.lastQuery());
	}

    for (ForumPtr f : forum->getForums())
	{
		createForumEntries(f, board);
	}
}

void BoardManager::createBoardOptions(BoardPtr board)
{
	QSqlDatabase	db = QSqlDatabase::database(OWL_DATABASE_NAME);
	QSqlQuery		query(db);

	query.prepare("INSERT INTO boardvars "
		"(boardid, name, value) "
		"VALUES (:boardid, :name, :value)");	

	query.bindValue(":boardid", board->getDBId());

    for (const auto& p : *(board->getOptions()))
	{
        query.bindValue(":name", p.first);
        query.bindValue(":value", p.second);

		if (!query.exec())
		{
			logger()->error("createForumVars() failed: %1", query.lastError().text());
			logger()->debug("executed query: %1", query.lastQuery());
		}
	}

	db.commit();
}

bool BoardManager::createBoard(BoardPtr board)
{
	QMutexLocker	locker(&_mutex);
	QSqlDatabase	db = QSqlDatabase::database(OWL_DATABASE_NAME);
	QSqlQuery		query(db);
	bool			bRet(false);

	query.prepare("INSERT INTO boards "
		"(enabled, autologin, name, url, parser, "
		"serviceUrl, username, password, icon, lastupdate) "
		"VALUES (:enabled, :autologin, :name, :url, :parser, :serviceurl, "
		":username, :password, :icon, :lastupdate)");

	query.bindValue(":enabled", board->isEnabled() ? "1" : "0");
	query.bindValue(":autologin", board->isAutoLogin() ? "1" : "0");
	query.bindValue(":name", board->getName());
	query.bindValue(":url", board->getUrl());
	query.bindValue(":parser", board->getParser()->getName());
	query.bindValue(":serviceurl", board->getServiceUrl());
	query.bindValue(":username", board->getUsername());
	
	QString plainTextPW(board->getPassword());
	query.bindValue(":password", _encryptor->Encrypt(plainTextPW));
	
	query.bindValue(":icon", board->getFavIcon());
	query.bindValue(":lastupdate", QDateTime::currentDateTime());

	if (query.exec())
	{
		db.commit();
		board->setDBId(query.lastInsertId().toInt());
		
		_boardList.push_back(board);
		qSort(_boardList.begin(), _boardList.end(), &BoardManager::boardDisplayOrderLessThan);

		bRet = true;
	}
	else
	{
		logger()->error("createBoard() failed: %1", query.lastError().text());
		logger()->debug("executed query: %1", query.lastQuery());
	}

	if (bRet)
	{
		this->createBoardOptions(board);

        for (ForumPtr forum : board->getRoot()->getForums())
		{
			createForumEntries(forum, board);
		}
	}

	return bRet;
}

void BoardManager::retrieveSubForumVars(ForumPtr forum)
{
	QSqlDatabase	db(QSqlDatabase::database(OWL_DATABASE_NAME));

	if (!db.isOpen())
	{
		db.open();
	}

	QSqlQuery query(db);

	query.prepare(
		"SELECT * FROM forumvars "
		"WHERE forumsid=:forumsid");

	query.bindValue(":forumsid", forum->getDBId());

	if (query.exec())
	{
		forum->getVars().clear();

		QSqlRecord rec = query.record();

		int name = rec.indexOf("name");
		int value = rec.indexOf("value");

		while (query.next())
		{
			forum->setVar(query.value(name).toString(), query.value(value).toString());
		}
	}
}

void BoardManager::retrieveSubForumList(BoardPtr board, ForumPtr forum, bool bDeep /*= false*/)
{
	ForumList		list; 
    QSqlDatabase	db(QSqlDatabase::database(OWL_DATABASE_NAME));

	if (!db.isOpen())
	{
		db.open();
	}

	QSqlQuery query(db);
	
	query.prepare(
		"SELECT * FROM forums "
		"WHERE boardId=:boardid "
		"AND parentId=:parentid "
		"ORDER BY forumOrder");
	
	query.bindValue(":boardid", board->getDBId());
	query.bindValue(":parentid", forum->getId());

	if (query.exec())
	{
		forum->getForums().clear();

		QSqlRecord rec = query.record();

		int id = rec.indexOf("id");
		int forumId = rec.indexOf("forumId");
		int forumName = rec.indexOf("forumName");
		int forumType = rec.indexOf("forumType");
		int forumOrder = rec.indexOf("forumOrder");

		while (query.next())
		{
			ForumPtr newForum(new Forum(query.value(forumId).toString()));
			newForum->setDBId(query.value(id).toUInt());
			newForum->setName(query.value(forumName).toString());
			newForum->setDisplayOrder(query.value(forumOrder).toUInt());

			forum->addChild(newForum);
			newForum->setBoard(board);

			QString typeStr(query.value(forumType).toString());
			if (typeStr == "FORUM")
			{
				newForum->setForumType(Forum::FORUM);
			}
			else if (typeStr == "CATEGORY")
			{
				newForum->setForumType(Forum::CATEGORY);
			}
			else
			{
				newForum->setForumType(Forum::LINK);
			}
			
			forum->getForums().push_back(newForum);
		}
	}
	else
	{
		logger()->error("retrieveSubForumList() failed: %1", query.lastError().text());
		logger()->debug("executed query: %1", query.lastQuery());
	}

    for (ForumPtr current : forum->getForums())
	{
		retrieveSubForumVars(current);
	}

	if (bDeep)
	{
        for (ForumPtr child : forum->getForums())
		{
			retrieveSubForumList(board, child, bDeep);
		}
	}
}

bool BoardManager::retrieveBoardForums(BoardPtr b)
{
	bool			bRet(false);

	QString rootId = b->getOptions()->getText("rootId");

	ForumPtr root = Forum::createRootForum(rootId);
	retrieveSubForumList(b, root, true);
	b->setRoot(root);

	return bRet;
}

uint BoardManager::updateBoards()
{
	QMutexLocker locker(&_mutex);
	uint iCount = 0;

	for (BoardPtr b : _boardList)
	{
		if (updateBoard(b))
		{
			iCount++;
		}
	}

	return iCount;
}

bool BoardManager::updateBoard(BoardPtr board)
{
	QMutexLocker locker(&_mutex);
	QSqlDatabase db = QSqlDatabase::database(OWL_DATABASE_NAME);
	bool bRet = false;

	QSqlQuery query(db);

	query.prepare(
		"UPDATE boards SET "
		"name=:name, url=:url, serviceUrl=:serviceurl, username=:username, password=:password, "
		"lastupdate = :lastupdate, autologin=:autologin "
		"WHERE boardid = :id");

	query.bindValue(":name", board->getName());
	query.bindValue(":url", board->getUrl());
	query.bindValue(":serviceurl", board->getServiceUrl());
	query.bindValue(":username", board->getUsername());
	query.bindValue(":lastupdate", board->getLastUpdate());
    
	QString plainTextPW(board->getPassword());
	query.bindValue(":password", _encryptor->Encrypt(plainTextPW));
	
	query.bindValue(":id", board->getDBId());
	query.bindValue(":autologin", board->isAutoLogin() ? "1" : "0");

	if (query.exec())
	{
		query.finish();
		updateBoardOptions(board, true);
        db.commit();
		
        bRet = true;
	}
	else
	{
		logger()->error("updateBoard() failed: %1", query.lastError().text());
		logger()->debug("executed query: %1", query.lastQuery());
        OWL_THROW_EXCEPTION(BoardManagerException(query.lastError().text(), query.lastQuery()));
	}

	return bRet;
}

bool BoardManager::deleteBoard(BoardPtr board)
{
	QMutexLocker locker(&_mutex);
	QSqlDatabase db = QSqlDatabase::database(OWL_DATABASE_NAME);
	bool bRet = false;

	if (db.open())
	{
		auto displayOrder = board->getOptions()->getInt("displayOrder");

		QSqlQuery query(db);

		query.prepare(QString("DELETE FROM boards WHERE boardid = :id"));
		query.bindValue(":id", board->getDBId());

		if (!query.exec())
		{
			logger()->error("deleteBoard() failed in 'boards' table: %1", query.lastError().text());
			logger()->debug("executed query: %1", query.lastQuery());
		}
        
        query.prepare(QString("DELETE FROM boardvars WHERE boardid = :id"));
        query.bindValue(":id", board->getDBId());
        
        if (!query.exec())
        {
			logger()->error("deleteBoard() failed in 'boardvars' table: %1", query.lastError().text());
			logger()->debug("executed query: %1", query.lastQuery());
        }
        
        query.prepare("SELECT * FROM forums WHERE boardId=:boardid");
        query.bindValue(":boardid", board->getDBId());
        
        if (query.exec())
        {
            QSqlRecord rec = query.record();
            
            // the index of the forum's id in relation to the message
            // board and NOT in the db
            //int forumIdIdx = rec.indexOf("forumId");

			// dbID
			int forumIdIdx = rec.indexOf("id");
            
            while (query.next())
            {
                deleteForumVars(query.value(forumIdIdx).toString());
            }
        }

        query.prepare("DELETE FROM forums WHERE boardId = :id");
        query.bindValue(":id", board->getDBId());
        
        if (query.exec())
        {
			logger()->debug("executed query: %1", query.lastQuery());
		}

        
        db.commit();
        
        auto iPos = _boardList.indexOf(board);
        if (iPos != -1)
        {
            _boardList.removeAt(iPos);
        }

		if (displayOrder <= _boardList.size())
		{
			for (BoardPtr b : _boardList)
			{
				auto bDO = b->getOptions()->getInt("displayOrder");
				if (bDO > displayOrder)
				{
					b->getOptions()->setOrAdd("displayOrder", (int)(bDO - 1));
					updateBoardOptions(b, false);
				}
			}

			db.commit();
		}
	}
	else
	{
		logger()->warn("deleteBoard() failed. database not open");
	}    

	return bRet;
}
        
bool BoardManager::deleteForumVars(const QString& forumId) const
{
    bool bRet = false;
	QSqlDatabase db = QSqlDatabase::database(OWL_DATABASE_NAME);

	QSqlQuery query(db);

	query.prepare(QString("DELETE FROM forumvars WHERE forumsid = :id"));
    query.bindValue(":id", forumId);
	logger()->trace("deleteing vars in forum %1", forumId);

    if (!query.exec())
    {
		logger()->error("deleteForumVars() failed in 'forumvars' table: %1", query.lastError().text());
		logger()->debug("executed query: %1", query.lastQuery());
    }
    
    return bRet;
}

void BoardManager::updateBoardOptions(BoardPtr board, bool bDoCommit /*= false*/)
{
	QSqlDatabase db = QSqlDatabase::database(OWL_DATABASE_NAME);
	QSqlQuery query(db);

    for (const auto& p : *(board->getOptions()))
	{
		query.prepare(
			"UPDATE boardvars SET value=:value "
			"WHERE boardid = :id AND name=:name");

        query.bindValue(":value", p.second);
		query.bindValue(":id", board->getDBId());
        query.bindValue(":name", p.first);

		if (!query.exec())
		{
			logger()->error("updateBoardOptions() failed: %1", query.lastError().text());
			logger()->debug("executed query: %1", query.lastQuery());
		}
	}

	query.finish();

	if (bDoCommit)
	{
		db.commit();
	}
}

BoardPtr BoardManager::boardByItem(QStandardItem* item) const
{
	BoardPtr board;

    for (BoardPtr b : _boardList)
	{
		if (b->getModelItem() == item)
		{
			board = b;
			break;
		}
	}

	return board;
}

BoardManagerPtr BoardManager::_instance;

} // namespace owl
