#include "redline/bindings.hpp"

#include <map>

using namespace Redline;

const KeyCombination Redline::NoKeyCombination;

class KeyBindings::Internals
{
public:
  typedef std::map<Key, const Command*> Bindings;
  Bindings bindings;
};

KeyBindings::KeyBindings() :
  internals(new Internals)
{
}

KeyBindings::~KeyBindings()
{
  delete internals;
}

bool KeyBindings::Add(const KeyCombination &keys, const Command &command)
{
  if (keys.GetKeys().size() != 1)
  {
    return false;
  }

  internals->bindings[keys.GetKeys()[0]] = &command;
  return true;
}

const Command *KeyBindings::Get(const KeyCombination &keys) const
{
  if (keys.GetKeys().size() != 1)
  {
    return 0;
  }

  Internals::Bindings::const_iterator it = internals->bindings.find(keys.GetKeys()[0]);
  return (it != internals->bindings.end() ? it->second : 0);
}
