/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#if ORTHANC_PLUSTACHE_ENABLED == 1

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

#endif
