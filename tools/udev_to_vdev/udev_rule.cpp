/*

Copyright (c) 2014, Gravis (https://github.com/GravisZro), Jude Nelson (judecn@gmail.com)

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/

#include "udev_rule.h"

bool udev_field_t::operator << (const char& input) throw(const char*)
{
  if(full())
    throw("field overflow!");

  if(op.is_locked())
  {
    if(input == '"')
    {
      if(!value.empty() && value.at(value.size() - 1) != '\\')
        value.lock();
    }
    else
      value.push_back(input);
  }
  else
  {
    switch(input)
    {
      case '{':
        if(!subname.is_locked())
          throw ("unexpected opening curled bracket");
        subname.unlock();
        name.lock();
        break;

      case '}':
        if(subname.is_locked())
          throw ("unexpected closing curled bracket");
        subname.lock();
        break;

      case '=':
        name.lock();
        op.push_back(input);
        if(op.size() == 2)
          op.lock();
        break;

      case ':':
      case '+':
      case '!':
        name.lock();
        if(!op.empty())
          throw ("bad operator");
        op.push_back(input);
        break;

      case '"':
        if(!name.is_locked() || !subname.is_locked())
          throw ("unexpected quotation");
        op.lock();
        break;

      case '_':
      case 'a' ... 'z':
      case 'A' ... 'Z':
        if(!name.is_locked())
          name.push_back(input);
        else if(!subname.is_locked())
          subname.push_back(input);
        else
          throw ("expected quotation");
        break;

      case ' ':
      case '\t':
        if(!name.is_locked() && !name.empty())
          name.lock();
        break; // ignore whitespace

      default:
        throw ("invalid character");
    }
  }

  return !full();
}

void udev_field_t::clear(void)
{
  name.clear();
  name.unlock();

  subname.clear();
  subname.lock();

  op.clear();
  op.unlock();

  value.clear();
  value.unlock();
}

bool udev_rule_t::operator << (const char& input) throw(const char*)
{
  if(full())
    throw("rule overflow!");

  udev_field_t& back = fields.back();

  if(!comment.is_locked())
  {
    if(input == '\n')
      comment.lock();
    comment.push_back(input);
  }
  else if(input == '#')
  {
    comment.unlock();
    comment.push_back(input);
  }
  else if (input == '\\' && back.name.empty())
    line_continues = true;
  else if (input == '\n')
  {
    if(!line_continues)
      fields.lock();
    line_continues = false;
  }
  else if(back.full())
  {
    switch(input)
    {
      case '\n':
      case ' ':
      case '\t':
        break; // ignore whitespace

      case ',':
        if(line_continues)
          throw ("expected endline");
        if(!back.empty())
          fields.emplace_back();
        break;

      default:
        throw ("unexpected character between fields");
    }
  }
  else
    back << input;

  return !full();
}

void udev_rule_t::clear(void)
{
  fields.clear();
  fields.emplace_back();
  line_continues = false;
  comment.lock();
}

void udev_rule_file_t::operator << (const char& input) throw(const char*)
{
  udev_rule_t& back = rules.back();
  back << input;
  if(back.full())
    rules.emplace_back();
}

void udev_rule_file_t::clear(void)
{
  rules.clear();
  rules.emplace_back();
}
