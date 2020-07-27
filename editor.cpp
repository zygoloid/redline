#include "redline/editor.hpp"

#include <iostream>
#include <deque>
#include <pthread.h>

#include "redline/bindings.hpp"
#include "redline/command.hpp"
#include "redline/terminal.hpp"
#include "redline/mode.hpp"

using namespace Redline;

template<typename T>
class LockedFifo
{
public:
  LockedFifo()
  {
    pthread_mutex_init(&mutex, 0);
  }
  ~LockedFifo()
  {
    pthread_mutex_destroy(&mutex);
  }
  void Push(T t) throw()
  {
    pthread_mutex_lock(&mutex);
    fifo.push_back(t);
    pthread_mutex_unlock(&mutex);
  }
  bool Pop(T &t) throw()
  {
    pthread_mutex_lock(&mutex);
    bool result = false;
    if (!fifo.empty())
    {
      t = fifo.front();
      fifo.pop_front();
      result = true;
    }
    pthread_mutex_unlock(&mutex);
    return result;
  }
private:
  pthread_mutex_t mutex;
  std::deque<T> fifo;
};

class Editor::Internals
{
public:
  Internals(Editor &_editor) : editor(_editor), terminal(), mode()
  {
  }

  void Run(bool noTerminal);

  Editor &editor;
  Terminal *terminal;
  Mode *mode;

  LockedFifo<const Command*> asyncCommands;
};

Editor::Editor() :
  internals(new Internals(*this))
{
}

Editor::~Editor()
{
  delete internals;
}

void Editor::Internals::Run(bool noTerminal)
{
  //Assert(!terminal, "Recursive invocation of Redline::Editor::Run?");

  if (!noTerminal)
  {
    terminal = new Terminal;
  }

  while (mode)
  {
    if (terminal)
    {
      // We've gone idle, waiting for input.
      mode->Idle();

      // Update screen.
      mode->Render(*terminal);

      // Block waiting for next key.
      terminal->WaitForKey();
    }

    do
    {
      Key key = terminal ? terminal->GetKey() : getchar();
      // Convert EOF from getchar() to Keys::Eof, but leave all
      // other characters intact.
      key = (!terminal && key == EOF) ? Keys::Eof : key;
      KeyCombination keys(key);
      if (const Command *command = mode->GetHandler(keys))
      {
        command->Run(editor, keys);
      }
      else if (terminal && key != Keys::AsyncInterrupted)
      {
        terminal->Bell();
      }

      // Process async commands.
      const Command *command = 0;
      while (asyncCommands.Pop(command))
      {
        command->Run(editor, KeyCombination());
        delete command;
      }
    } while (terminal && terminal->HaveKey());
  }

  delete terminal;
  terminal = 0;
}

void Editor::AsyncCommand(const Command *command)
{
  internals->asyncCommands.Push(command);
  if (internals->terminal)
  {
    internals->terminal->AsyncInterruptWaitForKey();
  }
}

//! Read a line of input.
void Editor::Run(bool noTerminal /*= false*/) { internals->Run(noTerminal); }
Terminal *Editor::GetTerminal() const { return internals->terminal; }
Mode *Editor::GetMode() const { return internals->mode; }

void Editor::EndMode()
{
  delete internals->mode;
  // ~Mode calls SetMode.
}

void Editor::SetMode(Mode *mode)
{
  internals->mode = mode;
}
