#pragma once
#include <QObject>
#include <mutex>

#include <boost/signals2.hpp>

#ifdef _WINDOWS
#else
#include "terminalosx.h"
#endif

namespace owl
{

class Terminal final : public QObject
{
    Q_OBJECT

    std::string _commandline;
    bool        _echo = true;

public:
    Terminal();
    virtual ~Terminal();

    [[nodiscard]]
    std::string getLine();
    void setLine(const std::string& v) { _commandline = v; }

    void backspace();
    void backspaces(std::size_t spaces)
    {
        for (auto i = 0u; i < spaces; i++)
        {
            backspace();
        }
    }

    void clearLine()
    {
        backspaces(_commandline.size());
        _commandline.clear();
    }

    void reset()
    {
        _commandline.clear();
    }

    void setEcho(bool val) { _echo = val; }

    boost::signals2::signal<bool(char)> onChar2;
    boost::signals2::signal<bool()> onBackspace2;
    boost::signals2::signal<bool()> onEnter2;

    boost::signals2::signal<void()> onUpArrow;
    boost::signals2::signal<void()> onDownArrow;
    boost::signals2::signal<void()> onLeftArrow;
    boost::signals2::signal<void()> onRightArrow;

    boost::signals2::signal<void()> onHome;
    boost::signals2::signal<void()> onEnd;

    boost::signals2::signal<bool()> onClearLine;
    boost::signals2::signal<void()> onDeleteWord;

// START OLD CODE
public:
    bool getEcho() const { return _bEcho; }

    void setPrompt(bool b) { _bPrompt = b; }
    bool isPrompt() const { return _bPrompt; }

    void run();

Q_SIGNALS:
    void onChar(QChar c);
    void onBackspace();
    bool onEnter();

private:
    bool _bEcho = true;
    bool _bPrompt = false;
    std::pair<bool, char> getChar();
// END OLD CODE
};

} // namespace
