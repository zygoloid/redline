#ifndef REDLINE_EDITOR_HPP_INCLUDED
#define REDLINE_EDITOR_HPP_INCLUDED

#include "redline/forward-decls.hpp"

#include <string>

namespace Redline
{
  class Editor
  {
  public:
    Editor();
    ~Editor();

    //! Read lines of input.
    void Run(bool noTerminal = false);

    //! Send a command asynchronously, from another thread.
    /*! Takes ownership of \p command, which must have been allocated by \c new.
     */
    void AsyncCommand(const Command *command);

    //! Get the current terminal, if any.
    Terminal *GetTerminal() const;

    //! Get the current mode.
    Mode *GetMode() const;

    //! End the current mode.
    void EndMode();

  private:
    friend class Mode;
    void SetMode(Mode *mode);

  private:
    class Internals;
    Internals *internals;
  };
}

#endif
