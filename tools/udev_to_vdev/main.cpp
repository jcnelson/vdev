/*

Copyright (c) 2014, Gravis (https://github.com/GravisZro), Jude Nelson (judecn@gmail.com)

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/

#include <iostream>
#include <fstream>
#include <string>

#include "udev_rule.h"

int main(int argc, char **argv)
{
  if(argc != 2)
  {
    std::cout << "Usage: "
              << argv[0]
              << " <filename>"
              << std::endl;
    return 0;
  }

  std::ifstream udev_file;
  udev_file.open(argv[1], std::ios_base::in);

  if(!udev_file.is_open())
  {
    std::cout << "bad file name"
              << std::endl;
    return -1;
  }

  udev_rule_file_t rule_file;
  char buffer;
  try
  {
    while(udev_file.get(buffer).good())
      rule_file << buffer;
  }
  catch(const char* msg)
  {
    std::cout << "error: " << msg << std::endl;
    std::cout << "character: '" << buffer << "'" << std::endl;
    std::cout << "character (hex): " << std::hex << (int)buffer << std::endl;
    std::cout << std::endl;
  }

  for(auto rule = rule_file.rules.begin(); rule != rule_file.rules.end(); ++rule)
  {
    if(rule->fields.back().empty())
      continue; // skip empty rule

    if(!rule->comment.empty())
      std::cout << "COMMENT:" << std::endl << rule->comment << std::endl;
    std::cout << "RULE:" << std::endl;
    for(auto pos = rule->fields.begin(); pos != rule->fields.end(); ++pos)
    {
      std::cout << "  FIELD: " << pos->name;
      if(!pos->subname.empty())
        std::cout << "{" << pos->subname << "}";
      if(!pos->value.empty())
        std::cout << pos->op << "\"" << pos->value << "\"";
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }

  return 0;
}

