#ifndef __MCIMANAGER_COMPAT_ATTRIB_H__
#define __MCIMANAGER_COMPAT_ATTRIB_H__

#if defined(__clang__)

#ifndef ATTRIB_MALLOC
#define ATTRIB_MALLOC __attribute__((malloc))
#endif

#ifndef ATTRIB_CONST
#define ATTRIB_CONST __attribute__((const))
#endif

#ifndef ATTRIB_PURE
#define ATTRIB_PURE __attribute__((pure))
#endif

#elif defined(__GNUC__) || defined(__GNUG__)

#ifndef ATTRIB_MALLOC
#define ATTRIB_MALLOC __attribute__((malloc))
#endif

#ifndef ATTRIB_CONST
#define ATTRIB_CONST __attribute__((const))
#endif

#ifndef ATTRIB_PURE
#define ATTRIB_PURE __attribute__((pure))
#endif

#elif defined(_MSC_VER)

#ifndef ATTRIB_MALLOC
#define ATTRIB_MALLOC __declspec(restrict)
#endif

#ifndef ATTRIB_CONST
#define ATTRIB_CONST __declspec(noalias)
#endif

#ifndef ATTRIB_PURE
#define ATTRIB_PURE __declspec(noalias)
#endif

#endif

#endif  // __MCIMANAGER_COMPAT_ATTRIB_H__
