#define STRINGIFY2(arg) #arg
#define STRINGIFY(arg) STRINGIFY2(arg)

extern const char *buildstring;
const char *buildstring =
#ifndef NO_BUILD_TIMESTAMPS
__TIME__ " " __DATE__ " "
#endif
#ifdef SVNREVISION
STRINGIFY(SVNREVISION)
#else
"-"
#endif
#ifdef BUILDTYPE
" " STRINGIFY(BUILDTYPE)
#endif
;
