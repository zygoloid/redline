#ifndef REDLINE_FORWARD_DECLS_HPP_INCLUDED
#define REDLINE_FORWARD_DECLS_HPP_INCLUDED

namespace Redline
{
  class Editor;
  class Command;
  class KeyBindings;
  class KeyCombination;
  class Terminal;
  class DecoratedText;
  class Text;
  class Cursor;
  class Mode;
  class History;

  extern const KeyCombination NoKeyCombination;

  typedef int Key;
  typedef void *HistoryCursor;
}

#endif
