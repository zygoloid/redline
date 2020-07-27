#include "redline/emacs.hpp"

//! For SIGINT
//@{
#include <signal.h>
//@}

#include <map>
#include <memory>
#include <sstream>

#include "redline/bindings.hpp"
#include "redline/command.hpp"
#include "redline/editor.hpp"
#include "redline/history.hpp"
#include "redline/terminal.hpp"
#include "redline/text.hpp"

using namespace Redline;

namespace
{
  KeyBindings bindings;

  bool IsPrintable(Key key)
  {
    return key < 0x80 && isprint(key);
  }

  Cursor WordLeft(Cursor c)
  {
    // 1. Skip whitespace.
    while (c.GetLeft() && isspace(c.GetLeft())) { c = c.Move(-1, 0); }
    // 2. Skip non-whitespace.
    while (c.GetLeft() && !isspace(c.GetLeft())) { c = c.Move(-1, 0); }
    // 3. Done.
    return c;
  }

  Cursor WordRight(Cursor c)
  {
    // 1. Skip non-whitespace.
    while (c.GetRight() && !isspace(c.GetRight())) { c = c.Move(1, 0); }
    // 2. Skip whitespace.
    while (c.GetRight() && isspace(c.GetRight())) { c = c.Move(1, 0); }
    // 3. Done.
    return c;
  }

  //! insert-* : add characters at cursor.
  //@{
  void InsertChar(EmacsMode &mode, const KeyCombination &keys)
  {
    Text &t = mode.GetText();
    const std::vector<Key> &v = keys.GetKeys();
    std::string add(v.begin(), v.end());
    t.Insert(InsertLeft, mode.GetCursor(), add);
  }
  ModeCommand<EmacsMode> insertChar("insert-char", Command::WantKeys, InsertChar, bindings);

  void InsertCode(EmacsMode &mode, const KeyCombination &keys)
  {
    const std::vector<Key> &v = keys.GetKeys();
    std::ostringstream oss;
    oss << v[0];
    mode.GetText().Insert(InsertLeft, mode.GetCursor(), oss.str());
  }
  ModeCommand<EmacsMode> insertCode("insert-code", Command::WantKeys, InsertCode, bindings);

  void InsertNewline(EmacsMode &mode) { mode.GetText().Insert(InsertLeft, mode.GetCursor(), "\n"); }
  ModeCommand<EmacsMode> insertNewline("insert-newline", InsertNewline, bindings, Keys::Alt + Keys::Enter, Keys::Ctrl + Keys::Alt + 'M', Keys::Ctrl + Keys::Alt + 'J');
  //@}


  //! cursor-* : move cursor.
  //@{
  void CursorLeft(EmacsMode &mode) { mode.SetCursor(mode.GetCursor().Move(-1, 0)); }
  ModeCommand<EmacsMode> cursorLeft("cursor-left", CursorLeft, bindings, Keys::Left, Keys::Ctrl + 'B');
  void CursorRight(EmacsMode &mode) { mode.SetCursor(mode.GetCursor().Move(1, 0)); }
  ModeCommand<EmacsMode> cursorRight("cursor-right", CursorRight, bindings, Keys::Right, Keys::Ctrl + 'F');
  void CursorUp(EmacsMode &mode) { mode.SetCursor(mode.GetCursor().Move(0, -1)); }
  ModeCommand<EmacsMode> cursorUp("cursor-up", CursorUp, bindings, Keys::Alt + Keys::Up);
  void CursorDown(EmacsMode &mode) { mode.SetCursor(mode.GetCursor().Move(0, 1)); }
  ModeCommand<EmacsMode> cursorDown("cursor-down", CursorDown, bindings, Keys::Alt + Keys::Down);

  void CursorWordLeft(EmacsMode &mode) { mode.SetCursor(WordLeft(mode.GetCursor())); }
  ModeCommand<EmacsMode> cursorWordLeft("cursor-word-left", CursorWordLeft, bindings, Keys::Ctrl + Keys::Left, Keys::Alt + Keys::Left, Keys::Alt + 'b');
  void CursorWordRight(EmacsMode &mode) { mode.SetCursor(WordRight(mode.GetCursor())); }
  ModeCommand<EmacsMode> cursorWordRight("cursor-word-right", CursorWordRight, bindings, Keys::Ctrl + Keys::Right, Keys::Alt + Keys::Right, Keys::Alt + 'f');

  void CursorUpOrHistoryPrevious(EmacsMode &mode)
  {
    Cursor newCursor = mode.GetCursor().Move(0, -1);
    if (newCursor != mode.GetCursor())
    {
      mode.SetCursor(newCursor);
    }
    else
    {
      mode.HistoryPrevious();
    }
  }
  ModeCommand<EmacsMode> cursorUpOrHistoryPrevious("cursor-up-or-history-previous", CursorUpOrHistoryPrevious, bindings, Keys::Up);
  void CursorDownOrHistoryNext(EmacsMode &mode)
  {
    Cursor newCursor = mode.GetCursor().Move(0, 1);
    if (newCursor != mode.GetCursor())
    {
      mode.SetCursor(newCursor);
    }
    else
    {
      mode.HistoryNext();
    }
  }
  ModeCommand<EmacsMode> cursorDownOrHistoryNext("cursor-down-or-history-next", CursorDownOrHistoryNext, bindings, Keys::Down);

  // Go to start / end of current line. If already there, go to start / end of prev / next line.
  void CursorHome(EmacsMode &mode) { mode.SetCursor(mode.GetText().Begin(mode.GetCursor().Move(-1, 0).GetLine())); }
  ModeCommand<EmacsMode> cursorHome("cursor-home", CursorHome, bindings, Keys::Ctrl + 'A', Keys::Home);
  void CursorEnd(EmacsMode &mode) { mode.SetCursor(mode.GetText().End(mode.GetCursor().Move(1, 0).GetLine())); }
  ModeCommand<EmacsMode> cursorEnd("cursor-end", CursorEnd, bindings, Keys::Ctrl + 'E', Keys::End);
  //@}

  
  //! delete-* : delete characters near cursor.
  //@{
  void DeleteLeft(EmacsMode &mode) { mode.GetText().Delete(mode.GetCursor().Move(-1, 0), mode.GetCursor()); }
  ModeCommand<EmacsMode> deleteLeft("delete-left", DeleteLeft, bindings, Keys::Backspace, Keys::Ctrl + 'H');
  void DeleteRight(EmacsMode &mode) { mode.GetText().Delete(mode.GetCursor(), mode.GetCursor().Move(1, 0)); }
  ModeCommand<EmacsMode> deleteRight("delete-right", DeleteRight, bindings, Keys::Delete);
  void DeleteToEnd(EmacsMode &mode)
  {
    if (mode.GetCursor().GetLine() == mode.GetCursor().Move(1, 0).GetLine())
    {
      mode.GetText().Delete(mode.GetCursor(), mode.GetText().End(mode.GetCursor().GetLine()));
    }
    else
    {
      mode.GetText().Delete(mode.GetCursor(), mode.GetText().Begin(mode.GetCursor().GetLine() + 1));
    }
  }
  ModeCommand<EmacsMode> deleteToEnd("delete-end", DeleteToEnd, bindings, Keys::Ctrl + 'K');
  void DeleteLine(EmacsMode &mode)
  {
    int line = mode.GetCursor().GetLine();
    if (mode.GetText().Begin(line) == mode.GetText().Begin(line + 1))
    {
      --line;
    }
    mode.GetText().Delete(mode.GetText().Begin(line), mode.GetText().Begin(line+1));
  }
  ModeCommand<EmacsMode> deleteLine("delete-line", DeleteLine, bindings, Keys::Ctrl + 'U');
  void DeleteRightOrEndMode(EmacsMode &mode)
  {
    if (mode.GetText().Begin() == mode.GetText().End())
    {
      //mode.GetEditor().EndMode();
      delete &mode;
    }
    else
    {
      DeleteRight(mode);
    }
  }
  ModeCommand<EmacsMode> deleteRightOrEndMode("delete-right-or-end-mode", DeleteRightOrEndMode, bindings, Keys::Ctrl + 'D', Keys::Eof);

  void DeleteWordLeft(EmacsMode &mode) { mode.GetText().Delete(WordLeft(mode.GetCursor()), mode.GetCursor()); }
  ModeCommand<EmacsMode> deleteWordLeft("delete-word-left", DeleteWordLeft, bindings, Keys::Ctrl + 'W');
  //@}
  
  void Undo(EmacsMode &mode)
  {
    // TODO
  }
  ModeCommand<EmacsMode> undo("undo", Undo, bindings, Keys::Ctrl + '_');

  void CancelOrSigInt(EmacsMode &mode)
  {
    Editor &editor = mode.GetEditor();
    if (Terminal *t = editor.GetTerminal())
    {
      if (mode.GetText().Begin() == mode.GetText().End())
      {
        t->Commit(/*addNewline=*/false);

        SuspendTerminal suspend(*t);
        kill(-tcgetpgrp(0), SIGINT);
        // Simulate a race condition in editline
        mode.Execute("");
      }
      else
      {
        // Commit seems to be more what people expect from this than Hide.
        t->Commit();
        mode.GetText().Delete(mode.GetText().Begin(), mode.GetText().End());
        mode.SetHistoryPositionToEnd();
      }
    }
  }
  ModeCommand<EmacsMode> cancelOrSigInt("cancel-or-sigint", CancelOrSigInt, bindings, Keys::Ctrl + 'C', Keys::Interrupt);

  //! Generic (non-mode-specific) stuff.
  //@{
  void SigQuit(Editor &editor)
  {
    if (Terminal *t = editor.GetTerminal())
    {
      t->Commit();

      SuspendTerminal suspend(*t);
      kill(-tcgetpgrp(0), SIGQUIT);
    }
  }
  Command sigquit("sigquit", SigQuit, bindings, Keys::Quit);

  void Suspend(Editor &editor)
  {
    if (Terminal *t = editor.GetTerminal())
    {
      t->Commit(/*addNewline =*/false);

      SuspendTerminal suspend(*t);
      kill(-tcgetpgrp(0), SIGTSTP);

      // Text gets shown again by Editor.
    }
  }
  Command suspend("suspend", Suspend, bindings, Keys::Ctrl + 'Z', Keys::Suspend);

  void Redisplay(Editor &editor)
  {
    if (Terminal *t = editor.GetTerminal())
    {
      t->Redisplay();
    }
  }
  Command redisplay("redisplay", Redisplay, bindings, Keys::Ctrl + 'L');
  //@}
  

  //! history-*: navigating history.
  //@{
  ModeCommand<EmacsMode> historyPrevious("history-previous", &EmacsMode::HistoryPrevious, bindings, Keys::Ctrl + 'P', Keys::Ctrl + Keys::Up);
  ModeCommand<EmacsMode> historyNext("history-next", &EmacsMode::HistoryNext, bindings, Keys::Ctrl + 'N', Keys::Ctrl + Keys::Down);
  //@}

  ModeCommand<EmacsMode> tabComplete("tab-complete", &EmacsMode::TabComplete, bindings, '\t');

  ModeCommand<EmacsMode> acceptLine("accept-line", &EmacsMode::AcceptLine, bindings, Keys::Enter, Keys::Ctrl + 'M', Keys::Ctrl + 'J');

  void AcceptLineAndHistoryNext(EmacsMode &mode)
  {
    HistoryCursor pos = mode.GetHistoryPosition();
    if (mode.AcceptLine())
    {
      mode.SetHistoryPosition(pos);
      mode.HistoryNext();
    }
  }
  ModeCommand<EmacsMode> acceptLineAndHistoryNext("accept-line-and-history-next", AcceptLineAndHistoryNext, bindings, Keys::Ctrl + 'O');
  

  KeyBindings reverseISearchBindings;
  class ReverseISearchMode : public Mode
  {
  public:
    ReverseISearchMode(EmacsMode &baseMode);
    virtual const Command *GetHandler(const KeyCombination &keys);
    virtual void Render(Terminal &terminal)
    {
      DecoratedText dt;
      int row, col;
      baseMode.Render(dt, row, col);
      dt.Add(Attributes::Normal, "\nreverse-i-search: " + searchFor + "_");
      terminal.SetText(dt, row, col);
    }
    void AcceptLine()
    {
      delete this;
    }

    void Insert(const KeyCombination &keys)
    {
      searchFor.push_back(keys.GetKeys()[0]);
      positions.push_back(positions.back());
      if (!Matches() && !Next())
      {
        Delete();
      }
    }
    void Delete()
    {
      if (searchFor.size())
      {
        searchFor.resize(searchFor.size() - 1);
        positions.pop_back();

        positions.back().Activate(baseMode);
      }
    }
    bool Next()
    {
      do
      {
        while (baseMode.GetText().Begin() != baseMode.GetCursor())
        {
          baseMode.SetCursor(baseMode.GetCursor().Move(-1, 0));
          if (Matches())
          {
            positions.back() = HistoryPosition(baseMode);
            return true;
          }
        }
      } while (baseMode.HistoryPrevious());

      // No match!
      Terminal::Bell();
      positions.back().Activate(baseMode);
      return false;
    }

  private:
    // Do we have a match right now?
    bool Matches()
    {
      return baseMode.GetText().Get(baseMode.GetCursor(), baseMode.GetCursor().Move(searchFor.size(), 0)) == searchFor;
    }

  private:
    class HistoryPosition
    {
    public:
      HistoryPosition(EmacsMode &mode) :
        cursor(mode.GetHistoryPosition()),
        line(mode.GetCursor().GetLine()),
        column(mode.GetCursor().GetColumn())
      {
      }
      void Activate(EmacsMode &mode)
      {
        mode.SetHistoryPosition(cursor);
        mode.SetCursor(mode.GetText().Begin().Move(column, line));
      }
    private:
      HistoryCursor cursor;
      int line, column;
    };

    std::string searchFor;
    std::vector<HistoryPosition> positions;
    EmacsMode &baseMode;
  };

  ModeCommand<ReverseISearchMode> insertCharRISearch("insert-char", Command::WantKeys, &ReverseISearchMode::Insert, reverseISearchBindings);
  ModeCommand<ReverseISearchMode> deleteLeftRISearch("delete-to-left", &ReverseISearchMode::Delete, reverseISearchBindings, Keys::Backspace);
  ModeCommand<ReverseISearchMode> nextISearch("reverse-i-search", &ReverseISearchMode::Next, reverseISearchBindings, Keys::Ctrl + 'R');

  // FIXME: these should be global
  KeyBinding sigquitISearch(reverseISearchBindings, sigquit, Keys::Quit);
  KeyBinding suspendISearch(reverseISearchBindings, suspend, Keys::Ctrl + 'Z', Keys::Suspend);
  KeyBinding redisplayISearch(reverseISearchBindings, redisplay, Keys::Ctrl + 'L');

  // Bind Ctrl-C to AcceptLine to prevent it cancelling the history line.
  ModeCommand<ReverseISearchMode> acceptLineRISearch("accept-line", &ReverseISearchMode::AcceptLine, reverseISearchBindings, Keys::Ctrl + 'C', Keys::Interrupt);

  ReverseISearchMode::ReverseISearchMode(EmacsMode &baseMode) :
    Mode(baseMode.GetEditor(), reverseISearchBindings), baseMode(baseMode)
  {
    positions.push_back(HistoryPosition(baseMode));
  }

  const Command *ReverseISearchMode::GetHandler(const KeyCombination &keys)
  {
    if (keys.GetKeys().size() == 1 && IsPrintable(keys.GetKeys()[0]))
    {
      return &insertCharRISearch;
    }
    if (const Command *command = Mode::GetHandler(keys))
    {
      return command;
    }

    // Unknown key: exit mode and pass to base mode.
    const Command *handler = baseMode.GetHandler(keys);

    // Re-render without the 'reverse-i-search' banner, in case
    // the baseMode calls terminal->Commit().
    if (Terminal *terminal = GetEditor().GetTerminal())
    {
      baseMode.Render(*terminal);
    }

    delete this;
    return handler;
  }

  void ReverseISearch(EmacsMode &mode) { new ReverseISearchMode(mode); }
  ModeCommand<EmacsMode> reverseISearch("reverse-i-search", ReverseISearch, bindings, Keys::Ctrl + 'R');

  void ExecuteCommand(EmacsMode &mode, const std::string &command, void *arg)
  {
    Text &t = mode.GetText();

    // Back up old stuff.
    std::string oldText = t.Get();
    int line = mode.GetCursor().GetLine(), col = mode.GetCursor().GetColumn();

    // Set new command.
    t.Delete(t.Begin(), t.End());
    t.Insert(InsertLeft, t.Begin(), command);

    // Run it.
    mode.DoExecute(command, arg);

    // Undo that stuff.
    t.Delete(t.Begin(), t.End());
    t.Insert(InsertLeft, t.Begin(), oldText);
    mode.SetCursor(t.Begin(line).Move(col, 0));
  }
}

class EmacsMode::Internals
{
public:
  Internals(Editor &editor) : text(editor), cursor(text.Begin()), haveHistoryPosition(false), historyPosition(), tabCompleting(false) {}
  Text text;
  Cursor cursor;
  bool haveHistoryPosition;
  HistoryCursor historyPosition;
  bool tabCompleting;
  std::string hintText;

  typedef std::map<HistoryCursor, std::string> HistoryEdits;
  HistoryEdits historyEdits;

  HistoryCursor GetHistoryPosition(History *h);
  void SetHistoryPosition(HistoryCursor pos);
  void SetHistoryPositionToEnd();
};

EmacsMode::EmacsMode(Editor &editor) :
  Mode(editor, bindings), internals(new Internals(editor))
{
}

EmacsMode::~EmacsMode()
{
  delete internals;
}

const Command *EmacsMode::GetHandler(const KeyCombination &keys)
{
  const Command *command = 0;
  if (keys.GetKeys().size() == 1 && IsPrintable(keys.GetKeys()[0]))
  {
    command = &insertChar;
  }
  else
  {
    command = Mode::GetHandler(keys);
  }

  if (command != &tabComplete)
  {
    internals->tabCompleting = false;
  }

  /*if (!command)
  {
    command = &insertCode;
  }*/

  return command;
}

void EmacsMode::Render(Terminal &terminal)
{
  DecoratedText dt;
  int row, col;
  Render(dt, row, col);
  terminal.SetText(dt, row, col);
}

std::string EmacsMode::GetPrompt(int line)
{
  return line ? "> " : "$ ";
}

void EmacsMode::Render(DecoratedText &dt, int &row, int &col)
{
  const Cursor &cursor = GetCursor();
  const Text &text = GetText();

  row = cursor.GetLine();
  col = std::min<int>(cursor.GetColumn(), text.Get(cursor.GetLine()).size());

  int startLine = 0, endLine = text.GetNumLines();
  int charsOnScreen = 80 * 25;
  // Don't render too many lines.
  if (Terminal *t = GetEditor().GetTerminal())
  {
    if (endLine > 2 * t->GetNumRows())
    {
      startLine = std::max(startLine, row - t->GetNumRows());
      endLine = std::min(endLine, row + t->GetNumRows());
    }
    charsOnScreen = t->GetNumRows() * t->GetNumColumns();
  }
  for (int line = startLine; line < endLine; ++line)
  {
    if (line != startLine) { dt.Add(Attributes::Normal, "\n"); }

    std::string prompt = GetPrompt(line);
    if (line == row) { col += prompt.size(); }
    dt.Add(Attributes::Normal, prompt);

    // Don't render too much of this line.
    std::string textThisLine = text.Get(line);
    if (textThisLine.size() > 2 * charsOnScreen)
    {
      // Significantly more text in this line than characters on the
      // screen. Don't try to render it all; that'd take ages.
      size_t startCol = (line < row ? textThisLine.size() - charsOnScreen :
                         line > row ? 0 :
                         std::max(0, col - charsOnScreen));
      size_t num = (line == row ? 2 : 1) * charsOnScreen;
      textThisLine.substr(startCol, num).swap(textThisLine);
      if (line == row) { col -= startCol; }
    }
    dt.Add(Attributes::Normal, textThisLine);
  }
  if (startLine == 0 && endLine == text.GetNumLines() && !internals->hintText.empty())
  {
    dt.Add(Attributes::Normal, "\n" + internals->hintText);
  }

  row -= startLine;
}

void EmacsMode::SetHintText(const std::string &text)
{
  internals->hintText = text;
}

namespace
{
  static void PrintInColumns(Terminal *t, const EmacsMode::Completions &values)
  {
    const int numSpaces = 2;
    // Don't use last column, since writing a character there can force us onto another line.
    const int numTerminalCols = t->GetNumColumns() - 2;

    for (int numColumns = std::max<int>(std::min<int>((numTerminalCols + numSpaces) / (1 + numSpaces), values.size()), 1);
         numColumns >= 1; --numColumns)
    {
      std::vector<int> widths(numColumns);
      int totalWidth = (numColumns - 1) * numSpaces;

      int n = 0;
      for (EmacsMode::Completions::const_iterator it = values.begin();
           it != values.end() && totalWidth <= numTerminalCols; ++it, ++n)
      {
        int w = it->first.size() + it->second.size();
        int &colWidth = widths[n % numColumns];
        if (w > colWidth)
        {
          totalWidth += w - colWidth;
          colWidth = w;
        }
      }

      if (totalWidth <= numTerminalCols || numColumns == 1)
      {
        DecoratedText text;
        int row = 0, col = 0;
        int spaces = 0, n = 0;
        for (EmacsMode::Completions::const_iterator it = values.begin();
             it != values.end(); ++it, ++n)
        {
          if (n % numColumns)
          {
            text.Add(Attributes::Normal, std::string(spaces, ' '));
          }
          else if (n)
          {
            text.Add(Attributes::Normal, "\n");
          }
          text.Add(Attributes::Normal, it->first);
          text.Add(Attributes::Normal, it->second);
          spaces = numSpaces + widths[n % numColumns] - it->first.size() - it->second.size();
        }
        t->SetText(text, row, col);
        t->Commit();
        break;
      }
      else if (n < numColumns)
      {
        // On the first line. Obviously can't do it with more than n columns.
        // So try n next time.
        numColumns = n + 1;
      }
    }
  }
}

void EmacsMode::GetCompletions(Completions &matches) {}
void EmacsMode::TabComplete()
{
  // Find completions.
  Completions matchset;
  GetCompletions(matchset);

  // Is the completion unique? If not, beep.
  if (matchset.size() != 1)
  {
    Terminal::Bell();
  }
  else
  {
    GetText().Insert(Redline::InsertLeft, GetCursor(), matchset.begin()->second);
    return;
  }

  if (matchset.empty())
  {
    return;
  }

  if (internals->tabCompleting)
  {
    // Second press of tab: print completions.
    if (Terminal *t = GetEditor().GetTerminal())
    {
      std::string hintText = internals->hintText;
      if (!hintText.empty())
      {
        SetHintText("");
        Render(*t);
      }
      t->Commit();
      PrintInColumns(t, matchset);
      SetHintText(hintText);
    }
    else
    {
      for (Completions::iterator it = matchset.begin(); it != matchset.end(); /**/)
      {
        fputs(it->first.c_str(), stdout);
        fputs(it->second.c_str(), stdout);
        putchar(++it == matchset.end() ? '\n' : ' ');
      }
      fflush(stdout);
    }
  }
  internals->tabCompleting = true;

  // Find common prefix.
  Completions::iterator it = matchset.begin();
  std::string match = (*it++).second;
  for (/**/; it != matchset.end(); ++it)
  {
    std::string name = it->second;
    size_t pos;
    for (pos = 0; pos < match.size() && pos < name.size(); ++pos)
    {
      if (match[pos] != name[pos])
      {
        break;
      }
    }
    match.resize(pos);
  }

  // Insert the common portion.
  GetText().Insert(Redline::InsertLeft, GetCursor(), match);
}

bool EmacsMode::AcceptLine()
{
  if (TextIsComplete())
  {
    Text &text = GetText();
    DoExecute(text.Get());
    text.Delete(text.Begin(), text.End());
    return true;
  }
  else
  {
    GetText().Insert(InsertLeft, GetCursor(), "\n");
    return false;
  }
}

bool EmacsMode::TextIsComplete()
{
  return true;
}

void EmacsMode::DoExecute(const std::string &text, void *arg)
{
  std::auto_ptr<SuspendTerminal> susp;
  if (Terminal *t = GetEditor().GetTerminal())
  {
    // Make sure the right text is in the terminal's buffer, and
    // permanently commit it.
    SetHintText("");
    Render(*t);
    t->Commit();
    susp.reset(new SuspendTerminal(*t));
  }

  if (History *h = GetHistory())
  {
    h->Add(text);
  }
  Execute(text, arg);
  if (History *h = GetHistory())
  {
    SetHistoryPositionToEnd();
  }

  susp.reset();
}

void EmacsMode::AsyncExecute(const std::string &text, void *arg)
{
  GetEditor().AsyncCommand(
    new ModeCommand<EmacsMode>("", boost::bind(ExecuteCommand, _1, text, arg), bindings));
}

void EmacsMode::Execute(const std::string &, void *arg) {}

Text &EmacsMode::GetText() { return internals->text; }
Cursor EmacsMode::GetCursor() const { return internals->cursor; }
void EmacsMode::SetCursor(const Cursor &cursor) { internals->cursor = cursor; }

History *EmacsMode::GetHistory() { return 0; }

HistoryCursor EmacsMode::Internals::GetHistoryPosition(History *h)
{
  if (h && !haveHistoryPosition)
  {
    return h->End();
  }
  return historyPosition;
}
void EmacsMode::Internals::SetHistoryPosition(HistoryCursor pos)
{
  if (pos)
  {
    historyPosition = pos;
    haveHistoryPosition = true;
  }
}
void EmacsMode::Internals::SetHistoryPositionToEnd()
{
  historyPosition = 0;
  haveHistoryPosition = false;
  // Jump-to-end means we're done for now with this editing.
  historyEdits.clear();
}

HistoryCursor EmacsMode::GetHistoryPosition() { return internals->GetHistoryPosition(GetHistory()); }
bool EmacsMode::SetHistoryPosition(HistoryCursor pos)
{
  HistoryCursor prev = GetHistoryPosition();
  if (pos && pos != prev)
  {
    internals->historyEdits[prev] = GetText().Get();
    if (History *h = GetHistory())
    {
      Internals::HistoryEdits::const_iterator it = internals->historyEdits.find(pos);
      std::string hist = it != internals->historyEdits.end() ? it->second : h->Get(pos);
      if (!hist.empty() || pos == h->End() || it != internals->historyEdits.end())
      {
        internals->SetHistoryPosition(pos);
        GetText().Delete(GetText().Begin(), GetText().End());
        GetText().Insert(InsertLeft, GetText().Begin(), hist);
        return true;
      }
    }
  }
  return false;
}
void EmacsMode::SetHistoryPositionToEnd()
{
  internals->SetHistoryPositionToEnd();
}

bool EmacsMode::HistoryPrevious()
{
  if (History *h = GetHistory())
  {
    HistoryCursor pos = internals->GetHistoryPosition(h);
    if (pos && pos != h->Begin())
    {
      return SetHistoryPosition(h->Previous(pos));
    }
  }
  return false;
}
bool EmacsMode::HistoryNext()
{
  if (History *h = GetHistory())
  {
    HistoryCursor pos = internals->GetHistoryPosition(h);
    if (pos && pos != h->End())
    {
      return SetHistoryPosition(h->Next(pos));
    }
  }
  return false;
}
