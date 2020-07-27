#ifndef REDLINE_TERMINAL_HPP_INCLUDED
#define REDLINE_TERMINAL_HPP_INCLUDED

#include "redline/forward-decls.hpp"

#include <string>

#include <boost/noncopyable.hpp>

namespace Redline
{
  class TerminalAttribute;

  namespace Attributes
  {
    extern TerminalAttribute Normal;
    extern TerminalAttribute Error;
  }

  //------------------------------------------------------------------------------------------------
  /*! A section of text, decorated with terminal attributes.
   */
  //------------------------------------------------------------------------------------------------
  class DecoratedText : boost::noncopyable
  {
  public:
    DecoratedText();
    ~DecoratedText();

    void Add(const TerminalAttribute &attribute, const std::string &text);

    class Internals;
  private:
    friend class Terminal;
    Internals *internals;
  };

  //------------------------------------------------------------------------------------------------
  /*! Temporarily suspend the terminal.
   */
  //------------------------------------------------------------------------------------------------
  class SuspendTerminal : boost::noncopyable
  {
  public:
    SuspendTerminal(Terminal &terminal);
    ~SuspendTerminal();

    class Internals;
  private:
    Internals *internals;
  };

  //------------------------------------------------------------------------------------------------
  /*! A terminal, with the ability to read keys and output text. At the moment, only a single
   *  terminal is supported.
   */
  //------------------------------------------------------------------------------------------------
  class Terminal : boost::noncopyable
  {
  public:
    Terminal();
    ~Terminal();

  public:
    //! Blocking wait for a keypress.
    void WaitForKey();

    //! Non-blocking check for keys.
    bool HaveKey();

    //! Non-blocking grab of current key.
    Key GetKey();

    //! Interrupt WaitForKey().
    void AsyncInterruptWaitForKey();

    //! Set the currently-visible terminal text.
    void SetText(const DecoratedText &text, int cursorLine, int cursorCol);

    //! Hide the current text.
    void Hide();

    //! Commit the current text to the display.
    void Commit(bool addNewline = true);

    //! Redisplay the current text.
    void Redisplay();

    //! Get the height of the terminal.
    int GetNumRows();

    //! Get the width of the terminal.
    int GetNumColumns();

    //! Emit a warning bell.
    static void Bell();

    class Internals;
  private:
    friend class SuspendTerminal;
    Internals *internals;
  };
}

#endif
