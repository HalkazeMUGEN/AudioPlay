#ifndef __SYNCFPS_COMPAT_ATTRIB_H__
#define __SYNCFPS_COMPAT_ATTRIB_H__

#if defined(__clang__)

#ifndef ATTRIB_MALLOC
#define ATTRIB_MALLOC __attribute__((malloc))
#endif

#elif defined(__GNUC__) || defined(__GNUG__)

#ifndef ATTRIB_MALLOC
#define ATTRIB_MALLOC __attribute__((malloc))
#endif

#elif defined(_MSC_VER)

#ifndef ATTRIB_MALLOC
#define ATTRIB_MALLOC __declspec(restrict)
#endif

#endif

#endif  // __SYNCFPS_COMPAT_ATTRIB_H__
