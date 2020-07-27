#include "redline/terminal.hpp"

#include <map>
#include <vector>
#include <deque>
#include <iostream>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <term.h>

#include "redline/bindings.hpp"

#define TERMINAL_USE_TCGETATTR

using namespace Redline;

namespace
{
  //! Clean up some macros with Unfriendly Names.
  int TiLines()
  {
    return lines;
  }
  int TiColumns()
  {
    return columns;
  }
#undef lines
#undef columns

  void GetTerminalSize(int &x, int &y)
  {
#ifdef TIOCGSIZE
    ttysize size1;
    if (ioctl(1, TIOCGSIZE, &size1) == 0)
    {
      x = size1.ts_cols;
      y = size1.ts_lines;
      return;
    }
#endif

#ifdef TIOCGWINSZ
    winsize size2;
    if (ioctl(1, TIOCGWINSZ, &size2) == 0)
    {
      x = size2.ws_col;
      y = size2.ws_row;
      return;
    }
#endif

    x = TiColumns();
    y = TiLines();
  }

  //------------------------------------------------------------------------------------------------
  /*! Is the given terminfo string valid? tigetstr can return static_cast<char*>(-1) on error.
   */
  //------------------------------------------------------------------------------------------------
  static bool IsValidTiStr(const char *str)
  {
    return str && (str + 1) && *str;
  }

  //------------------------------------------------------------------------------------------------
  /*! Get a terminfo string for a given capability, or 0 if none is available.
   */
  //------------------------------------------------------------------------------------------------
  static char *GetTiStr(/*const*/ char *cap)
  {
    char *tiStr = tigetstr(cap);
    return IsValidTiStr(tiStr) ? tiStr : 0;
  }

  //------------------------------------------------------------------------------------------------
  /*! Does the terminal have the given capability?
   */
  //------------------------------------------------------------------------------------------------
  static bool HasTiFlag(/*const*/ char *cap)
  {
    return tigetflag(cap) > 0;
  }

  //------------------------------------------------------------------------------------------------
  /*! Emit the given terminfo string.
   */
  //------------------------------------------------------------------------------------------------
  static bool TiStr(/*const*/ char *cap)
  {
    if (char *tiStr = GetTiStr(cap))
    {
      putp(tiStr);
      return true;
    }
    return false;
  }

  //------------------------------------------------------------------------------------------------
  /*! Initialize the terminal, and read the corresponding terminfo. Must be done before anything
   *  else which uses terminfo.
   *
   *  This also puts the terminal into our desired state. Note that we make no attempt to undo this
   *  when the Terminal is destroyed; that's largely because it's impossible to find out what the
   *  original terminal state was.
   */
  //------------------------------------------------------------------------------------------------
  static void InitTerminal()
  {
    static bool doneInit = false;
    if (!doneInit)
    {
      doneInit = true;
      setupterm(0, 1, 0);
    }
  }

  //------------------------------------------------------------------------------------------------
  /*! Produce a bell. putchar('\a') on steroids.
   */
  //------------------------------------------------------------------------------------------------
  static void TiBell() { TiStr("bel"); }

  struct TerminalKeys
  {
    int eof, susp, intr, quit;
  };

  //------------------------------------------------------------------------------------------------
  /*! Machinations for putting the terminal into raw mode without turning on altscreen mode (there's
   *  no way to do one without the other using curses, sadly).
   *
   *  Three different implementations. I'm not sure if the second or third forms are useful; they're
   *  inspired by one of the many versions of 'libeditline'. They may not even compile.
   */
  //------------------------------------------------------------------------------------------------
#ifdef TERMINAL_USE_TCGETATTR
  #include <termios.h>
  struct TerminalData
  {
    struct termios data;
    TerminalData() { tcgetattr(0, &data); }
    void Set() { tcsetattr(0, TCSADRAIN, &data); }
    TerminalKeys GetKeys()
    {
      TerminalKeys keys;
      keys.eof = data.c_cc[VEOF];
      keys.susp = data.c_cc[VSUSP];
      keys.intr = data.c_cc[VINTR];
      keys.quit = data.c_cc[VQUIT];
      return keys;
    }
    void SetRaw()
    {
      data.c_lflag &= ~(ECHO | ICANON | ISIG);
      data.c_iflag &= ~(ISTRIP | INPCK);
      data.c_cc[VMIN] = 1;
      data.c_cc[VTIME] = 0;
    }
  };
#elif defined(TERMINAL_USE_TCGET)
  #include <termio.h>
  struct TerminalData
  {
    struct termio	data;
    TerminalData() { ioctl(0, TCGETA, &data); }
    void Set() { ioctl(0, TCSETAW, &data); }
    TerminalKeys GetKeys()
    {
      TerminalKeys keys;
      keys.eof = data.c_cc[VEOF];
      keys.susp = data.c_cc[VSUSP];
      keys.intr = data.c_cc[VINTR];
      keys.quit = data.c_cc[VQUIT];
      return keys;
    }
    void SetRaw()
    {
      data.c_lflag &= ~(ECHO | ICANON | ISIG);
      data.c_iflag &= ~(ISTRIP | INPCK);
      data.c_cc[VMIN] = 1;
      data.c_cc[VTIME] = 0;
    }
  };
#else
  #include <sgtty.h>
  struct TerminalData
  {
    struct sgttyb	sgttyb;
    struct tchars	tchars;
    struct ltchars_ltchars;
    TerminalData()
    {
      ioctl(0, TIOCGETP, &sgttyb);
      ioctl(0, TIOCGETC, &tchars);
      ioctl(0, TIOCGLTC, &ltchars);
    }
    void Set()
    {
      ioctl(0, TIOCSETP, &sgttyb);
      ioctl(0, TIOCSETC, &tchars);
      ioctl(0, TIOCSLTC, &ltchars);
    }
    TerminalKeys GetKeys()
    {
      TerminalKeys keys;
      keys.eof = tchars.t_eofc;
      keys.intr = tchars.t_intrc;
      keys.quit = tchars.t_quitc;
      keys.susp = ltchars.t_suspc;
      return keys;
    }
    void SetRaw()
    {
      sgttyb.sg_flags &= ~ECHO;
      sgttyb.sg_flags |= RAW;
#ifdef PASS8
      sgttyb.sg_flags |= PASS8;
#endif
      tchars.t_intrc = -1;
      tchars.t_quitc = -1;
    }
  };
#endif

  //! terminfo capabilities used to find the key sequences for various special keys.
  static struct { Key key; /*const*/ char *capname; } specialKeys[] = {
    {Keys::Enter, "kent"},
    {Keys::Up, "kcuu1"},
    {Keys::Down, "kcud1"},
    {Keys::Left, "kcub1"},
    {Keys::Right, "kcuf1"},

    {Keys::Backspace, "kbs"},
    {Keys::PageUp, "kpp"},
    {Keys::PageDown, "knp"},
    {Keys::Home, "khome"},
    {Keys::End, "kend"},
    {Keys::Insert, "kich1"},
    {Keys::Delete, "kdch1"},
  };

  /*! terminfo capabilities used to find key sequences for keys we want to ignore (rather than
   *  writing junk to the current line like readline, editline, zle and friends do).
   */
  /*const*/ char *ignoredKeys[] = {
    "ka1", "ka3", "kb2", "kbeg", "kcbt", "kc1", "kc3", "kcan", "ktbc", "kclr", "kclo", "kcmd",
    "kcpy", "kcrt", "kctab", "kdl1", "krmir", "kel", "ked", "kext", "kf0", "kf0", "kf1", "kf2",
    "kf3", "kf4", "kf5", "kf6", "kf7", "kf8", "kf9", "kf10", "kf11", "kf12", "kf13", "kf14", "kf15",
    "kf16", "kf17", "kf18", "kf19", "kf20", "kf21", "kf22", "kf23", "kf24", "kf25", "kf26", "kf27",
    "kf28", "kf29", "kf30", "kf31", "kf32", "kf33", "kf34", "kf35", "kf36", "kf37", "kf38", "kf39",
    "kf40", "kf41", "kf42", "kf43", "kf44", "kf45", "kf46", "kf47", "kf48", "kf49", "kf50", "kf51",
    "kf52", "kf53", "kf54", "kf55", "kf56", "kf57", "kf58", "kf59", "kf60", "kf61", "kf62", "kf63",
    "kfnd", "khlp", "kil1", "kll", "kmrk", "kmsg", "kmous", "kmov", "knxt", "kopn", "kopt", "kprv",
    "kprt", "krdo", "kref", "krfr", "krpl", "krst", "kres", "ksav", "kBEG", "kCAN", "kCMD", "kCPY",
    "kCRT", "kDC", "kDL", "kslt", "kEND", "kEOL", "kEXT", "kind", "kFND", "kHLP", "kHOM", "kIC",
    "kLFT", "kMSG", "kMOV", "kNXT", "kOPT", "kPRV", "kPRT", "kri", "kRDO", "kRPL", "kRIT", "kRES",
    "kSAV", "kSPD", "khts", "kUND", "kspd", "kund",
  };

  //! Some default keys.
  static struct { Key key; /*const*/ char *seq; } backupBindings[] = {
    // Fairly standard arrow keys. Both standard keypad mode and standard application mode bindings.
    {Keys::Up, "\e[A"},    {Keys::Up, "\eOA"},
    {Keys::Down, "\e[B"},  {Keys::Down, "\eOB"},
    {Keys::Right, "\e[C"}, {Keys::Right, "\eOC"},
    {Keys::Left, "\e[D"},  {Keys::Left, "\eOD"},
    // Usually specified in terminfo, but not always.
    {Keys::Insert, "\e[2~"}, {Keys::Delete, "\e[3~"},
    {Keys::Home, "\e[1~"},   {Keys::End, "\e[4~"},
    {Keys::Home, "\e[H"},    {Keys::End, "\e[F"},
    {Keys::Home, "\eOH"},    {Keys::End, "\eOF"},
    {Keys::PageUp, "\e[5~"}, {Keys::PageDown, "\e[6~"},
    // termcap and terminfo give no way to specify these, but they're sent by most modern terminal
    // emulators (and presumably some old terminals). Note that there are also Shift variations
    // where the trailing number is one higher (it's a bitfield, Shift = 1, Alt = 2, Ctrl = 4, plus
    // one). TODO: add the Shift variants.
    {Keys::Alt + Keys::Up, "\e[1;3A"},    {Keys::Ctrl + Keys::Up, "\e[1;5A"},    {Keys::Ctrl + Keys::Alt + Keys::Up, "\e[1;7A"},
    {Keys::Alt + Keys::Down, "\e[1;3B"},  {Keys::Ctrl + Keys::Down, "\e[1;5B"},  {Keys::Ctrl + Keys::Alt + Keys::Down, "\e[1;7B"},
    {Keys::Alt + Keys::Right, "\e[1;3C"}, {Keys::Ctrl + Keys::Right, "\e[1;5C"}, {Keys::Ctrl + Keys::Alt + Keys::Right, "\e[1;7C"},
    {Keys::Alt + Keys::Left, "\e[1;3D"},  {Keys::Ctrl + Keys::Left, "\e[1;5D"},  {Keys::Ctrl + Keys::Alt + Keys::Left, "\e[1;7D"},
    // Likewise these ones.
    {Keys::Alt + Keys::Insert, "\e[2;3~"},   {Keys::Ctrl + Keys::Insert, "\e[2;5~"},   {Keys::Ctrl + Keys::Alt + Keys::Insert, "\e[2;7~"},
    {Keys::Alt + Keys::Delete, "\e[3;3~"},   {Keys::Ctrl + Keys::Delete, "\e[3;5~"},   {Keys::Ctrl + Keys::Alt + Keys::Delete, "\e[3;7~"},
    {Keys::Alt + Keys::Home, "\e[1;3H"},     {Keys::Ctrl + Keys::Home, "\e[1;5H"},     {Keys::Ctrl + Keys::Alt + Keys::Home, "\e[1;7H"},
    {Keys::Alt + Keys::End, "\e[1;3F"},      {Keys::Ctrl + Keys::End, "\e[1;5F"},      {Keys::Ctrl + Keys::Alt + Keys::End, "\e[1;7F"},
    {Keys::Alt + Keys::PageUp, "\e[5;3~"},   {Keys::Ctrl + Keys::PageUp, "\e[5;5~"},   {Keys::Ctrl + Keys::Alt + Keys::PageUp, "\e[5;7~"},
    {Keys::Alt + Keys::PageDown, "\e[6;3~"}, {Keys::Ctrl + Keys::PageDown, "\e[6;5~"}, {Keys::Ctrl + Keys::Alt + Keys::PageDown, "\e[6;7~"},
    // Alt + F<n> sometimes.
    {Keys::Ignored, "\e[12;3~"}, {Keys::Ignored, "\e[13;3~"}, {Keys::Ignored, "\e[14;3~"},
    {Keys::Ignored, "\e[15;3~"}, {Keys::Ignored, "\e[16;3~"}, {Keys::Ignored, "\e[17;3~"},
    {Keys::Ignored, "\e[18;3~"}, {Keys::Ignored, "\e[19;3~"}, {Keys::Ignored, "\e[20;3~"},
    {Keys::Ignored, "\e[21;3~"}, {Keys::Ignored, "\e[22;3~"}, {Keys::Ignored, "\e[23;3~"},
    // PuTTY produces these (as do, apparently, VT100s).
    {'*', "\eOj"}, {'+', "\eOk"}, {'+', "\eOl"}, {'-', "\eOm"}, {'.', "\eOn"}, {'/', "\eOo"}, 
    {'0', "\eOp"}, {'1', "\eOq"}, {'2', "\eOr"}, {'3', "\eOs"}, {'4', "\eOt"}, {'5', "\eOu"},
    {'6', "\eOv"}, {'7', "\eOw"}, {'8', "\eOx"}, {'9', "\eOy"},
    // NumLock sometimes generates ^[OP. These four keys seem to be pretending to be F1-F4.
    {Keys::Ignored, "\eOP"}, {'/', "\eOQ"}, {'*', "\eOR"}, {'-', "\eOS"},
    {Keys::Enter, "\eOM"}, 
  };

  Key kk[] = {Keys::Up, Keys::Up, Keys::Down, Keys::Down, Keys::Left, Keys::Right, Keys::Left, Keys::Right, 'b', 'a'};

  //------------------------------------------------------------------------------------------------
  /*! A map from character sequence to key. Pass this each character as it's read from the input
   *  stream, and it'll parse the characters into keys.
   */
  //------------------------------------------------------------------------------------------------
  class KeyMap
  {
  public:
    KeyMap(const TerminalKeys &terminalKeys);

    //! Incrementally parse an extra character.
    std::vector<Key> MapKey(int key);

    void Print(std::ostream &out);

  private:
    void AddMapping(char from, Key to);
  private:
    class Node;
    Node *root;
    Node *curr;
    std::vector<Key> buffer;
  };

  //------------------------------------------------------------------------------------------------
  /*! A node in KeyMap's parse tree.
   */
  //------------------------------------------------------------------------------------------------
  class KeyMap::Node
  {
  public:
    Node() : key() {}
    //! Add a mapping from \p mapping to key \p mapTo.
    void AddMapping(const char *mapping, Key mapTo)
    {
      if (*mapping)
      {
        Node *&slot = next[*mapping];
        if (!slot)
        {
          slot = new Node;
        }
        slot->AddMapping(mapping + 1, mapTo);
      }
      else if (!key)
      {
        key = mapTo;
      }
    }
    //! Is this a terminal node (ie, no descendents)?
    bool IsTerminal() const { return next.empty(); }
    //! Get the key corresponding to this node, or 0 if none.
    Key GetKey() const { return key; }
    //! Given a character, find the corresponding descendent node.
    Node *Map(int key) const
    {
      Next::const_iterator it = next.find(key);
      if (it != next.end())
      {
        return it->second;
      }
      return 0;
    }

    //! Debug printout of the map.
    void Print(std::ostream &out, int depth)
    {
      std::string indent(depth * 2, ' ');
      for (Next::iterator it = next.begin(); it != next.end(); ++it)
      {
        out << indent << it->first << " ->";
        if (it->second->GetKey())
        {
          out << " " << it->second->GetKey();
        }
        out << "\n";
        it->second->Print(out, depth + 1);
      }
    }
  private:
    int key;
    typedef std::map<int, Node*> Next;
    Next next;
  };

  //------------------------------------------------------------------------------------------------
  /*! Build the key map. Mappings from characters to keys will be read from terminfo.
   *
   *  \param keys  The user's current EOF (^D), suspend (^Z), interrupt (^C) and quit (^\) keys.
   */
  //------------------------------------------------------------------------------------------------
  KeyMap::KeyMap(const TerminalKeys &keys) : root(new Node), curr(0)
  {
    AddMapping(keys.eof, Keys::Eof);
    AddMapping(keys.susp, Keys::Suspend);
    AddMapping(keys.intr, Keys::Interrupt);
    AddMapping(keys.quit, Keys::Quit);

#define arraysize(x) (sizeof(x) / sizeof((x)[0]))
    // Non-trivial keys we care about.
    for (int n = 0; n < arraysize(specialKeys); ++n)
    {
      const char *str = tigetstr(specialKeys[n].capname);
      if (IsValidTiStr(str))
      {
        root->AddMapping(str, specialKeys[n].key);
      }
    }

    // Keys we want to ignore.
    for (int n = 0; n < arraysize(ignoredKeys); ++n)
    {
      const char *str = tigetstr(ignoredKeys[n]);
      if (IsValidTiStr(str))
      {
        root->AddMapping(str, Keys::Ignored);
      }
    }

    // Add in the mappings which termcap / terminfo get wrong.
    // Do this after the real mappings so that in the case of
    // a conflict, the real ones win.
    for (int n = 0; n < arraysize(backupBindings); ++n)
    {
      root->AddMapping(backupBindings[n].seq, backupBindings[n].key);
    }
#undef arraysize
  }

  void KeyMap::AddMapping(char from, Key to)
  {
    char str[2] = {from, 0};
    root->AddMapping(str, to);
  }

  void KeyMap::Print(std::ostream &out)
  {
    root->Print(out, 0);
  }

  std::vector<Key> KeyMap::MapKey(int key)
  {
    std::vector<Key> keys;

    // TODO: if it's been more than a second or so since the last key,
    // and curr->GetKey() is set, then we might want to return that. As
    // things stand, we don't return Escape until another key gets pressed.

    if (!curr)
    {
      curr = root;
    }

    buffer.push_back(key);
    curr = curr->Map(key);

    if (!curr)
    {
      // Key sequence can't resolve. Interpret first character as a key
      // by itself and try again. Not the fastest way to do this, but it'll do.
      // As a degenerate case, we also get here if an unbound key is pressed.
      keys.push_back(buffer[0]);
      std::vector<Key> oldBuffer;
      oldBuffer.swap(buffer);
      for (int n = 1; n < oldBuffer.size(); ++n)
      {
        std::vector<Key> results = MapKey(oldBuffer[n]);
        keys.insert(keys.end(), results.begin(), results.end());
      }
    }
    else if (curr->IsTerminal())
    {
      // Key sequence resolved.
      Key mapped = curr->GetKey();
      if (mapped)
      {
        keys.push_back(mapped);
      }
      else
      {
        TiBell();
      }
      curr = 0;
      buffer.clear();
    }
    else
    {
      // Key sequence not yet resolved.
    }

    //std::cerr << keys.size() << " keys" << std::endl;

    return keys;
  }
}


//--------------------------------------------------------------------------------------------------
/*! Terminal attribute.
 */
//--------------------------------------------------------------------------------------------------
class Redline::TerminalAttribute {} Redline::Attributes::Normal, Redline::Attributes::Error;

//--------------------------------------------------------------------------------------------------
/*! Decorated text implementation.
 */
//--------------------------------------------------------------------------------------------------
class DecoratedText::Internals
{
  struct DecoratedChar
  {
    DecoratedChar(const TerminalAttribute &, char _c) : c(_c) {}
    char c;

    bool operator==(const DecoratedChar &o) const { return c == o.c; }
    bool operator!=(const DecoratedChar &o) const { return !operator==(o); }
  };
public:
  Internals() : lines(1) {}

  typedef std::vector<DecoratedChar> Line;
  std::vector<Line> lines;

  void Add(const TerminalAttribute &attribute, const std::string &text)
  {
    for (std::string::const_iterator it = text.begin(); it != text.end(); ++it)
    {
      if (*it == '\n')
      {
        lines.push_back(Line());
        // Avoid too many reallocations.
        lines.back().reserve(80);
      }
      else
      {
        lines.back().push_back(DecoratedChar(attribute, *it));
      }
    }
  }

  void Prepare(int maxLines, int maxCols, int &cursorLine, int &cursorCol)
  {
    // TODO: Convert special characters to displayed versions.
    //  eg. "\x05\0xC2" -> "[^E][M-B]"

    // Wrap lines which are too long.
    // Warning: this loop is a little subtle; lines gets bigger as we progress.
    for (size_t line = 0; line < lines.size(); ++line)
    {
      // Note, if size == maxCols, we still wrap, since we need an extra column for the cursor.
      // If the cursor is after the last column, it goes on the extra (empty) line we create.
      if (lines[line].size() >= maxCols)
      {
        int newWidth = maxCols - 1;

        // Prefer to wrap at a space.
        for (int pos = newWidth - 1; pos > newWidth - 16 && pos > maxCols / 2; --pos)
        {
          if (lines[line][pos].c == ' ')
          {
            newWidth = pos + 1;
            break;
          }
        }

        lines.insert(lines.begin() + line + 1, Line(lines[line].begin() + newWidth, lines[line].end()));
        lines[line].resize(newWidth, DecoratedChar(Attributes::Normal, ' '));

        // Add a trailing backslash to continued lines.
        lines[line].resize(maxCols, DecoratedChar(Attributes::Normal, ' '));
        lines[line][maxCols - 1] = DecoratedChar(Attributes::Normal, '\\');

        // Move the cursor with the text.
        if (cursorLine == line && cursorCol >= newWidth)
        {
          ++cursorLine;
          cursorCol -= newWidth;
        }
        else if (cursorLine > line)
        {
          ++cursorLine;
        }
      }
    }

    // Remove lines farthest from cursor if too many lines.
    if (lines.size() > maxLines)
    {
      int first = std::max(0, std::min<int>(cursorLine - maxLines / 2, lines.size() - maxLines));
      lines.erase(lines.begin() + first + maxLines, lines.end());
      lines.erase(lines.begin(), lines.begin() + first);
      cursorLine -= first;
    }
  }
};

DecoratedText::DecoratedText() : internals(new Internals) {}
DecoratedText::~DecoratedText() { delete internals; }
void DecoratedText::Add(const TerminalAttribute &attribute, const std::string &text)
{
  internals->Add(attribute, text);
}


//--------------------------------------------------------------------------------------------------
/*! Terminal implementation.
 */
//--------------------------------------------------------------------------------------------------
class Terminal::Internals
{
public:
  Internals();
  ~Internals();

  void DoWaitForKey(bool wait);

  void Redisplay();
  void Commit(bool addNewline);

  void WriteChar(char ch);
  void Flush() { fflush(stdout); }

  int GetCursorCol();

  void UpdateSize() { GetTerminalSize(columns, lines); }
  int GetColumns() { return columns; }
  int GetLines() { return lines; }

  bool CursorLeft(int by);
  bool CursorRight(int by);
  bool CursorUp(int by);
  bool CursorDown(int by);
  bool CursorTo(int line, int col);

  void SetText(const DecoratedText::Internals &text, int cursorLine, int cursorCol);

public:
  TerminalData oldTerminalData;
  TerminalData newTerminalData;
  int suspended;

  void Enable()
  {
    if (--suspended == 0)
    {
      fflush(stdout);
      newTerminalData.Set();
      // Turn on 'keypad-transmit', AKA 'send me the key sequences you
      // said you would' mode. Otherwise arrow keys come in garbled.
      TiStr("smkx");
    }
  }
  void Disable()
  {
    if (suspended++ == 0)
    {
      TiStr("rmkx");
      fflush(stdout);
      oldTerminalData.Set();
    }
  }

  //! Input handling.
  //@{
  KeyMap keyMap;
  std::deque<Key> buffer;
  bool meta;
  int interruptFd[2];
  //@}

  //! Output handling.
  //@{
  DecoratedText::Internals text;
  int lines, columns;
  int cursorLine, cursorCol;
  //@}

  bool renderDebug;
  int renderDebugPos;
};

Terminal::Internals::Internals() :
  oldTerminalData(), newTerminalData(oldTerminalData), suspended(1),
  keyMap(oldTerminalData.GetKeys()), meta(false),
  text(), cursorLine(), cursorCol(-1), renderDebug(false), renderDebugPos(0)
{
  newTerminalData.SetRaw();
  Enable();

  //keyMap.Print(std::cerr);
  UpdateSize();

  pipe(interruptFd);
}

Terminal::Internals::~Internals()
{
  Disable();
}

Terminal::Terminal() : internals((InitTerminal(), new Internals))
{
}

Terminal::~Terminal()
{
  Commit();
  delete internals;
}


class SuspendTerminal::Internals
{
public:
  Internals(Terminal &_terminal) : terminal(_terminal) {}
  Terminal &terminal;
};

SuspendTerminal::SuspendTerminal(Terminal &terminal) :
  internals(new Internals(terminal))
{
  internals->terminal.internals->Disable();
}
SuspendTerminal::~SuspendTerminal()
{
  internals->terminal.internals->Enable();
  delete internals;
}

void Terminal::Internals::DoWaitForKey(bool wait)
{
  // Read characters and map them to keys.
  while (buffer.empty())
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    FD_SET(interruptFd[0], &fds);

    timeval t = {0};
    while (select(interruptFd[0]+1, &fds, 0, 0, wait ? 0 : &t) == -1) {}

    if (FD_ISSET(interruptFd[0], &fds))
    {
      int c = 0;
      read(interruptFd[0], &c, 1);
      buffer.push_back(Keys::AsyncInterrupted);
      break;
    }

    if (!FD_ISSET(0, &fds) && !wait)
    {
      break;
    }

    // Read characters until we get a complete key.
    unsigned char c = 0;
    fflush(stdout);
    while (read(1, &c, 1) < 1 && errno == EINTR) {}
    int ch = c;
    //std::cerr << "Char " << ch << std::endl;

    std::vector<Key> keys = keyMap.MapKey(ch);
    buffer.insert(buffer.begin(), keys.begin(), keys.end());

    // Translate Esc, Key -> Alt + Key.
    for (std::deque<Key>::iterator it = buffer.begin(); it != buffer.end(); /*increment in loop*/)
    {
      if (meta)
      {
        *it += Keys::Alt;
        meta = false;
        ++it;
      }
      else if (*it == 27)
      {
        meta = true;
        it = buffer.erase(it);
      }
      else
      {
        if (kk[renderDebugPos] == *it)
        {
          if (++renderDebugPos == sizeof(kk)/sizeof(kk[0]))
          {
            renderDebug = !renderDebug;
            renderDebugPos = 0;
            it = buffer.erase(it);
            buffer.insert(it, Keys::Backspace);
            continue;
          }
        }
        else
        {
          renderDebugPos = 0;
        }
        ++it;
      }
    }
  }
}

void Terminal::WaitForKey()
{
  internals->DoWaitForKey(true);
}

bool Terminal::HaveKey()
{
  internals->DoWaitForKey(false);
  return !internals->buffer.empty();
}

//--------------------------------------------------------------------------------------------------
/*! Read one key from the terminal.
 *  \return The key read, or 0 if no key is available.
 */
//--------------------------------------------------------------------------------------------------
Key Terminal::GetKey()
{
  // Translate Esc, Key -> Alt + Key.
  if (!internals->buffer.empty())
  {
    Key key = internals->buffer.front();
    internals->buffer.pop_front();
    return key;
  }
  return 0;
}

void Terminal::AsyncInterruptWaitForKey()
{
  while (write(internals->interruptFd[1], "", 1) < 1 && errno == EINTR) {}
}

bool Terminal::Internals::CursorLeft(int by)
{
  if (by < 0) { return CursorRight(-by); }

  if (!by) {}
  else if (cursorCol == by && TiStr("cr")) {}
  else if (char *hpa = GetTiStr("hpa"))
  {
    putp(tparm(hpa, cursorCol - by));
  }
  else if (char *cub = GetTiStr("cub"))
  {
    putp(tparm(cub, by));
  }
  else if (char *cub1 = GetTiStr("cub1"))
  {
    for (int n = 0; n < by; ++n) { putp(cub1); }
  }
  else
  {
    return false;
  }

  cursorCol -= by;
  return true;
}

bool Terminal::Internals::CursorRight(int by)
{
  if (by < 0) { return CursorLeft(-by); }

  if (!by) {}
  else if (char *hpa = GetTiStr("hpa"))
  {
    putp(tparm(hpa, cursorCol + by));
  }
  else if (char *cuf = GetTiStr("cuf"))
  {
    putp(tparm(cuf, by));
  }
  else if (char *cuf1 = GetTiStr("cuf1"))
  {
    for (int n = 0; n < by; ++n) { putp(cuf1); }
  }
  else
  {
    return false;
  }

  cursorCol += by;
  return true;
}

bool Terminal::Internals::CursorUp(int by)
{
  if (by < 0) { return CursorDown(-by); }

  if (!by) {}
  else if (char *cuu = GetTiStr("cuu"))
  {
    putp(tparm(cuu, by));
  }
  else if (char *cuu1 = GetTiStr("cuu1"))
  {
    for (int n = 0; n < by; ++n) { putp(cuu1); }
  }
  else if (char *cub1 = HasTiFlag("bw") ? GetTiStr("cub1") : 0)
  {
    // Backspace wraps, so columns * bs goes up one line.
    for (int n = 0; n < by * GetColumns(); ++n) { putp(cub1); }
  }
  else
  {
    return false;
  }

  cursorLine -= by;
  return true;
}

bool Terminal::Internals::CursorDown(int by)
{
  if (by < 0) { return CursorUp(-by); }

  if (!by) {}
  else if (char *cud = GetTiStr("cud"))
  {
    putp(tparm(cud, by));
  }
  else if (char *cud1 = GetTiStr("cud1"))
  {
    for (int n = 0; n < by; ++n) { putp(cud1); }
  }
  else
  {
    return false;
  }

  cursorLine += by;
  return true;
}

int Terminal::Internals::GetCursorCol()
{
  if (cursorCol == -1)
  {
    // Don't really care if this fails. If we print a newline here,
    // we definitely mess up our output, whereas if we don't, we only
    // potentially mess up.
    TiStr("cr");
    cursorCol = 0;
  }
  return cursorCol;
}

bool Terminal::Internals::CursorTo(int line, int col)
{
  if (line > cursorLine && col == 0)
  {
    while (line != cursorLine)
    {
      WriteChar('\n');
    }
    return true;
  }
  else
  {
    return CursorLeft(GetCursorCol() - col) && CursorUp(cursorLine - line);
  }
}

void Terminal::Internals::WriteChar(char ch)
{
  if (ch == '\n')
  {
    TiStr("nel") || putchar('\n');

    ++cursorLine;
    cursorCol = 0;
  }
  else
  {
    putchar(ch);

    if (++cursorCol == GetColumns())
    {
      if (HasTiFlag("xenl"))
      {
        // Needed for predictable behaviour.
        putchar('\n');
      }
      ++cursorLine;
      cursorCol = 0;
    }
  }
}

namespace
{
  struct WithoutCursor
  {
    WithoutCursor() { TiStr("civis"); }
    ~WithoutCursor() { TiStr("cnorm"); fflush(stdout); }
  };
}

//--------------------------------------------------------------------------------------------------
/*! Set the currently-displayed terminal text.
 */
//--------------------------------------------------------------------------------------------------
void Terminal::SetText(const DecoratedText &_text, int cursorLine, int cursorCol)
{
  // Only need to bother with this on SIGWINCH, unless the terminal size changes
  // while we're not the foreground process. But the cost is small, so let's do
  // it anyway.
  internals->UpdateSize();

  DecoratedText::Internals text = *_text.internals;
  text.Prepare(internals->lines, internals->columns, cursorLine, cursorCol);
  internals->SetText(text, cursorLine, cursorCol);
}
void Terminal::Internals::SetText(const DecoratedText::Internals &newText,
                                  int cLine, int cCol)
{
  WithoutCursor hideCursor;

  // TODO: Handle 'os' capability somehow (no editing?).

  if (renderDebug)
  {
    static int n = 0;
    n = (n + 1) % 12;
    if (n < 6)
    {
      fputs("\x1b[1;3", stdout);
      putchar('1' + n);
    }
    else
    {
      fputs("\x1b[22;3", stdout);
      putchar('1' + n - 6);
    }
    putchar('m');
  }

  // There's a nice dynamic programming algorithm to compute an optimal set of edits
  // to convert one line of newText into another, which we certainly could use here (if
  // the terminal supports character insert/delete commands). For now, just do a char
  // by char update.
  typedef DecoratedText::Internals::Line Line;
  Line blankLine;
  for (size_t line = 0; line < newText.lines.size() || line < text.lines.size(); ++line)
  {
    const Line &from = (line < text.lines.size() ? text.lines[line] : blankLine);
    const Line &to = (line < newText.lines.size() ? newText.lines[line] : blankLine);

    if (line >= text.lines.size() && cursorLine != line)
    {
      // Possibly the first time writing to this line. Use newline rather than cursor
      // down in order to ensure the screen scrolls down if necessary.
      if (CursorDown(line - cursorLine - 1))
      {
        WriteChar('\n');
      }
    }

    for (size_t col = 0; col < from.size() || col < to.size(); ++col)
    {
      if (col >= from.size() || col >= to.size() || from[col] != to[col])
      {
        // Make sure we're at or before column \p col, on the right line.
        if (!CursorTo(line, col) && (line != cursorLine || cursorCol > col) && !CursorTo(line, 0))
        {
          // About to call SetText. This should never be problematic, but better safe
          // than stack overflow.
          static bool alreadyHere = false;
          if (!alreadyHere)
          {
            alreadyHere = true;
            Commit(true);
            SetText(newText, cLine, cCol);
            alreadyHere = false;
          }
          return;
        }

        // Copy text up to the right point. Normally this just means writing the
        // current character, but if CursorTo fails, then we may be retyping the
        // whole line.
        while (cursorLine == line && cursorCol <= col)
        {
          WriteChar(cursorCol < to.size() ? to[cursorCol].c : ' ');
        }
      }
    }
  }

  // This won't work on various types of dumb terminals. FIXME: we can fake this
  // up with \r followed by retyping the whole line.
  CursorTo(cLine, cCol);
  text = newText;

  Flush();
}

//--------------------------------------------------------------------------------------------------
/*! Hide (reset) the current text.
 */
//--------------------------------------------------------------------------------------------------
/*static*/ void Terminal::Hide()
{
  internals->SetText(DecoratedText::Internals(), 0, 0);
  internals->cursorCol = -1;
}

//--------------------------------------------------------------------------------------------------
/*! Commit the currently-set text, such that it can no longer be updated by SetText.
 *
 *  \param addNewline  If \c true, add a newline to the end of the text. Otherwise, the cursor will
 *         be left somewhere on the last line of the text. Set this to \c false if you are about to
 *         perform some action which typically adds a newline for itself (such as SIGTSTP). If this
 *         is set to \c false, the cursor column is not reset; this is typically what you want,
 *         since shells tend to restore it when bringing the process back to the foreground, but we
 *         will restore it ourselves later anyway (if we can) in case the shell doesn't.
 */
//--------------------------------------------------------------------------------------------------
void Terminal::Commit(bool addNewline)
{
  internals->Commit(addNewline);
}
void Terminal::Internals::Commit(bool addNewline)
{
  int line = text.lines.size() - 1;
  int column = text.lines[line].size();
  if (column >= GetColumns())
  {
    // Can't go to this column. The next line is present but blank, so just go there.
    // Prepare() should actually guarantee that the last line isn't this long, so this is
    // unreachable at the moment.
    CursorTo(line + addNewline, 0);
  }
  else
  {
    CursorTo(line, column);
    if (addNewline)
    {
      WriteChar('\n');
    }
  }

  cursorLine = 0;
  if (addNewline)
  {
    if (cursorCol != 0)
    {
      // Something went wrong. Do the best we can.
      WriteChar('\n');
    }
  }

  Flush();

  // Cursor column now 'unknown'.
  cursorCol = -1;
  text = DecoratedText::Internals();
}

//--------------------------------------------------------------------------------------------------
/*! Redisplay the current text, in the case that you think the terminal is corrupted. This should
 *  usually only be triggered by the user.
 */
//--------------------------------------------------------------------------------------------------
/*static*/ void Terminal::Redisplay()
{
  internals->Redisplay();
}
void Terminal::Internals::Redisplay()
{
  DecoratedText::Internals currText = text;
  int line = cursorLine, col = cursorCol;

  if (TiStr("clear"))
  {
    cursorCol = 0;
    cursorLine = 0;
  }
  else
  {
    // Could send lots of newlines here.
    if (!CursorTo(0, 0))
    {
      CursorTo(line + 1, 0);
      cursorLine = 0;
    }
    cursorCol = -1;
  }
  text = DecoratedText::Internals();

  SetText(currText, line, col);
}

//--------------------------------------------------------------------------------------------------
/*! Get the number of rows available in the terminal.
 */
//--------------------------------------------------------------------------------------------------
int Terminal::GetNumRows()
{
  internals->UpdateSize();
  return internals->GetLines();
}

//--------------------------------------------------------------------------------------------------
/*! Get the number of columns available in the terminal.
 */
//--------------------------------------------------------------------------------------------------
int Terminal::GetNumColumns()
{
  internals->UpdateSize();
  return internals->GetColumns();
}

//--------------------------------------------------------------------------------------------------
/*! Emit a warning bell / screen flash.
 */
//--------------------------------------------------------------------------------------------------
/*static*/ void Terminal::Bell()
{
  InitTerminal();
  TiBell();
}
