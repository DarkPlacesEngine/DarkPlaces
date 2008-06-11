#define STRINGIFY2(arg) #arg
#define STRINGIFY(arg) STRINGIFY2(arg)

char *buildstring = __TIME__ " " __DATE__
#ifdef SVNREVISION
" " STRINGIFY(SVNREVISION)
#endif
#ifdef BUILDTYPE
" " STRINGIFY(BUILDTYPE)
#endif
;
