#ifndef REDLINE_EMACSMODE_HPP_INCLUDED
#define REDLINE_EMACSMODE_HPP_INCLUDED

#include "redline/forward-decls.hpp"

#include <set>
#include <string>
#include <utility>

#include "redline/mode.hpp"

namespace Redline
{
  class EmacsMode : public Mode
  {
  public:
    EmacsMode(Editor &editor);
    ~EmacsMode();

    //! Mode interface.
  public:
    virtual const Command *GetHandler(const KeyCombination &keys);
    virtual void Render(Terminal &terminal);

    //! Editing interface.
  public:
    Text &GetText();
    Cursor GetCursor() const;
    void SetCursor(const Cursor &cursor);
    void Render(DecoratedText &text, int &row, int &col);
    void SetHintText(const std::string &text);

    //! Commands.
  public:
    bool HistoryPrevious();
    bool HistoryNext();

    bool SetHistoryPosition(HistoryCursor pos);
    void SetHistoryPositionToEnd();
    HistoryCursor GetHistoryPosition();

    typedef std::pair<std::string, std::string> Completion;
    typedef std::set<Completion> Completions;
    virtual void GetCompletions(Completions &matches);

    virtual void TabComplete();
    bool AcceptLine();
    // Guts of AcceptLine. Set up state and call Execute.
    void DoExecute(const std::string &text, void *arg = 0);

    void AsyncExecute(const std::string &command, void *arg = 0);

    //! Interface for derived classes to implement.
  public:
    virtual bool TextIsComplete();
    virtual void Execute(const std::string &text, void *arg = 0);
    virtual std::string GetPrompt(int line);

    virtual History *GetHistory();

  private:
    class Internals;
    Internals *internals;
  };
}

#endif
