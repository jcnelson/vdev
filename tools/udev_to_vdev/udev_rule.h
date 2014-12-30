/*

Copyright (c) 2014, Gravis (https://github.com/GravisZro), Jude Nelson (judecn@gmail.com)

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/

#ifndef UDEV_FIELD_H
#define UDEV_FIELD_H

#include <string>
#include <list>

template<typename type>
class lockable : public type
{
public:
  inline lockable(void) : m_locked(false) { }
  inline bool is_locked(void) const { return m_locked; }
  inline void lock  (void) { m_locked = true ; }
  inline void unlock(void) { m_locked = false; }
private:
  bool m_locked;
};

// parser a single field
struct udev_field_t
{
  inline udev_field_t(void) { subname.lock(); }
  bool operator << (const char& input) throw(const char*); // returns false when full
  void clear(void);
  inline bool full(void) const { return value.is_locked(); }
  inline bool empty(void) const { return value.empty(); }

  lockable<std::string> name;
  lockable<std::string> subname;
  lockable<std::string> op;
  lockable<std::string> value;
};

// parses all fields in rule
struct udev_rule_t
{
  inline udev_rule_t(void) : line_continues(false) { fields.emplace_back(); comment.lock(); }
  bool operator << (const char& input) throw(const char*); // returns false when full
  void clear(void);
  inline bool full(void) const { return fields.is_locked(); }
  inline bool empty(void) const { return fields.back().empty(); }

  lockable<std::list<udev_field_t>> fields;
  bool line_continues;
  lockable<std::string> comment;
};

struct udev_rule_file_t
{
  inline udev_rule_file_t(void) { rules.emplace_back(); }
  void operator << (const char& input) throw(const char*);
  void clear(void);

  std::list<udev_rule_t> rules;
};
#endif // UDEV_FIELD_H
