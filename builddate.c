#define STRINGIFY2(arg) #arg
#define STRINGIFY(arg) STRINGIFY2(arg)

extern const char *buildstring;
const char *buildstring = __TIME__ " " __DATE__
#ifdef SVNREVISION
" " STRINGIFY(SVNREVISION)
#endif
#ifdef BUILDTYPE
" " STRINGIFY(BUILDTYPE)
#endif
;
