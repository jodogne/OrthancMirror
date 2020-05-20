#include <string>

struct OrthancLinesIterator;

OrthancLinesIterator* OrthancLinesIterator_Create(const std::string& content)
{
  return NULL;
}

bool OrthancLinesIterator_GetLine(std::string& target,
                                  const OrthancLinesIterator* iterator)
{
  return false;
}

void OrthancLinesIterator_Next(OrthancLinesIterator* iterator)
{
}

void OrthancLinesIterator_Free(OrthancLinesIterator* iterator)
{
}
