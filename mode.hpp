#ifndef REDLINE_MODE_HPP_INCLUDED
#define REDLINE_MODE_HPP_INCLUDED

#include "redline/forward-decls.hpp"

#include <boost/noncopyable.hpp>

namespace Redline
{
  class Mode : boost::noncopyable
  {
  public:
    Mode(Editor &editor, const KeyBindings &bindings);
    virtual ~Mode();

    virtual Mode *GetParent() const;

    virtual const Command *GetHandler(const KeyCombination &keys);
    virtual void Render(Terminal &terminal) = 0;
    virtual void Idle();

    Editor &GetEditor() const;

  private:
    class Internals;
    Internals *internals;
  };
}

#endif
