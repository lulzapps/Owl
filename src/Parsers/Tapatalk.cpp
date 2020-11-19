#include "../Utils/OwlUtils.h"
#include "../Utils/QSgml.h"
#include "Tapatalk.h"
#include <cmath>

#include <Utils/OwlLogger.h>

namespace owl
{

const uint Tapatalk4x::LOGINTIMEOUT = 60 * 15; // 15 minutes

Tapatalk4x::Tapatalk4x(const QString& baseUrl)
    : ParserBase(TAPATALK_NAME, TAPATALK_PRETTYNAME, baseUrl),
	  _rootId("-1"),
      _rootIdRealized(false),
	  _configLoaded(false),
	  _bUseMD5(false),
	  _bUseSha1(false),
      _apiLevel(3),
      _forumMapInitialized(false),
      _mutex(QMutex::Recursive),
      _logger(owl::initializeLogger("Tapatalk4x"))
{
	_options->add("boardware", "tapatalk");
	_options->add("boardwaremax", "4.9.9.0");
	_options->add("boardwaremin", "4.0");

    if (!baseUrl.endsWith("mobiquo.php"))
    {
        QString newUrl = QString("%1/mobiquo/mobiquo.php").arg(getBaseUrl());
        setBaseUrl(newUrl);
    }

    _webclient.setHeader("mobiquoid", "2");
    _webclient.setHeader("Mobiquo_id", "2");
    _webclient.setHeader("Connection", "keep-alive");
    _webclient.setHeader("Accept", "*/*");
    _webclient.setContentType("text/xml");
}

Tapatalk4x::~Tapatalk4x()
{
    // do nothing
}

//////////////////////////////////////////////////////////////////////////////////
// ParserBase Implmentation

QVariant Tapatalk4x::doLogin(const LoginInfo& info)
{
    loadConfig();

    _loginInfo = info;

	QString username(info.first.toLatin1().toBase64());
	QString password(info.second.toLatin1().toBase64());

	if (_bUseMD5)
	{
		// TODO: MD5 is not working with AMB for some reason
		//password = QCryptographicHash::hash(password.toLatin1(),QCryptographicHash::Md5).toBase64();
	}
	else if (_bUseSha1)
	{
		password = QCryptographicHash::hash(password.toLatin1(),QCryptographicHash::Sha1).toBase64();
	}

	ParamList paramList;
	paramList.append(TapaTalkParam(ParamType::BASE64, username));
	paramList.append(TapaTalkParam(ParamType::BASE64, password));

    // Per the Tapatalk API, the 3rd param indicates an "Anonymous" login
    paramList.append(TapaTalkParam(ParamType::BOOLEAN, QVariant::fromValue(false)));

    // BUG #117: This 4th param is not mentioned in the Tapatalk docs but it is sent by the Tapatalk
    // client. This param and the "tapatalk=1" cookie at login seem to be the key making sure a
    // session stays logged in for over 15 minutes.
    paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(QString("1"))));

    QString strLoginData(getRequestXml("login", paramList));

    // BUG #117: Tapatalk's API docs specifically say not to submit any cookies with the
    // login-request, so we need to explicitly delete any previously set cookies when
    // logging in
    _webclient.deleteAllCookies();

    // The Tapatalk client sends this cookie with each login request
    _webclient.addSendCookie("tapatalk","1");

    QString data = _webclient.UploadString(getBaseUrl(), strLoginData,
        WebClient::NOTIDY |
        WebClient::NOENCRYPT |
        WebClient::NOCACHE);

    // Now we delete the cookie so it doesn't get sent again.
    _webclient.eraseSendCookies();

    StringMap result;
    result.setOrAdd("success", false);

    // for example, a 503 can cause an empty reply, in which case we want to return false, and in the case
    // of an auto-relogin, we will try to re-login in the next request since _lastLogin will not get changed
    if (data.size() > 0)
    {
        XRVariant response2(data);
        if (!response2.canConvert(QVariant::Map))
        {
            OWL_THROW_EXCEPTION(Exception("Cannot convert 'login' response to QVariant::Map"));
        }

        auto loginMap = response2.toMap();


        if (loginMap.contains("result") && loginMap["result"].toBool())
        {
            _lastLogin = QDateTime::currentDateTime();

            result.setOrAdd("success", true);

            if (!_rootIdRealized)
            {
                QString strPostData(getRequestXml("get_forum"));
                const QString ldata = uploadString(strPostData);
                getRootId(ldata);
            }
        }
        else
        {
            if (loginMap.contains("result_text"))
            {
                result.setOrAdd("error", loginMap["result_text"].toString());
            }
        }
    }

	return QVariant::fromValue(result);
}

QVariant Tapatalk4x::doLogout()
{
	QMutexLocker	locker(&_mutex);
	QString			strPostData(getRequestXml("logout_user"));

    uploadString(strPostData);

	// Tapatalk gives no feedback to 'logout_user'
    StringMap result;
	result.add("success", true);

	return QVariant::fromValue(result);
}

QVariant Tapatalk4x::doGetBoardwareInfo()
{
   StringMap result;
    const QString strPostData(getRequestXml("get_config"));
    const QString data = uploadString(strPostData);

    XRVariant response(data);
    if (!response.canConvert(QVariant::Map))
	{
        OWL_THROW_EXCEPTION(Exception("Cannot convert 'get_config' response to QVariant::Map"));
	}

	auto infoMap = response.toMap();
	
	if (infoMap.contains("version"))
	{
		result.add("success", true);
		result.add("boardware", "tapatalk");

        // _version must be set before getForumName() is called else it will fail
        if (infoMap.contains("version"))
        {
            _version = infoMap["version"].toString();
        }

		QString strName(getForumName());
		if (!strName.isEmpty())
		{
			result.add("name", strName);
		}

		// since tapatalk has a bizarre versioning scheme,
		// we have to hardcode an acceptable version number
		result.add("version", (QString)"4.5");
	}
	else
	{
		result.add("success", false);
	}

//	if (!_rootIdRealized)
//	{
//		QString strPostData(getRequestXml("get_forum"));

//        const QString data = uploadString(strPostData);
//		getRootId(data);
//	}

	return QVariant::fromValue(result);
}

QVariant Tapatalk4x::doTestParser(const QString& html)
{
    StringMap result;
	result.add("success", false); // assume failure!

	QDomDocument xmlDoc;
	if (xmlDoc.setContent(html))
	{
		XRVariant infoVar(html);
		if (infoVar.canConvert(QVariant::Map) && infoVar.toMap().contains("version"))
		{
			auto infoMap = infoVar.toMap();
			if (!infoMap.value("version").toString().isEmpty())
			{
				result.setOrAdd("success", true);
			}
		}	
	}

	return QVariant::fromValue(result);
}

QVariant Tapatalk4x::doGetForumList(const QString& forumId)
{
	QMutexLocker locker(&_mutex);
	ForumList retval;

	if (!_forumMapInitialized)
	{
		QString strPostData(getRequestXml("get_forum"));

        const QString data = uploadString(strPostData);
		getRootId(data);

		ForumPtr root = Forum::createRootForum(_rootId);
		_forumMap.insert(_rootId, root);

		XRVariant response(data);
		walkForum(&response);

		_forumMapInitialized = true;
	}

	// This is s hack! The ConfigureBoardDlg called doGetForumList() in order
	// to crawl the board, at which point the default rootId = -1. However,
	// since some boards have a rootId = 0, we need to check it here. This
	// seems like a bad solution but it works.
	QString searchId(forumId);
	if (searchId == "-1" && _rootId != searchId)
	{
		searchId = _rootId;
	}

    if (_forumMap.contains(searchId))
	{
        ForumPtr f = _forumMap[searchId];

        const auto& children = f->getChildren();
        for (const BoardItemPtr& bi : children)
		{
            const ForumPtr cf = bi->upCast<ForumPtr>(false);
			if (cf != nullptr)
			{
				ForumPtr nf(new Forum(*cf));
				retval.append(nf);
			}
		}
	}

	return QVariant::fromValue(retval);
}

QVariant Tapatalk4x::doThreadList(ForumPtr forumInfo, int)
{
	QMutexLocker locker(&_mutex);
	ThreadList retval;

	// tapatalk uses a 0-based index where Owl uses a 1-based index so 
	// compensate for that with: (forumInfo->getPageNumber()-1)
	uint iStart = (forumInfo->getPageNumber()-1) * forumInfo->getPerPage();
	uint iEnd = iStart + forumInfo->getPerPage()-1;

	// load the sticky threads
	ParamList paramList;
	paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(forumInfo->getId())));
	paramList.append(TapaTalkParam(ParamType::INT, QVariant::fromValue(iStart)));
	paramList.append(TapaTalkParam(ParamType::INT, QVariant::fromValue(iEnd)));
	paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(QString("TOP"))));

	QString strPostData(getRequestXml("get_topic", paramList));

    QString data = uploadString(strPostData);
	XRVariant responseData(data);

	if (!responseData.canConvert(QVariant::Map))
	{
        OWL_THROW_EXCEPTION(Exception("Cannot convert 'get_topic' response to QVariant::Map"));
	}

	auto responseMap = responseData.toMap();
	if (responseMap.contains("topics") && responseMap["topics"].canConvert(QVariant::List))
	{
		auto topicList = responseMap["topics"].toList();
		for (auto topic : topicList)
		{
			ThreadPtr newThread = makeThreadObject(&topic);
			if (newThread.get() != nullptr)
			{
				newThread->setParent(forumInfo);
				newThread->setSticky(true);
				retval.push_back(newThread);
			}
		}
	}
    else
	{
		QString strError("Call to 'get_topic' for sticky-threads failed.");

		if (responseMap.contains("result_text"))
		{
			strError.append(QString(" Error: '%1'").arg(responseMap["result_text"].toString()));
		}

        _logger->error(strError.toStdString());
	}

	// load the non-sticky threads
	paramList.removeLast();
	paramList.removeLast();
	paramList.append(TapaTalkParam(ParamType::INT, QVariant::fromValue(iEnd)));

	strPostData.clear();
	strPostData = getRequestXml("get_topic", paramList);

	data.clear();
    data = uploadString(strPostData);

	XRVariant responseData2(data);

	if (!responseData2.canConvert(QVariant::Map))
	{
        OWL_THROW_EXCEPTION(Exception("Cannot convert 'get_topic' response to QVariant::Map"));
	}

	auto responseMap2 = responseData2.toMap();
	if (responseMap2.contains("topics") && responseMap2["topics"].canConvert(QVariant::List))
	{
		auto topicList = responseMap2["topics"].toList();
		for (auto topic : topicList)
		{
			ThreadPtr newThread = makeThreadObject(&topic);
			if (newThread.get() != nullptr)
			{
				newThread->setParent(forumInfo);
				retval.push_back(newThread);
			}
		}
	}
    else
	{
		QString strError("Call to 'get_topic' for threads failed.");

		if (responseMap2.contains("result_text"))
		{
			strError.append(QString(" Error: '%1'").arg(responseMap2["result_text"].toString()));
		}

        _logger->error(strError.toStdString());
	}

	int iTotalTopics = 0;
	if (responseMap.contains("total_topic_num"))
	{
        iTotalTopics = responseMap2["total_topic_num"].toInt();
	}

	int iPageCount = ceil(((double)iTotalTopics) / ((double)forumInfo->getPerPage()));
    if (iPageCount == 0)
    {
        iPageCount = 1;
    }
    
    forumInfo->setPageCount(iPageCount);
    forumInfo->getThreads().clear();
    forumInfo->getThreads().append(retval);
    return QVariant::fromValue(forumInfo);
}

QVariant Tapatalk4x::doPostList(ThreadPtr threadInfo, int)
{
	QMutexLocker locker(&_mutex);
	PostList retval;

	ParamList paramList;
	paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(threadInfo->getId())));
	paramList.append(TapaTalkParam(ParamType::INT, QVariant::fromValue(threadInfo->getPerPage())));
    paramList.append(TapaTalkParam(ParamType::BOOLEAN, QVariant::fromValue(false)));

	QString strPostData(getRequestXml("get_thread_by_unread", paramList));

    const QString data = uploadString(strPostData);
	XRVariant responseData(data);

	if (!responseData.canConvert(QVariant::Map))
	{
        OWL_THROW_EXCEPTION(Exception("Cannot convert 'get_thread_by_unread' response to QVariant::Map"));
	}

	// convert the response map
	auto responseMap = responseData.toMap();

	// we have been handed back the 'position' which is the 1-based index position
	// of the first unread post and 'posts_per_request' which should equal 
	// threadInfo->getPerPage()
	//auto iPerPage = responseMap["posts_per_request"].toUInt();
	auto iPerPage = threadInfo->getPerPage();
	auto iPosition = responseMap["position"].toUInt();
	//Q_ASSERT(iPerPage == threadInfo->getPerPage());

	// calculate the total page count
	int iTotalPosts = responseMap["total_post_num"].toInt();
	uint iPageCount = std::ceil(((double)iTotalPosts) / ((double)threadInfo->getPerPage()));
    if (iPageCount == 0)
    {
        iPageCount = 1;
    }

	// iPosition should be less than (or equal to) the total number of posts in the thread
	Q_ASSERT(iPosition <= iPageCount * threadInfo->getPerPage());
    threadInfo->setPageCount(iPageCount);

	// now calculate which page we are actually on
	int iCurrentPage = ceil(((double)iPosition) / ((double)iPerPage));
    if (iCurrentPage == 0)
    {
        iCurrentPage = 1;
    }
    threadInfo->setPageNumber(iCurrentPage);

	// clear any posts we're storing
	threadInfo->setFirstUnreadPost(PostPtr());
	threadInfo->getPosts().clear();	
	
	if (responseMap.contains("posts") && responseMap["posts"].canConvert(QVariant::List))
	{
		auto iCount = 1 + ((iCurrentPage - 1) * iPerPage);
		auto topicList = responseMap["posts"].toList();
        int index = ((threadInfo->getPageNumber() - 1) * threadInfo->getPerPage())+1;

		for (auto topic : topicList)
		{
			PostPtr newPost = makePostObject(&topic);
			if (newPost.get() != nullptr)
			{
                newPost->setIndex(index++);
				newPost->setParent(threadInfo);

				retval.push_back(newPost);
                if (iCount == (int)iPosition)
				{
					threadInfo->setFirstUnreadPost(newPost);
				}
			}

			iCount++;
		}
	}

	threadInfo->getPosts().clear();
	threadInfo->getPosts().append(retval);
	threadInfo->setPageCount(iPageCount);
	
	return QVariant::fromValue(threadInfo);
}

QVariant Tapatalk4x::doPostList1(ThreadPtr threadInfo, int)
{
	QMutexLocker locker(&_mutex);
	PostList retval;

	// tapatalk uses a 0-based index where Owl uses a 1-based index so 
	// compensate for that with: (threadInfo->getPageNumber()-1)
	uint iStart = (threadInfo->getPageNumber()-1) * threadInfo->getPerPage();

	// TODO: Some times iEnd seems to be more of a 'count' param even though the 
	//		 Tapatalk API says it should be an index. Hence, iStart=0 and iEnd=9 
	//		 will return the first 9 posts instead of the first 10. But this only seems
	//		 to be the case when using this function from the TerminalWindow app.
	uint iEnd = (iStart + threadInfo->getPerPage()) - 1;

	ParamList paramList;
	paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(threadInfo->getId())));
	paramList.append(TapaTalkParam(ParamType::INT, QVariant::fromValue(iStart)));
	paramList.append(TapaTalkParam(ParamType::INT, QVariant::fromValue(iEnd)));
	paramList.append(TapaTalkParam(ParamType::BOOLEAN, QVariant::fromValue(false)));

	QString strPostData(getRequestXml("get_thread", paramList));

    const QString data = uploadString(strPostData);
	XRVariant responseData(data);

	if (!responseData.canConvert(QVariant::Map))
	{
        OWL_THROW_EXCEPTION(Exception("Cannot convert 'get_thread' response to QVariant::Map"));
	}

	auto responseMap = responseData.toMap();
	if (responseMap.contains("posts") && responseMap["posts"].canConvert(QVariant::List))
	{
		auto topicList = responseMap["posts"].toList();
		for (auto topic : topicList)
		{
			PostPtr newPost = makePostObject(&topic);
			newPost->setParent(threadInfo);
			if (newPost.get() != nullptr)
			{
				retval.push_back(newPost);
			}
		}
	}

	int iTotalTopics = 0;
	if (responseMap.contains("total_post_num"))
	{
		iTotalTopics = responseMap["total_post_num"].toInt();
	}

	int iPageCount = ceil(((double)iTotalTopics) / ((double)threadInfo->getPerPage()));

	threadInfo->getPosts().clear();
	threadInfo->getPosts().append(retval);
	threadInfo->setPageCount(iPageCount);
	
	return QVariant::fromValue(threadInfo);
}

QVariant Tapatalk4x::doGetPostList(ThreadPtr t, PostListOptions listOption, int webOptions)
{
	QMutexLocker locker(&_mutex);

	if (listOption == PostListOptions::FIRST_UNREAD)
	{
		return doPostList(t, webOptions);
	}
	else if (listOption == PostListOptions::FIRST_POST)
	{
		return doPostList1(t, webOptions);
	}
	else if (listOption == PostListOptions::LAST_POST)
	{
		OWL_THROW_EXCEPTION(Exception("Not implemented"));
	}

	return QVariant::fromValue(t);
}

QVariant Tapatalk4x::doSubmitNewThread(ThreadPtr threadInfo)
{
	QMutexLocker locker(&_mutex);
	QString strTemp;

	ParamList paramList;
	strTemp = threadInfo->getParent()->getId();
	paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(strTemp)));
	
	strTemp = threadInfo->getTitle().toLatin1().toBase64();
	paramList.append(TapaTalkParam(ParamType::BASE64, QVariant::fromValue(strTemp)));

	strTemp = threadInfo->getPosts().at(0)->getText().toLatin1().toBase64();
	paramList.append(TapaTalkParam(ParamType::BASE64, QVariant::fromValue(strTemp)));

    QString strNewThreadData(getRequestXml("new_topic", paramList));

    const QString data = uploadString(strNewThreadData);
	XRVariant response(data);

	if (!response.canConvert(QVariant::Map))
	{
        OWL_THROW_EXCEPTION(Exception("Cannot convert 'new_topic' response to QVariant::Map"));
	}

	ThreadPtr ret;
	auto responseMap = response.toMap();
	
	if (responseMap.contains("result") && responseMap["result"].toBool())
	{
		ret.reset();
		QString newId(responseMap["topic_id"].toString());
		ret = ThreadPtr(new Thread(newId));
	}
    else
	{
		if (responseMap.contains("result_text"))
		{
            _logger->error("No threadId returned from doSubmitNewThread(): Error '{}'",
                responseMap["result_text"].toString().toStdString());
		}
		else
		{
            _logger->error("No threadId returned from doSubmitNewThread()");
		}
	}
	
	return QVariant::fromValue(ret);
}

QVariant Tapatalk4x::doSubmitNewPost(PostPtr postInfo)
{
	QMutexLocker locker(&_mutex);
	QString strTemp;

	ParamList paramList;

	// get the forumId
	ThreadPtr threadInfo = postInfo->getParent()->upCast<ThreadPtr>();
	strTemp = threadInfo->getParent()->getId();
	paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(strTemp)));

	strTemp = postInfo->getParent()->getId();
	paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(strTemp)));

	strTemp = postInfo->getTitle().toLatin1().toBase64();
	paramList.append(TapaTalkParam(ParamType::BASE64, QVariant::fromValue(strTemp)));

	strTemp = postInfo->getText().toLatin1().toBase64();
	paramList.append(TapaTalkParam(ParamType::BASE64, QVariant::fromValue(strTemp)));

    const QString strNewPostData(getRequestXml("reply_post", paramList));
    const QString data = uploadString(strNewPostData);
	XRVariant response(data);

	if (!response.canConvert(QVariant::Map))
	{
        OWL_THROW_EXCEPTION(Exception("Cannot convert 'reply_post' response to QVariant::Map"));
	}

	PostPtr retPost;
	auto responseMap = response.toMap();

	if (responseMap.contains("result") && responseMap["result"].toBool())
	{
		retPost.reset();
		QString newId(responseMap["post_id"].toString());
		
		retPost = PostPtr(new Post(newId));
		postInfo->getParent()->addChild(retPost);
	}
    else
	{
		if (responseMap.contains("result_text"))
		{
            const auto errorText = responseMap["result_text"].toString();
            _logger->error("Tapatalk response had a blank or false result with error: '{}'", errorText.toStdString());
            OWL_THROW_EXCEPTION(Exception(errorText));
		}
		else
		{
            _logger->error("Tapatalk response had a blank or false result with no error text");
            OWL_THROW_EXCEPTION(Exception(tr("There was an unkonwn error submitting the post.")));
		}
	}

	return QVariant::fromValue(retPost);
}

QVariant Tapatalk4x::doGetEncryptionSettings()
{
	return QVariant();
}

QVariant Tapatalk4x::doMarkForumRead(ForumPtr forumInfo)
{
	QMutexLocker locker(&_mutex);

	ParamList paramList;

	if (forumInfo->getId() != getRootForumId())
	{
		paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(forumInfo->getId())));
	}

    const QString strPostData(getRequestXml("mark_all_as_read", paramList));
    const QString data = uploadString(strPostData);
	XRVariant response(data);

	if (!response.canConvert(QVariant::Map))
	{
        OWL_THROW_EXCEPTION(Exception("Cannot parse response for 'doMarkForumRead'"));
	}

	auto map = response.toMap();
	if (!map.contains("result") || !map["result"].toBool())
	{
		QString strError("Cannot parse response for 'doMarkForumRead'");

		if (map.contains("result_text") && !map["result_text"].toString().isEmpty())
		{
			strError += QString(": %1").arg(map["result_text"].toString());
		}

        OWL_THROW_EXCEPTION(Exception(strError));
	}

	return QVariant::fromValue(forumInfo);
}

QVariant Tapatalk4x::doGetUnreadForums()
{
	QMutexLocker locker(&_mutex);
	QHash<QString, ForumPtr> unreadHash;
	ForumList retval;

	ParamList paramList;
	paramList.append(TapaTalkParam(ParamType::INT, QVariant::fromValue(0)));
	paramList.append(TapaTalkParam(ParamType::INT, QVariant::fromValue(50)));

    const QString strPostData(getRequestXml("get_unread_topic", paramList));
    const QString data = uploadString(strPostData);
	XRVariant response(data);

	if (!response.canConvert(QVariant::Map))
	{
        OWL_THROW_EXCEPTION(Exception("Cannot convert response for 'get_unread_topic' to QVariant::Map"));
	}	

	auto map = response.toMap();
	if (map.contains("topics"))
	{
		if (!map["topics"].canConvert(QVariant::List))
		{
            OWL_THROW_EXCEPTION(Exception("Cannot convert response for 'get_unread_topic.topics' to QVariant::List"));
		}

		auto topicList = map["topics"].toList();
		for (auto topic : topicList)
		{
			if (!topic.canConvert(QVariant::Map))
			{
                OWL_THROW_EXCEPTION(Exception("Cannot convert response for 'get_unread_topic.topics.topic' to QVariant::Map"));
			}

			auto topicMap = topic.toMap();
			QString strId = topicMap["forum_id"].toString();
			if (!unreadHash.contains(strId))
			{
				ForumPtr f = makeForumObject(&topic);
				if (f.get() != nullptr)
				{
					unreadHash.insert(strId, f);
				}
			}
		}
	}
    else
	{
		QString strError("Call to 'get_unread_topic' failed.");

		if (map.contains("result_text"))
		{
			strError.append(QString(" Error: '%1'").arg(map["result_text"].toString()));
		}

        _logger->error(strError.toStdString());
	}

	QHashIterator<QString, ForumPtr> it(unreadHash);

	while (it.hasNext())
	{
		it.next();
		retval.push_back(it.value());
	}

	return QVariant::fromValue(retval);
}

// /ParserBase Implmentation
//////////////////////////////////////////////////////////////////////////////////

QString Tapatalk4x::getPostQuote(PostPtr postinfo)
{
    QMutexLocker locker(&_mutex);
    QString retval;

    ParamList paramList;
    paramList.append(TapaTalkParam(ParamType::STRING, QVariant::fromValue(postinfo->getId())));

    const QString strPostData(getRequestXml("get_quote_post", paramList));
    const QString data = uploadString(strPostData);
    XRVariant responseData(data);

    if (responseData.canConvert(QVariant::Map))
    {
        const auto responsemap = responseData.toMap();
        if (responsemap.find("post_content") != responsemap.end())
        {
            retval = QString("%1\n\n")
                .arg(responsemap["post_content"].toString());
        }
        else
        {
            _logger->debug("Failed to get post quote for post '{}' because: '{}'",
                postinfo->getId().toStdString(),
                responsemap["result_text"].toString().toStdString());
        }
    }

    if (retval.isEmpty())
    {
        retval = ParserBase::getPostQuote(postinfo);
    }

    return retval;
}

void Tapatalk4x::walkForum( QVariant* variant )
{
	if (variant->canConvert(QVariant::List))
	{
		auto iDisplayOrder = 0;
		auto rootList = variant->toList();
		for (auto fItem : rootList)
		{
			ForumPtr newForum = makeForumObject(&fItem);
			if (newForum.get() != nullptr)
			{
				newForum->setDisplayOrder(++iDisplayOrder);
	
				_forumMap.insert(newForum->getId(), newForum);

				auto strParentId = fItem.toMap().value("parent_id").toString();
				if (_forumMap.contains(strParentId))
				{
					_forumMap[strParentId]->addChild(newForum);
				}

				auto childVariant = fItem.toMap().value("child");
				walkForum(&childVariant);
			}
		}
	}
}

QString Tapatalk4x::getRequestXml(const QString& methodName, ParamList params)
{
	if (methodName.isEmpty())
	{
        OWL_THROW_EXCEPTION(Exception("Invalid arugment, methodName cannot be empty"));
	}

	QString retXml;
	QXmlStreamWriter writer(&retXml);
	writer.setAutoFormatting(true);

	writer.writeStartDocument("1.0");
	writer.writeStartElement("methodCall");
	writer.writeTextElement("methodName", methodName);

	QString strRet = 
		QString("<?xml version=\"1.0\"?><methodCall><methodName>%1</methodName>")
		.arg(methodName);

	if (params.size() > 0)
	{
		writer.writeStartElement("params");

		for (TapaTalkParam param : params)
		{
			writer.writeStartElement("param");

			if (!param.name.isEmpty())
			{
				writer.writeTextElement("name", param.name);
			}

			writer.writeStartElement("value");

			QString typeStr;
			QString strValue = param.value.toString();
			if (param.type == ParamType::BASE64)
			{
				typeStr = "base64";
			}
			else if (param.type == ParamType::STRING)
			{
				typeStr = "string";
			}
			else if (param.type == ParamType::BOOLEAN)
			{
				typeStr = "boolean";
				if (param.value.toBool())
				{
                    strValue = "true";
				}
				else
				{
                    strValue = "false";
				}
			}
			else if (param.type == ParamType::INT)
			{
				typeStr = "int";
				strValue = QString::number(param.value.toInt());
			}

			if (typeStr.isEmpty())
			{
				QString strError = QString("Unknown Tapatalk param type: '%1'").arg(param.type);
                OWL_THROW_EXCEPTION(Exception(strError));
			}

			writer.writeTextElement(typeStr, strValue);

			writer.writeEndElement();
			writer.writeEndElement();
		}

		writer.writeEndElement();
	}

	writer.writeEndElement();
	writer.writeEndDocument();

    return retXml;
}

const QString Tapatalk4x::uploadString(const QString& payload)
{
    if (_lastLogin.secsTo(QDateTime::currentDateTime()) >= Tapatalk4x::LOGINTIMEOUT)
    {
        doLogin(_loginInfo);
    }

    return _webclient.UploadString(getBaseUrl(), payload,
        WebClient::NOTIDY |
        WebClient::NOENCRYPT |
        WebClient::NOCACHE);
}

owl::ForumPtr Tapatalk4x::makeForumObject( QVariant* variant )
{
	ForumPtr newForum;

	if (variant->canConvert(QVariant::Map))
	{
		auto forumMap = variant->toMap();

		newForum = ForumPtr(new Forum(forumMap["forum_id"].toString()));
		newForum->setName(forumMap["forum_name"].toString());
		newForum->setHasUnread(forumMap["new_post"].toBool());

		if (forumMap["sub_only"].toBool())
		{
			newForum->setForumType(Forum::CATEGORY);
		}
		else
		{
			if (forumMap.contains("url") && !forumMap["url"].toString().isEmpty())
			{
				newForum->setForumType(Forum::LINK);
				newForum->setVar("link", forumMap["url"].toString());
			}
			else
			{
				newForum->setForumType(Forum::FORUM);
			}
		}
	}

	return newForum;
}

owl::ThreadPtr Tapatalk4x::makeThreadObject( QVariant* variant )
{
	ThreadPtr newThread;

	if (variant->canConvert(QVariant::Map))
	{
		auto topicMap = variant->toMap();
		
		newThread = ThreadPtr(new Thread(topicMap["topic_id"].toString()));
		newThread->setTitle(topicMap["topic_title"].toString());
		newThread->setAuthor(topicMap["topic_author_name"].toString());
		newThread->setPreviewText(topicMap["short_content"].toString());
		newThread->setHasUnread(topicMap["new_post"].toBool());
        newThread->setIconUrl(topicMap["icon_url"].toString());
        newThread->setViews(topicMap["view_number"].toUInt());
        newThread->setOpen(!topicMap["is_closed"].toBool());

		// construct the last post
		// TODO: need to figure out how to get the LAST postId in the thread
		PostPtr post(new Post("-1"));

		if (topicMap.contains("last_reply_user"))
		{
			post->setAuthor(topicMap["last_reply_user"].toString());
		}
		else if (topicMap.contains("last_reply_author_name"))
		{
			post->setAuthor(topicMap["last_reply_author_name"].toString());
		}

        if (topicMap.contains("reply_number"))
        {
            bool bok = false;
            const auto replycount = topicMap["reply_number"].toString().replace(",", QString()).toUInt(&bok);
            if (bok)
            {
                newThread->setReplyCount(replycount);
            }
        }

		if (QDateTime dt = topicMap["last_reply_time"].toDateTime(); dt.isValid())
		{
			QString strTime = dt.toString("MM-dd-yyyy hh:mm AP");
			post->setDatelineString(strTime);
			post->setDateTime(dt);
		}
        else if (auto dt = topicMap["post_time"].toDateTime(); dt.isValid())
        {
            QString strTime = dt.toString("MM-dd-yyyy hh:mm AP");
            post->setDatelineString(strTime);
            post->setDateTime(dt);
        }
        else
        {
            auto epochTS = topicMap["timestamp"].toUInt();
            auto dt2 = QDateTime::fromSecsSinceEpoch(epochTS);
            QString strTime = dt2.toString("MM-dd-yyyy hh:mm AP");
            post->setDatelineString(strTime);
            post->setDateTime(dt2);
        }

		newThread->setLastPost(post);
	}

	return newThread;
}

owl::PostPtr Tapatalk4x::makePostObject( QVariant* variant )
{
	PostPtr newPost;

	if (variant->canConvert(QVariant::Map))
	{
		auto postMap = variant->toMap();

		newPost = PostPtr(new Post(postMap["post_id"].toString()));
		newPost->setText(postMap["post_content"].toString());
		newPost->setAuthor(postMap["post_author_name"].toString());
        newPost->setIconUrl(postMap["icon_url"].toString());

		int unixTime = postMap["timestamp"].toInt();
		if (unixTime > 0)
		{
			QDateTime dateTime;
			dateTime.setTime_t(unixTime);
			QString strTime = dateTime.toString("MM-dd-yyyy hh:mm AP");
			newPost->setDatelineString(strTime);
			newPost->setDateTime(dateTime);
		}
	}

	return newPost;
}

void Tapatalk4x::getRootId(QString data)
{
	if (_rootIdRealized)
	{
		return;
	}

	QString strId(-1);
	XRVariant response(data);

	if (!response.canConvert(QVariant::List))
	{
        _logger->error("Failed to get root ID. The response could not be converted to a List.");
		return;
	}

	auto rootForumMap = response.toList();
	if (rootForumMap.size() == 0)
	{
        _logger->error("Failed to get root ID. No forums were returned in the resulting List.");
		return;
	}

	// get the first child forum to determin the parentId which will 
	// give us the rootId
	XRVariant firstChild = rootForumMap.at(0);
	if (!firstChild.canConvert(QVariant::Map))
	{
        _logger->error("Failed to get root ID. First item in List could not be converted to Map.");
		return;
	}

	auto firstChildMap = firstChild.toMap();
	if (firstChildMap.contains("parent_id"))
	{
		_rootId = firstChildMap["parent_id"].toString();
		_rootIdRealized = true;
		return;
	}
    else
    {
        _logger->error("Failed to get root ID. Forum Map does not contain a 'parent_id'.");
    }

	return;
}

void Tapatalk4x::loadConfig()
{
	if (!_configLoaded)
	{
        const QString strPostData(getRequestXml("get_config"));
        const QString data = uploadString(strPostData);
		XRVariant response(data);

		if (!response.canConvert(QVariant::Map))
		{
            OWL_THROW_EXCEPTION(Exception("Cannot convert 'get_config' response to QVariant::Map"));
		}

		auto map = response.toMap();

		if (map.contains("support_md5"))
		{
			_bUseMD5 = map["support_md5"].toBool();
		}

		if (map.contains("support_sha1"))
		{
			_bUseSha1 = map["support_sha1"].toBool();
		}

		if (map.contains("api_level"))
		{
			_apiLevel = map["api_level"].toInt();
		}

		if (map.contains("version"))
		{
			_version = map["version"].toString();
		}

		_configLoaded = true;
	}
}

QString Tapatalk4x::getForumName()
{
	QString retval(_forumName);

	if (_forumName.isEmpty())
	{
		QUrl tempUrl(getBaseUrl());
		retval = tempUrl.host();	
	
		if (!_version.isEmpty())
		{
			// get the board's native url
			QString strtempUrl = getBaseUrl();
            strtempUrl = strtempUrl.replace(QRegularExpression("/.[^\\/]*?/mobiquo.php$"), QString());

			if (_version.startsWith("vb40", Qt::CaseInsensitive))
			{
				// we have to use a new WebClient object so that the cookies
				// in _webClient don't get reset
                WebClient client;
				QString pageSrc = client.DownloadString(strtempUrl);

				QSgml pageDoc;
				if (!pageSrc.isEmpty() && pageDoc.parse(pageSrc))
				{
					QList<QSgmlTag*> tempList;

					QRegExp vbr("\\s+\\-\\s*Powered\\s+by\\s+vBulletin");
					vbr.setCaseSensitivity(Qt::CaseInsensitive);

					pageDoc.getElementsByName("img", "alt", vbr, &tempList);
					if (tempList.size() > 0)
					{
						QSgmlTag* altTag = tempList.at(0);
						if (altTag != nullptr
							&& altTag->Attributes.contains("alt")
							&& !altTag->Attributes.value("alt").isEmpty())
						{
							QString temp(altTag->Attributes.value("alt").trimmed());
							retval = temp.replace(vbr, "");
						}
					}
				}
			}
			else
			{
				QString trimmer;

				if (_version.startsWith("vb3x", Qt::CaseInsensitive))
				{
					trimmer = " - Powered by vBulletin";
				}
				else if (_version.startsWith("sm-", Qt::CaseInsensitive))
				{
					 trimmer = " - Index";
				}
				//else if (_version.startsWith("xf1", Qt::CaseInsensitive)
				//	|| _version.startsWith("pb30", Qt::CaseInsensitive))

				// we have to use a new WebClient object so that the cookies
				// in _webClient don't get reset
                WebClient client;
				QString pageSrc = client.DownloadString(strtempUrl);

				QSgml pageDoc;
				if (!pageSrc.isEmpty() && pageDoc.parse(pageSrc))
				{
					QList<QSgmlTag*> tempList;
					pageDoc.getElementsByName("title", &tempList);

                    if (tempList.size() > 0 && tempList.at(0)->Children.size() > 0)
					{
						QString title(tempList.at(0)->Children[0]->Value);

						if (!trimmer.isEmpty())
						{
							title = title.replace(trimmer, "").trimmed();
						}
						else
						{
							title = title.trimmed();
						}

						if (!title.isEmpty())
						{
							retval = title;
						}
					}
				}
			}
		}

		_forumName = retval;
	}

	return retval;
}

} // namespace
