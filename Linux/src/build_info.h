#ifndef TUXTIMINGS_BUILD_INFO_H
#define TUXTIMINGS_BUILD_INFO_H

/* Make refreshes this before compilation. Standalone fixture compiles can use
 * the explicit archive fallback without a generated directory. */
#if defined(__has_include)
# if __has_include("../build/build_info.generated.h")
#  include "../build/build_info.generated.h"
# endif
#endif
#ifndef TUXTIMINGS_BUILD_ID
# define TUXTIMINGS_BUILD_VERSION "1.0.5+archive"
# define TUXTIMINGS_BUILD_REVISION "unknown"
# define TUXTIMINGS_BUILD_STATE "archive"
# define TUXTIMINGS_BUILD_PROFILE "unknown"
# define TUXTIMINGS_BUILD_ID "TuxTimings 1.0.5+archive (revision unknown, archive, profile unknown)"
#endif
#endif
