#ifndef REDLINE_TEXT_HPP_INCLUDED
#define REDLINE_TEXT_HPP_INCLUDED

#include "redline/forward-decls.hpp"

#include <string>

#include <boost/noncopyable.hpp>

namespace Redline
{
  //------------------------------------------------------------------------------------------------
  /*! A position within a text object. Preserved as much as possible by changes to the underlying
   *  text.
   */
  //------------------------------------------------------------------------------------------------
  class Cursor
  {
  public:
    Cursor();
    Cursor(const Cursor &o);
    Cursor &operator=(const Cursor &o);
    ~Cursor();

    bool IsValid() const;
    const Text *GetText() const;

    int GetLine() const;
    int GetColumn() const;

    char GetLeft() const;
    char GetRight() const;

    Cursor Move(int x, int y) const;

    friend bool operator==(const Cursor &l, const Cursor &r) { return l.Cmp(r) == 0; }
    friend bool operator!=(const Cursor &l, const Cursor &r) { return l.Cmp(r) != 0; }
    friend bool operator<(const Cursor &l, const Cursor &r) { return l.Cmp(r) < 0; }
    friend bool operator<=(const Cursor &l, const Cursor &r) { return l.Cmp(r) <= 0; }
    friend bool operator>(const Cursor &l, const Cursor &r) { return l.Cmp(r) > 0; }
    friend bool operator>=(const Cursor &l, const Cursor &r) { return l.Cmp(r) >= 0; }

    class Internals;
  private:
    friend class Text;
    Internals *internals;

    int Cmp(const Cursor &o) const;

    Cursor(Internals *internals);
  };

  enum InsertPosition { InsertLeft, InsertRight };

  //------------------------------------------------------------------------------------------------
  /*! A chunk of text.
   */
  //------------------------------------------------------------------------------------------------
  class Text : boost::noncopyable
  {
  public:
    Text(Editor &editor);
    ~Text();

    int GetNumLines() const;
    std::string Get() const;
    std::string Get(int line) const;
    std::string Get(const Cursor &from, const Cursor &to) const;

    Cursor Begin(int line) const;
    Cursor End(int line) const;
    Cursor Begin() const;
    Cursor End() const;

    void Insert(InsertPosition rel, const Cursor &pos, const std::string &text);
    void Delete(const Cursor &from, const Cursor &to);

    class Internals;
  private:
    Internals *internals;
  };
}

#endif
