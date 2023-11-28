#define PP_CONCAT(A, B) PP_CONCAT_IMPL(A, B)
#define PP_CONCAT_IMPL(A, B) A##B
#define PP_REMOVE_PARENS(T) PP_REMOVE_PARENS_IMPL T
#define PP_REMOVE_PARENS_IMPL(...) __VA_ARGS__

#define PP_GET_N(N, ...) PP_CONCAT(PP_GET_N_, N)(__VA_ARGS__)
#define PP_GET_N_0(_0, ...) _0
#define PP_GET_N_1(_0, _1, ...) _1
#define PP_GET_N_2(_0, _1, _2, ...) _2

#define PP_GET_TUPLE(N, T) PP_GET_N(N, PP_REMOVE_PARENS(T))

#define RSEQ_SER_N()                                                           \
  119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105,   \
      104, 103, 102, 101, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, \
      87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70,  \
      69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52,  \
      51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34,  \
      33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,  \
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

#define ARG_SER_N(                                                             \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16,     \
    _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, \
    _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, \
    _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, \
    _62, _63, _64, _65, _66, _67, _68, _69, _70, _71, _72, _73, _74, _75, _76, \
    _77, _78, _79, _80, _81, _82, _83, _84, _85, _86, _87, _88, _89, _90, _91, \
    _92, _93, _94, _95, _96, _97, _98, _99, _100, _101, _102, _103, _104,      \
    _105, _106, _107, _108, _109, _110, _111, _112, _113, _114, _115, _116,    \
    _117, _118, _119, N, ...)                                                  \
  N

#define PP_EXPAND(...) __VA_ARGS__

#define PP_GET_ARG_COUNT_INNER(...) PP_EXPAND(ARG_SER_N(__VA_ARGS__))
#define PP_GET_ARG_COUNT(...) PP_GET_ARG_COUNT_INNER(__VA_ARGS__, RSEQ_SER_N())

#define LOG_SEPERATOR ,

#define LOG_ARG_1(element, ...) PP_GET_TUPLE(1, element)
#define LOG_ARG_2(element, ...) \
  PP_GET_TUPLE(1, element)      \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_1(__VA_ARGS__))
#define LOG_ARG_3(element, ...) \
  PP_GET_TUPLE(1, element)      \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_2(__VA_ARGS__))
#define LOG_ARG_4(element, ...) \
  PP_GET_TUPLE(1, element)      \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_3(__VA_ARGS__))
#define LOG_ARG_5(element, ...) \
  PP_GET_TUPLE(1, element)      \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_4(__VA_ARGS__))
#define LOG_ARG_6(element, ...) \
  PP_GET_TUPLE(1, element)      \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_5(__VA_ARGS__))
#define LOG_ARG_7(element, ...) \
  PP_GET_TUPLE(1, element)      \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_6(__VA_ARGS__))
#define LOG_ARG_8(element, ...) \
  PP_GET_TUPLE(1, element)      \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_7(__VA_ARGS__))
#define LOG_ARG_9(element, ...) \
  PP_GET_TUPLE(1, element)      \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_8(__VA_ARGS__))
#define LOG_ARG_10(element, ...) \
  PP_GET_TUPLE(1, element)       \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_9(__VA_ARGS__))
#define LOG_ARG_11(element, ...) \
  PP_GET_TUPLE(1, element)       \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_10(__VA_ARGS__))
#define LOG_ARG_12(element, ...) \
  PP_GET_TUPLE(1, element)       \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_11(__VA_ARGS__))
#define LOG_ARG_13(element, ...) \
  PP_GET_TUPLE(1, element)       \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_12(__VA_ARGS__))
#define LOG_ARG_14(element, ...) \
  PP_GET_TUPLE(1, element)       \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_13(__VA_ARGS__))
#define LOG_ARG_15(element, ...) \
  PP_GET_TUPLE(1, element)       \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_14(__VA_ARGS__))
#define LOG_ARG_16(element, ...) \
  PP_GET_TUPLE(1, element)       \
  LOG_SEPERATOR PP_EXPAND(LOG_ARG_15(__VA_ARGS__))

#define ADD_VIEW(str) \
  std::string_view(PP_GET_TUPLE(0, str), sizeof(PP_GET_TUPLE(0, str)) - 1)

#define LOG_FMT_ARR_1(element, ...) ADD_VIEW(element)
#define LOG_FMT_ARR_2(element, ...) \
  ADD_VIEW(element)                 \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_1(__VA_ARGS__))
#define LOG_FMT_ARR_3(element, ...) \
  ADD_VIEW(element)                 \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_2(__VA_ARGS__))
#define LOG_FMT_ARR_4(element, ...) \
  ADD_VIEW(element)                 \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_3(__VA_ARGS__))
#define LOG_FMT_ARR_5(element, ...) \
  ADD_VIEW(element)                 \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_4(__VA_ARGS__))
#define LOG_FMT_ARR_6(element, ...) \
  ADD_VIEW(element)                 \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_5(__VA_ARGS__))
#define LOG_FMT_ARR_7(element, ...) \
  ADD_VIEW(element)                 \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_6(__VA_ARGS__))
#define LOG_FMT_ARR_8(element, ...) \
  ADD_VIEW(element)                 \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_7(__VA_ARGS__))
#define LOG_FMT_ARR_9(element, ...) \
  ADD_VIEW(element)                 \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_8(__VA_ARGS__))
#define LOG_FMT_ARR_10(element, ...) \
  ADD_VIEW(element)                  \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_9(__VA_ARGS__))
#define LOG_FMT_ARR_11(element, ...) \
  ADD_VIEW(element)                  \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_10(__VA_ARGS__))
#define LOG_FMT_ARR_12(element, ...) \
  ADD_VIEW(element)                  \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_11(__VA_ARGS__))
#define LOG_FMT_ARR_13(element, ...) \
  ADD_VIEW(element)                  \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_12(__VA_ARGS__))
#define LOG_FMT_ARR_14(element, ...) \
  ADD_VIEW(element)                  \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_13(__VA_ARGS__))
#define LOG_FMT_ARR_15(element, ...) \
  ADD_VIEW(element)                  \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_14(__VA_ARGS__))
#define LOG_FMT_ARR_16(element, ...) \
  ADD_VIEW(element)                  \
  LOG_SEPERATOR PP_EXPAND(LOG_FMT_ARR_15(__VA_ARGS__))

#define MAKE_FMT_ARRAY(N, ...)     \
  std::array<std::string_view, N>{ \
      PP_EXPAND(PP_CONCAT(LOG_FMT_ARR_, N)(__VA_ARGS__))};
#define MAKE_LOG_ARGS(N, ...) PP_EXPAND(PP_CONCAT(LOG_ARG_, N)(__VA_ARGS__))
