#include "redline/history.hpp"

using namespace Redline;

VectorHistory::VectorHistory(size_t maxLines) :
  lines(), start(), maxLines(maxLines)
{
}

static size_t FromCursor(HistoryCursor c) { return reinterpret_cast<size_t>(c) - 1; }
static HistoryCursor ToCursor(size_t n) { return reinterpret_cast<HistoryCursor>(n + 1); }

HistoryCursor VectorHistory::Begin() { return ToCursor(start); }
HistoryCursor VectorHistory::End() { return ToCursor(start + lines.size()); }
HistoryCursor VectorHistory::Next(HistoryCursor pos) { return ToCursor(FromCursor(pos) + 1); }
HistoryCursor VectorHistory::Previous(HistoryCursor pos) { return ToCursor(FromCursor(pos) - 1); }

std::string VectorHistory::Get(HistoryCursor pos)
{
  size_t n = FromCursor(pos) - start;
  return n < lines.size() ? lines[n] : std::string();
}

void VectorHistory::Add(const std::string &text)
{
  lines.push_back(text);
  if (lines.size() > maxLines)
  {
    lines.pop_front();
    ++start;
  }
}
