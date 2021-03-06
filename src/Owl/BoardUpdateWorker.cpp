#include "Data/Board.h"
#include "Data/BoardManager.h"
#include <Parsers/Forum.h>
#include "BoardUpdateWorker.h"

#include <Utils/OwlLogger.h>

namespace owl
{

class UpdaterMutexTryLocker
{
    QMutex&		_m;
    bool		_locked;

public:
    UpdaterMutexTryLocker(QMutex &m)
        : _m(m),
        _locked(m.tryLock())
    {
        // do nothing
    }

    virtual ~UpdaterMutexTryLocker()
    {
        if (_locked)
        {
            _m.unlock();
        }
    }

    UpdaterMutexTryLocker(const UpdaterMutexTryLocker&) = delete;

    bool isLocked() const
    {
        return _locked;
    }
};

BoardUpdateWorker::BoardUpdateWorker(BoardPtr board)
    : _board(board),
      _logger(owl::initializeLogger("BoardUpdateWorker"))
{
}

void BoardUpdateWorker::doWork()
{
    if (_isDeleted)
    {
        return;
    }

    // default to a hardcoded 10 minute refresh rate in case something
    // goes wrong
    std::uint32_t refreshRate = 1000 * 60 * 60;

    UpdaterMutexTryLocker locker(_mutex);
    if (locker.isLocked())
    {
        BoardPtr board = _board.lock();
        if (!board) return;

        try
        {
            const std::string boardName { board->getName().toStdString() };

            _logger->debug("BoardUpdateWorker::doWork() for board '{}' started", boardName);

            board->updateUnread();

            _logger->debug("BoardUpdateWorker::doWork() for board '{}' completed", boardName);

//            checkStructureUpdate();
        }
        catch (const WebException& ex)
        {
            _logger->error("Error during BoardUpdateWorker::doWork(): {}", ex.message().toStdString());
        }

        refreshRate = 1000 * board->getOptions()->get<std::uint32_t>("refreshRate");
    }

    QTimer::singleShot(refreshRate, [this]() { this->doWork(); });
}

//void BoardUpdateWorker::checkStructureUpdate()
//{
//    if (_isDeleted)
//    {
//        return;
//    }

//    // how long between each structure check (in seconds)
//    const uint iRefreshPeriod = 60 * 60 * 24; // one day

//    QDateTime boardTime = _board->getLastUpdate();

//    _logger->trace("Board {}({}) - last update was {}",
//        _board->getName().toStdString(), _board->getDBId(), boardTime.toString().toStdString());

//    if (boardTime.secsTo(QDateTime::currentDateTime()) >= iRefreshPeriod)
//    {
//        _logger->debug("Board {}({}) - verifying forum structure",
//            _board->getName().toStdString(), _board->getDBId());

//        BoardPtr savedBoard = BOARDMANAGER->getBoardInfo(_board->getDBId());
//        ForumPtr savedRoot = savedBoard->getRoot();

//        if (savedRoot != nullptr)
//        {
//            // update the Board::lastUpdate member
//            ForumPtr root = _board->getRootStructure(false);
//            if (root != nullptr)
//            {
//                if (_board->getRoot()->isStructureEqual(root))
//                {
//                    _logger->trace("Board {}({}) - stored structure and online structure are the same",
//                        _board->getName().toStdString(), _board->getDBId());
//                }
//                else
//                {
//                    _logger->trace("Board {}({}) - stored structure and online structure are NOT the same",
//                        _board->getName().toStdString(), _board->getDBId());

//                    Q_EMIT onForumStructureChanged(_board);
//                }

//                _board->setLastUpdate(QDateTime::currentDateTime());
//                BOARDMANAGER->updateBoard(_board);
//            }
//            else
//            {
//                _logger->warn("Board {}({}) - getRootStructure() returned a 'nullptr' root",
//                    _board->getName().toStdString(), _board->getDBId());
//            }
//        }
//        else
//        {
//            _logger->warn("Board {}({}) - getBoardInfo(), getRoot() returned a 'nullptr' root",
//                _board->getName().toStdString(), _board->getDBId());
//        }
//    }
//}

}
