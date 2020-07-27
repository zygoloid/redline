#include "redline/editor.hpp"
#include "redline/emacs.hpp"
#include "redline/history.hpp"
#include "redline/text.hpp"

#include <iostream>

class TestMode : public Redline::EmacsMode
{
public:
  TestMode(Redline::Editor &editor) : EmacsMode(editor), history(20) {}
  virtual bool TextIsComplete()
  {
    return GetText().Get(GetText().End().Move(-1, 0), GetText().End()) != "\\";
  }
  virtual void Execute(const std::string &line)
  {
    //std::cout << int(line[0]) << std::endl;
    sleep(1);
    //std::cout << "> " << line << std::endl;
    //system(line.c_str());
  }
  virtual Redline::History *GetHistory()
  {
    return &history;
  }

private:
  Redline::VectorHistory history;
};

int main()
{
  Redline::Editor editor;
  new TestMode(editor);
  editor.Run();
}
