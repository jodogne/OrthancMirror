#include "gtest/gtest.h"

#include <include/template.hpp>

TEST(Plustache, Basic)
{
  std::map<std::string, std::string> ctx;
  ctx["title"] = "About";

  Plustache::template_t t;
  ASSERT_EQ("<h1>About</h1>", t.render("<h1>{{title}}</h1>", ctx));
}

