#ifndef REDLINE_COMMAND_HPP_INCLUDED
#define REDLINE_COMMAND_HPP_INCLUDED

#include "redline/forward-decls.hpp"
#include "redline/mode.hpp"

#include <string>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

namespace Redline
{
  typedef boost::function<void(Editor&, const KeyCombination&)> CommandFn;
  typedef boost::function<void(Editor&)> CommandFnNoKeys;

  class Command : boost::noncopyable
  {
  public:
    enum WantKeysType { WantKeys };
    Command(const std::string &name, WantKeysType, const CommandFn &fn, KeyBindings &bindings,
            const KeyCombination &key1 = NoKeyCombination,
            const KeyCombination &key2 = NoKeyCombination,
            const KeyCombination &key3 = NoKeyCombination);
    Command(const std::string &name, const CommandFnNoKeys &fn, KeyBindings &bindings,
            const KeyCombination &key1 = NoKeyCombination,
            const KeyCombination &key2 = NoKeyCombination,
            const KeyCombination &key3 = NoKeyCombination);
    Command(const std::string &name, WantKeysType, const CommandFn &fn);
    Command(const std::string &name, const CommandFnNoKeys &fn);
    ~Command();

    void Run(Editor &editor, const KeyCombination &keys) const;

  protected:
    static Mode *GetMode(Editor &editor);

  private:
    class Internals;
    Internals *internals;
  };

  template <typename ModeType>
  class ModeCommand : public Command
  {
  public:
    typedef boost::function<void(ModeType&, const KeyCombination&)> ModeCommandFn;
    typedef boost::function<void(ModeType&)> ModeCommandFnNoKeys;

    ModeCommand(const std::string &name, WantKeysType, const ModeCommandFn &fn, KeyBindings &bindings,
                const KeyCombination &key1 = NoKeyCombination,
                const KeyCombination &key2 = NoKeyCombination,
                const KeyCombination &key3 = NoKeyCombination) :
      Command(name, WantKeys, boost::bind(&ModeCommand::CommandFn, fn, _1, _2), bindings, key1, key2, key3)
    {}
    ModeCommand(const std::string &name, const ModeCommandFnNoKeys &fn, KeyBindings &bindings,
                const KeyCombination &key1 = NoKeyCombination,
                const KeyCombination &key2 = NoKeyCombination,
                const KeyCombination &key3 = NoKeyCombination) :
      Command(name, boost::bind(&ModeCommand::CommandFnNoKeys, fn, _1), bindings, key1, key2, key3)
    {}

  private:
    static ModeType *GetTypedMode(Editor &editor)
    {
      for (Mode *mode = GetMode(editor); mode; mode = mode->GetParent())
      {
        if (ModeType *t = dynamic_cast<ModeType*>(mode)) { return t; }
      }
      return 0;
    }
    static void CommandFn(const ModeCommandFn &fn, Editor &editor, const KeyCombination &keys)
    {
      if (ModeType *t = GetTypedMode(editor)) { fn(*t, keys); }
    }
    static void CommandFnNoKeys(const ModeCommandFnNoKeys &fn, Editor &editor)
    {
      if (ModeType *t = GetTypedMode(editor)) { fn(*t); }
    }
  };

  class KeyBinding : boost::noncopyable
  {
  public:
    KeyBinding(KeyBindings &bindings, Command &command, const KeyCombination &key1,
               const KeyCombination &key2 = NoKeyCombination,
               const KeyCombination &key3 = NoKeyCombination);
  };
}

#endif
