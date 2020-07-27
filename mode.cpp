#include "redline/mode.hpp"

#include "redline/bindings.hpp"
#include "redline/command.hpp"
#include "redline/editor.hpp"

#include <iostream>

using namespace Redline;

class Mode::Internals
{
public:
  Internals(Editor &_editor, const KeyBindings &_bindings, Mode *_oldMode) :
    editor(_editor), bindings(_bindings), oldMode(_oldMode)
  {
  }

  const Command *GetCommand(const KeyCombination &keys)
  {
    return bindings.Get(keys);
  }

  Editor &editor;
  const KeyBindings &bindings;
  Mode *oldMode;
};

Mode::Mode(Editor &editor, const KeyBindings &bindings) :
  internals(new Internals(editor, bindings, editor.GetMode()))
{
  editor.SetMode(this);
}

Mode::~Mode()
{
  internals->editor.SetMode(internals->oldMode);
  delete internals;
}

Mode *Mode::GetParent() const
{
  return internals->oldMode;
}

void Mode::Idle()
{
}

Editor &Mode::GetEditor() const
{
  return internals->editor;
}

const Command *Mode::GetHandler(const KeyCombination &keys)
{
  return internals->GetCommand(keys);
}
