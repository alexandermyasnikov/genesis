
#include <cstdarg>
#include <chrono>



#define DEBUG_LOGGER(_name, _indent, _cond)   \
  debug_logger_n::logger_t debug_logger(debug_logger_n::ctx_t{   \
      .indent=_indent,   \
      .name=_name,   \
      .file=__FILE__,   \
      .function=__FUNCTION__,   \
      .line=__LINE__,   \
      .stream=stderr,   \
      .cond=_cond,   \
      .time=0,   \
  })

#define DEBUG_LOG(_name, _indent, _cond, ...)   \
  debug_logger_n::logger_t::log(debug_logger_n::ctx_t{   \
      .indent=_indent,   \
      .name=_name,   \
      .file=__FILE__,   \
      .function=__FUNCTION__,   \
      .line=__LINE__,   \
      .stream=stderr,   \
      .cond=_cond,   \
      .time=0,   \
  }, __VA_ARGS__)



namespace debug_logger_n {

  template <typename T>
  struct indent_t {
    static inline int indent = 0;
  };

  struct ctx_t {
    int&          indent;
    const char*   name;
    const char*   file;
    const char*   function;
    int           line;
    FILE*         stream;
    int           cond;
    uint64_t      time;
  };

  struct logger_t {
    logger_t(const ctx_t& ctx) : _ctx(ctx) {
      _ctx.time = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      if (_ctx.cond) {
        fprintf(_ctx.stream, "%s %d    %*s#%d --> %s\n",
            _ctx.name, _ctx.indent / 2,
            _ctx.indent, "", _ctx.line, _ctx.function);
        fflush(_ctx.stream);
      }
      _ctx.indent += 2;
    }

    ~logger_t() {
      _ctx.indent -= 2;
      _ctx.time = std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::system_clock::now().time_since_epoch()).count() - _ctx.time;
      if (_ctx.cond) {
        fprintf(_ctx.stream, "%s %d %c  %*s# <-- %s %ldms\n",
            _ctx.name, _ctx.indent / 2, std::uncaught_exceptions() ? '*' : ' ',
            _ctx.indent, "", _ctx.function, _ctx.time);
        fflush(_ctx.stream);
      }
    }

    static void log(const ctx_t& ctx, const char* format, ...) {
      fprintf(ctx.stream, "%s %d    %*s#%d    ",
          ctx.name, ctx.indent / 2, ctx.indent, "", ctx.line);

      va_list args;
      va_start(args, format);
      vfprintf(ctx.stream, format, args);
      va_end(args);
      fprintf(ctx.stream, "\n");
      fflush(ctx.stream);
    }

    ctx_t _ctx;
  };

}

