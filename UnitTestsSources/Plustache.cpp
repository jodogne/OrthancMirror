#include "gtest/gtest.h"

#include <include/template.hpp>


class OrthancPlustache : public Plustache::template_t
{
public:
protected:
  virtual std::string get_template(const std::string& tmpl)
  {
    //printf("OK [%s]\n", tmpl.c_str());
    return Plustache::template_t::get_template(tmpl);
  }

  virtual std::string get_partial(const std::string& partial) const
  {
    //printf("OK2 [%s]\n", partial.c_str());
    //return Plustache::template_t::get_partial(partial);
    return "<li>{{name}}</li>";
  }
};


TEST(Plustache, Basic1)
{
  PlustacheTypes::ObjectType ctx;
  ctx["title"] = "About";

  OrthancPlustache t;
  ASSERT_EQ("<h1>About</h1>", t.render("<h1>{{title}}</h1>", ctx));
}


TEST(Plustache, Basic2)
{
  Plustache::Context ctx;
  ctx.add("title", "About");

  OrthancPlustache t;
  ASSERT_EQ("<h1>About</h1>", t.render("<h1>{{title}}</h1>", ctx));
}


TEST(Plustache, Context)
{
  PlustacheTypes::ObjectType a;
  a["name"] = "Orthanc";

  PlustacheTypes::ObjectType b;
  b["name"] = "Jodogne";

  PlustacheTypes::CollectionType c;
  c.push_back(a);
  c.push_back(b);

  Plustache::Context ctx;
  ctx.add("items", c);

  OrthancPlustache t;
  ASSERT_EQ("<ul><li>Orthanc</li><li>Jodogne</li></ul>",
            t.render("<ul>{{#items}}<li>{{name}}</li>{{/items}}</ul>", ctx));
}


TEST(Plustache, Partials)
{
  PlustacheTypes::ObjectType a;
  a["name"] = "Orthanc";

  PlustacheTypes::ObjectType b;
  b["name"] = "Jodogne";

  PlustacheTypes::CollectionType c;
  c.push_back(a);
  c.push_back(b);

  Plustache::Context ctx;
  ctx.add("items", c);

  OrthancPlustache t;
  ASSERT_EQ("<ul><li>Orthanc</li><li>Jodogne</li></ul>",
            t.render("<ul>{{#items}}{{>partial}}{{/items}}</ul>", ctx));
}


