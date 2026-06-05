// DJGPP's math.h only declares double round(); <cmath> fails to populate
// std::round because roundf is absent from the C lib. DOS build only.
#ifdef __DJGPP__
#include <math.h>
namespace std {
    using ::round;
    inline float round(float x) { return (float)::round(x); }
}
#endif
