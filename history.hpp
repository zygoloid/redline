#ifndef REDLINE_HISTORY_HPP_INCLUDED
#define REDLINE_HISTORY_HPP_INCLUDED

#include "redline/forward-decls.hpp"

#include <deque>
#include <string>

namespace Redline
{
  //! History implementation.
  /*! All of the methods here are permitted to fail (by returning 0 or
   *  an empty string). This implies that 0 is not a valid HistoryCursor
   *  and that empty strings are not permitted as history items.
   */
  class History
  {
  public:
    virtual ~History() {}

    //! Get a cursor for the first history entry.
    virtual HistoryCursor Begin() = 0;
    //! Get a cursor past the last history entry (where Add will insert).
    virtual HistoryCursor End() = 0;

    //! Get the next history entry after a given one. Undefined at End().
    virtual HistoryCursor Next(HistoryCursor) = 0;
    //! Get the previous history entry before a given one. Undefined at Begin().
    virtual HistoryCursor Previous(HistoryCursor) = 0;

    //! Get the text corresponding to a history entry.
    virtual std::string Get(HistoryCursor) = 0;
    //! Add a new history entry at End().
    virtual void Add(const std::string &text) = 0;
  };

  //! History implementation in terms of a simple list of strings.
  class VectorHistory : public History
  {
  public:
    VectorHistory(size_t maxLines);

    virtual HistoryCursor Begin();
    virtual HistoryCursor End();

    virtual HistoryCursor Next(HistoryCursor);
    virtual HistoryCursor Previous(HistoryCursor);

    virtual std::string Get(HistoryCursor);
    virtual void Add(const std::string &text);

  private:
    std::deque<std::string> lines;
    size_t start;
    size_t maxLines;
  };
}

#endif
