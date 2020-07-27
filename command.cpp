#include "redline/command.hpp"
#include "redline/editor.hpp"
#include "redline/bindings.hpp"

using namespace Redline;

class Command::Internals
{
public:
  Internals(const std::string &_name, const CommandFn &_fn) :
    name(_name), fn(_fn)
  {
  }

  void Run(Editor &editor, const KeyCombination &keys)
  {
    fn(editor, keys);
  }

private:
  std::string name;
  CommandFn fn;
};

Command::Command(const std::string &name, WantKeysType, const CommandFn &fn,
                 KeyBindings &bindings, const KeyCombination &key1,
                 const KeyCombination &key2, const KeyCombination &key3) :
  internals(new Internals(name, fn))
{
  bindings.Add(key1, *this);
  bindings.Add(key2, *this);
  bindings.Add(key3, *this);
}

Command::Command(const std::string &name, const CommandFnNoKeys &fn,
                 KeyBindings &bindings, const KeyCombination &key1,
                 const KeyCombination &key2, const KeyCombination &key3) :
  internals(new Internals(name, boost::bind(fn, _1)))
{
  bindings.Add(key1, *this);
  bindings.Add(key2, *this);
  bindings.Add(key3, *this);
}

Command::Command(const std::string &name, WantKeysType, const CommandFn &fn) :
  internals(new Internals(name, fn))
{
}

Command::Command(const std::string &name, const CommandFnNoKeys &fn) :
  internals(new Internals(name, boost::bind(fn, _1)))
{
}

Command::~Command()
{
  delete internals;
}

void Command::Run(Editor &editor, const KeyCombination &keys) const
{
  internals->Run(editor, keys);
}

//! Avoids having to include editor.hpp in command.hpp.
/*static*/ Mode *Command::GetMode(Editor &editor)
{
  return editor.GetMode();
}

KeyBinding::KeyBinding(KeyBindings &bindings, Command &command,
                       const KeyCombination &key1, const KeyCombination &key2,
                       const KeyCombination &key3)
{
  bindings.Add(key1, command);
  bindings.Add(key2, command);
  bindings.Add(key3, command);
}
