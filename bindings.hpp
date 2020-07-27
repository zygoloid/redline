#ifndef REDLINE_BINDINGS_HPP_INCLUDED
#define REDLINE_BINDINGS_HPP_INCLUDED

#include "redline/forward-decls.hpp"

#include <boost/noncopyable.hpp>
#include <vector>

namespace Redline
{
  namespace Keys
  {
    enum KeyNames
    {
      Ctrl = -64,
      Alt = 0x80,

      AsyncInterrupted = 0,
      Backspace = 127,
      Escape = '\e',

      Eof = 0x200, // Ctrl-D usually
      Suspend,     // Ctrl-Z usually
      Interrupt,   // Ctrl-C usually
      Quit,        // Ctrl-\ usually

      Ignored,     // A key we don't care about

      Enter,
      Up,
      Down,
      Left,
      Right,

      PageUp,
      PageDown,
      Home,
      End,
      Insert,
      Delete
    };
  }

  class KeyCombination
  {
  public:
    KeyCombination() : keys() {}
    KeyCombination(Key key) : keys(1, key) {}

    const std::vector<Key> &GetKeys() const { return keys; }

    operator bool() { return !keys.empty(); }

  private:
    std::vector<Key> keys;
  };

  class KeyBindings : boost::noncopyable
  {
  public:
    KeyBindings();
    ~KeyBindings();

    bool Add(const KeyCombination &keys, const Command &command);
    const Command *Get(const KeyCombination &keys) const;

  private:
    class Internals;
    Internals *internals;
  };
}

#endif
