#pragma once
#ifndef INDENTER_HPP
#define INDENTER_HPP

#include <iostream>
#include <sstream>
#include <vector>

namespace indent {
  class indentbuf: public std::streambuf {
  public:
    static const int idx;

    indentbuf(std::ostream& orig, size_t indent)
    : orig(orig),
      orig_buf(orig.rdbuf()),
      comment_on(false),
      suppress(false),
      line_indent(indent),
      wrap(70),
      lookback(30),
      string_wrap_indent(2)
    {
      orig.rdbuf(this);
      update_prefix();
    }

    virtual ~indentbuf() {
      pubsync();
      orig.rdbuf(orig_buf);
    }

    // This one needs to be int_type so that it can be + or - to the current indent.
    void adjust_indent(int_type adj) {
      pubsync();
      line_indent += adj;
      update_prefix();
    }

    void set_indent(size_t val) {
      pubsync();
      line_indent = val;
      update_prefix();
    }

    void push_indent(size_t val) {
      pubsync();
      line_indent_stack.push_back(line_indent);
      line_indent = val;
      update_prefix();
    }

    void pop_indent() {
      pubsync();
      if (line_indent_stack.size() == 0) return;
      line_indent = line_indent_stack.back();
      line_indent_stack.pop_back();
      update_prefix();
    }

    void set_wrap(size_t wrap_val, size_t lookback_val, size_t string_wrap_indent_val=2) {
      pubsync();
      wrap = wrap_val;
      lookback = lookback_val;
      string_wrap_indent = string_wrap_indent_val;
    }

    void set_suppress(bool suppress_val) {
      pubsync();
      suppress = suppress_val;
    }

    // Comment will only last until the next natural newline.
    void comment() {
      pubsync();
      comment_on = true;
    }

  protected:
    int_type overflow(int_type ch) {
      // If we get an eof just return the right stuff.
      if (traits_type::eq_int_type(ch, traits_type::eof())) {
        return traits_type::not_eof(ch);
      }

      if (suppress) {
        return orig_buf->sputc(ch);
      }

      buffer += ch;
      if (ch == '\n' || ch == '\r') {
        buffer = prefix + buffer;
        // Ok.. we just got a new line, so output the buffer in wrapped chunks.
        while (buffer.size() > wrap) {
          // find the last space before the wrap point.
          size_t wrap_pos = buffer.find_last_of("\n ", wrap);
          size_t extra = 1;
          // If we didn't find a space, just wrap at the wrap point.
          if (wrap_pos == std::string::npos || (wrap - wrap_pos > lookback)) { wrap_pos = wrap; extra = 0; }
          else wrap_pos = avoid_marker_line_start(buffer, wrap_pos);
          if (comment_on) orig_buf->sputc(';');
          orig_buf->sputn(buffer.c_str(), wrap_pos);
          orig_buf->sputc('\n');
          buffer = buffer.substr(wrap_pos + extra);
          // If the remaining buffer is just a newline, clear it
          if (buffer == "\n") buffer.clear();
          if (buffer.size() > 0) {
            buffer = prefix + std::string(string_wrap_indent, ' ') + buffer;
          }
        }
        if (buffer.size() > 0) {
          if (comment_on) orig_buf->sputc(';');
          orig_buf->sputn(buffer.c_str(), buffer.size());
        }
        buffer.clear();
        // all output, turn off comment now.
        comment_on = false;
      }
      return ch;
    }

    int_type sync(void) {
      if (buffer.size() > 0) {
        if (comment_on) orig_buf->sputc(';');
        buffer = prefix + buffer;
        orig_buf->sputn(buffer.c_str(), buffer.size());
        buffer.clear();
      }
      return orig_buf->pubsync();
    }
  private:
    /**
     * @brief Tests whether text at pos starts with a report line marker.
     *
     * The report format uses a single character followed by a space at the start of a line to
     * introduce a new entry: '+' for structures, '-' for other factions' units, '*' for own
     * units, and '=', ':', '%', '!' for the attitude variants of a unit line.
     *
     * @param text buffer being wrapped
     * @param pos offset at which a wrapped line would begin
     * @return true if a line starting at pos would be read as a new entry by a report parser
     */
    bool starts_line_marker(const std::string& text, size_t pos) const {
      static const std::string markers = "-+*=:%!";
      return pos + 1 < text.size() && markers.find(text[pos]) != std::string::npos && text[pos + 1] == ' ';
    }

    /**
     * @brief Moves a wrap point back so the wrapped line does not begin with a line marker.
     *
     * Prose may legitimately contain " - " (an object description, for instance), and if the
     * line happens to break right before it, the wrapped continuation is indistinguishable from
     * a unit line at the same indent, which breaks report parsers. Walk back word by word until
     * the remainder no longer starts with a marker.
     *
     * If no acceptable earlier break exists within the lookback window, the original wrap point
     * is kept - a badly wrapped line is better than an arbitrarily short one.
     *
     * @param text buffer being wrapped
     * @param wrap_pos offset of the space chosen as the break point
     * @return the break point to use, at or before wrap_pos
     * @see starts_line_marker
     */
    size_t avoid_marker_line_start(const std::string& text, size_t wrap_pos) const {
      size_t candidate = wrap_pos;
      while (starts_line_marker(text, candidate + 1)) {
        if (candidate == 0) break;
        size_t prev = text.find_last_of("\n ", candidate - 1);
        if (prev == std::string::npos || wrap - prev > lookback) break;
        candidate = prev;
      }
      return starts_line_marker(text, candidate + 1) ? wrap_pos : candidate;
    }

    void update_prefix() {
      if (line_indent < 0) line_indent = 0;
      // Just make sure we never get a huge indent.
      if (line_indent > 30) line_indent = 30;
      if (line_indent) prefix = std::string(line_indent, ' ');
      else prefix.clear();
    }

    std::ostream& orig;
    std::streambuf *orig_buf;
    bool comment_on;
    bool suppress;
    int_type line_indent;
    size_t wrap;
    size_t lookback;
    size_t string_wrap_indent;
    std::string buffer;
    std::string prefix;
    std::vector<int_type> line_indent_stack;
  };

  std::ostream &wrap(std::ostream &os);
  std::ostream &comment(std::ostream &os);
  std::ostream &incr(std::ostream &os);
  std::ostream &decr(std::ostream &os);
  std::ostream &clear(std::ostream &os);

  struct wrap_data { size_t wrap; size_t lookback; size_t string_wrap_indent; };
  inline wrap_data wrap(size_t wrap=70, size_t lookback=30, size_t string_wrap_indent=2) {
    return { wrap, lookback, string_wrap_indent };
  }
  template<typename _CharT, typename _Traits>
  inline std::basic_ostream<_CharT, _Traits>& operator<<(std::basic_ostream<_CharT, _Traits>& os, wrap_data data) {
    if(os.pword(indentbuf::idx) == nullptr) {
      indentbuf *newbuf = new indentbuf(os, 0);
      newbuf->set_wrap(data.wrap, data.lookback, data.string_wrap_indent);
      os.pword(indentbuf::idx) = newbuf;
    } else {
      indentbuf *buf = static_cast<indentbuf *>(os.pword(indentbuf::idx));
      buf->set_wrap(data.wrap, data.lookback, data.string_wrap_indent);
    }
    return os;
  }

  struct indent_data { size_t indent; };
  inline indent_data set_indent(size_t indent=2) { return { indent }; }
  template<typename _CharT, typename _Traits>
  inline std::basic_ostream<_CharT, _Traits>& operator<<(std::basic_ostream<_CharT, _Traits>& os, indent_data data) {
    if(os.pword(indentbuf::idx) == nullptr) {
      indentbuf *newbuf = new indentbuf(os, data.indent);
      os.pword(indentbuf::idx) = newbuf;
    } else {
      indentbuf *buf = static_cast<indentbuf *>(os.pword(indentbuf::idx));
      buf->set_indent(data.indent);
    }
    return os;
  }

  struct suppress_data { bool suppress; };
  inline suppress_data suppress_format(bool suppress) { return { suppress }; }
  template<typename _CharT, typename _Traits>
  inline std::basic_ostream<_CharT, _Traits>&operator<<(std::basic_ostream<_CharT, _Traits>& os, suppress_data data) {
    if(os.pword(indentbuf::idx) == nullptr) {
      indentbuf *newbuf = new indentbuf(os, 0);
      os.pword(indentbuf::idx) = newbuf;
    }
    indentbuf *buf = static_cast<indentbuf *>(os.pword(indentbuf::idx));
    buf->set_suppress(data.suppress);
    return os;
  }

  struct push_indent_data { size_t indent; bool push; };
  inline push_indent_data push_indent(size_t indent) { return { indent, true }; }
  inline push_indent_data pop_indent() { return { 0, false }; }
  template<typename _CharT, typename _Traits>
  inline std::basic_ostream<_CharT, _Traits>& operator<<(
    std::basic_ostream<_CharT, _Traits>& os, push_indent_data data
  ) {
    if(os.pword(indentbuf::idx) == nullptr) {
      indentbuf *newbuf = new indentbuf(os, 0);
      os.pword(indentbuf::idx) = newbuf;
    }
    indentbuf *buf = static_cast<indentbuf *>(os.pword(indentbuf::idx));
    (data.push ? buf->push_indent(data.indent) : buf->pop_indent());
    return os;
  }
}
#endif // INDENTER_HPP
