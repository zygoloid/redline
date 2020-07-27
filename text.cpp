#include "redline/text.hpp"

#include <vector>

#include <iostream>
using namespace Redline;

namespace
{
  int Clamp(int x, int min, int max)
  {
    return (x < min ? min : x > max ? max : x);
  }
}

class Cursor::Internals
{
public:
  //! Construct root node.
  Internals() : refcount(0), text(), prev(this), next(this), line(), column() {}

  Internals(const Text *_text, int _line, int _column, Internals *_after) :
    refcount(0), text(_text), prev(_after), next(_after->next), line(_line), column(_column)
  {
    prev->next = next->prev = this;
  }
  ~Internals()
  {
    prev->next = next;
    next->prev = prev;
  }

  static Internals *IncRef(Internals *p)
  {
    if (p)
    {
      ++p->refcount;
    }
    return p;
  }
  static void DecRef(Internals *p)
  {
    if (p && --p->refcount == 0)
    {
      delete p;
    }
  }

  Internals *Next() const { return next; }

  Internals *Move(int x, int y)
  {
    // Do nothing if trying to go off top/bottom.
    int l = Clamp(line + y, 0, text->GetNumLines() - 1);
    // Wrap around if asked to go off left or right edge.
    int c = column + x;
    while (x && c < 0 && l)
    {
      --l;
      c += text->Get(l).size() + 1;
    }
    while (x && c > static_cast<int>(text->Get(l).size()) && l < text->GetNumLines() - 1)
    {
      c -= text->Get(l).size() + 1;
      ++l;
    }
    c = Clamp(c, 0, text->Get(l).size());
    return new Internals(text, l, c, this);
  }

private:
  int refcount;
public:
  const Text *text;
private:
  Internals *prev, *next;
public:
  int line, column;
};

Cursor::Cursor() : internals() {}
Cursor::Cursor(const Cursor &o) : internals(Internals::IncRef(o.internals)) {}
Cursor::Cursor(Internals *p) : internals(Internals::IncRef(p)) {}
Cursor &Cursor::operator=(const Cursor &o)
{
  Internals *p = Internals::IncRef(o.internals);
  Internals::DecRef(internals);
  internals = p;
  return *this;
}
Cursor::~Cursor() { Internals::DecRef(internals); }

bool Cursor::IsValid() const { return internals; } 
const Text *Cursor::GetText() const { return internals ? internals->text : 0; } 
int Cursor::GetLine() const { return internals ? internals->line : 0; }
int Cursor::GetColumn() const { return internals ? internals->column : 0; }
Cursor Cursor::Move(int x, int y) const { return internals ? internals->Move(x, y) : 0; }
int Cursor::Cmp(const Cursor &o) const
{
  return IsValid() != o.IsValid() ? IsValid() - o.IsValid() :
         GetLine() != o.GetLine() ? GetLine() - o.GetLine() :
         GetColumn() - o.GetColumn();
}

//--------------------------------------------------------------------------------------------------
/*! Get the character to the left of the cursor, if any, or 0 if not (if, for instance, the cursor
 *  is at the start of the text, or is invalid).
 */
//--------------------------------------------------------------------------------------------------
char Cursor::GetLeft() const
{
  if (const Text *text = GetText())
  {
    std::string t = text->Get(*this, Move(-1, 0));
    if (t.size())
    {
      return t[0];
    }
  }
  return 0;
}

//--------------------------------------------------------------------------------------------------
/*! Get the character to the right of the cursor, if any, or 0 if not (if, for instance, the cursor
 *  is at the end of the text, or is invalid).
 */
//--------------------------------------------------------------------------------------------------
char Cursor::GetRight() const
{
  if (const Text *text = GetText())
  {
    std::string t = text->Get(*this, Move(1, 0));
    if (t.size())
    {
      return t[0];
    }
  }
  return 0;
}


//--------------------------------------------------------------------------------------------------
/*! Implementation of block of text.
 */
//--------------------------------------------------------------------------------------------------
class Text::Internals
{
public:
  Internals(Editor &_editor) :
    editor(_editor), text(1)
  {
  }

  Editor &editor;
  std::vector<std::string> text;
  Cursor::Internals cursorList;
};

Text::Text(Editor &editor) :
  internals(new Internals(editor))
{
}

Text::~Text()
{
  delete internals;
}

int Text::GetNumLines() const
{
  return internals->text.size();
}

std::string Text::Get() const
{
  std::string text;
  for (size_t n = 0; n < internals->text.size(); ++n)
  {
    if (n) { text += '\n'; }
    text += internals->text[n];
  }
  return text;
}

std::string Text::Get(int line) const
{
  return internals->text[line];
}

std::string Text::Get(const Cursor &from, const Cursor &to) const
{
  std::string result;
  if (from.GetText() == this && to.GetText() == this)
  {
    int startLine = from.internals->line, endLine = to.internals->line;
    int start = from.internals->column, end = to.internals->column;

    if (to < from)
    {
      std::swap(startLine, endLine);
      std::swap(start, end);
    }

    for (int line = startLine; line <= endLine; ++line)
    {
      const std::string &text = internals->text[line];
      int startThisLine = (line == startLine ? start : 0);
      int endThisLine = (line == endLine ? end : text.size());
      result += text.substr(startThisLine, endThisLine - startThisLine);
      if (line != endLine)
      {
        result += '\n';
      }
    }
  }
  return result;
}

Cursor Text::Begin(int line) const
{
  if (line >= GetNumLines()) { return End(); }
  line = std::max(line, 0);
  return new Cursor::Internals(this, line, 0, &internals->cursorList);
}

Cursor Text::End(int line) const
{
  if (line < 0) { return Begin(); }
  line = std::min(line, GetNumLines() - 1);
  return new Cursor::Internals(this, line, internals->text[line].size(), &internals->cursorList);
}

Cursor Text::Begin() const
{
  return Begin(0);
}

Cursor Text::End() const
{
  return End(GetNumLines() - 1);
}

void Text::Insert(InsertPosition rel, const Cursor &pos, const std::string &text)
{
  if (pos.GetText() == this)
  {
    int line = pos.internals->line;
    int column = pos.internals->column;

    std::string restOfLine = internals->text[line].substr(column);
    internals->text[line].erase(column);

    int currLine = line;
    // Scan over chunks of the string separated by newlines.
    for (int textPos = 0; textPos <= text.size(); ++currLine)
    {
      int textEnd = std::min(text.find('\n', textPos), text.size());
      std::string addThisLine = text.substr(textPos, textEnd - textPos);
      if (currLine == line)
      {
        // First line. Append bits.
        internals->text[currLine].insert(column, addThisLine);
      }
      else
      {
        // Second or subsequent lines: insert a new line.
        internals->text.insert(internals->text.begin() + currLine, addThisLine);
      }
      textPos = textEnd + 1;
    }

    // Add back on the end of the line into which we were inserting.
    internals->text[currLine - 1] += restOfLine;
    int addedLines = currLine - line - 1;

    // Adjustment to column of cursor after pos on the same line.
    int deltaLastLine = (addedLines ? text.size() - text.rfind('\n') - column - 1: text.size());

    for (Cursor::Internals *c = internals->cursorList.Next(); c != &internals->cursorList; c = c->Next())
    {
      if (c->line == line && c->column >= column + rel)
      {
        c->line += addedLines;
        c->column += deltaLastLine;
      }
      else if (c->line > line)
      {
        c->line += addedLines;
      }
    }
  }
}

void Text::Delete(const Cursor &from, const Cursor &to)
{
  if (from.GetText() == this && to.GetText() == this)
  {
    int startLine = from.internals->line, endLine = to.internals->line;
    int start = from.internals->column, end = to.internals->column;

    if (to < from)
    {
      std::swap(startLine, endLine);
      std::swap(start, end);
    }

    internals->text[startLine] = internals->text[startLine].substr(0, start) + internals->text[endLine].substr(end);
    internals->text.erase(internals->text.begin() + startLine + 1, internals->text.begin() + endLine + 1);

    for (Cursor::Internals *c = internals->cursorList.Next(); c != &internals->cursorList; c = c->Next())
    {
      if (c->line > startLine || (c->line == startLine && c->column > start))
      {
        if (c->line < endLine || (c->line == endLine && c->column < end))
        {
          // In the deleted area.
          c->line = startLine;
          c->column = start;
        }
        else if (c->line == endLine)
        {
          // After the deleted area on the end line.
          c->line = startLine;
          c->column -= end - start;
        }
        else
        {
          // On a line below the deleted area.
          c->line -= endLine - startLine;
        }
      }
    }
  }
}
