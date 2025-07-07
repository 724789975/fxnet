#ifdef	USE_FX_API
#ifdef _WIN32
#define	FX_API			__declspec(dllimport)
#else
#define	FX_API
#endif
#else
#ifdef _WIN32
#define	FX_API			__declspec(dllexport)
#else
#define	FX_API
#endif
#endif