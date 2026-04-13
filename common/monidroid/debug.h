#ifdef DEBUG
#define MD_TRACE_LINE()   Monidroid::TaggedLog("Debug trace", "{}() at {}:{},", __FUNCTION__, __FILE_NAME__, __LINE__)
#else
#define MD_TRACE_LINE()
#endif