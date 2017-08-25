/***************************************************************
    Copyright 2016, 2017 Defence Science and Technology Group,
    Department of Defence,
    Australian Government

    This file is part of LASAGNE.

    LASAGNE is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.

    LASAGNE is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with LASAGNE.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************/
#ifndef DAF_CONFIG_H
#define DAF_CONFIG_H

#include "Constants.h"

#include <ace/config.h>

/********** Ensure Environment Validity ***********/

#if !defined(ACE_ENABLE_SWAP_ON_WRITE) && (ACE_BYTE_ORDER == ACE_LITTLE_ENDIAN)
# error "DAF requires ACE_ENABLE_SWAP_ON_WRITE to be set for this configuration."
#endif

/********** DAF Configuration *********************/

#if !defined(ACE_WIN32) && !defined(DAF_HAS_EVENTFD)
# define DAF_HAS_EVENTFD 1
#endif

#define DAF_UNUSED_STATIC(ID) \
template <typename T> inline void ID ## _UNUSED(const T& = (ID)) {}

#if defined(ACE_HAS_THREADS)

# if defined(ACE_HAS_WTHREADS) && !defined(ACE_HAS_WINCE)

#  define DAF_HAS_WIN32_PRIORITY_CLASS

#  define DAF_PRIORITY_IDLE             long(THREAD_PRIORITY_IDLE)
#  define DAF_PRIORITY_LOWEST           long(THREAD_PRIORITY_LOWEST)
#  define DAF_PRIORITY_BELOW_NORMAL     long(THREAD_PRIORITY_BELOW_NORMAL)
#  define DAF_PRIORITY_NORMAL           long(THREAD_PRIORITY_NORMAL)
#  define DAF_PRIORITY_ABOVE_NORMAL     long(THREAD_PRIORITY_ABOVE_NORMAL)
#  define DAF_PRIORITY_HIGHEST          long(THREAD_PRIORITY_HIGHEST)
#  define DAF_PRIORITY_TIME_CRITICAL    long(THREAD_PRIORITY_TIME_CRITICAL)

# else

#  define DAF_PRIORITY_IDLE             long(-15)
#  define DAF_PRIORITY_LOWEST           long(-2)
#  define DAF_PRIORITY_BELOW_NORMAL     long(-1)
#  define DAF_PRIORITY_NORMAL           long(0)
#  define DAF_PRIORITY_ABOVE_NORMAL     long(+1)
#  define DAF_PRIORITY_HIGHEST          long(+2)
#  define DAF_PRIORITY_TIME_CRITICAL    long(+15)

# endif

// Setting the REALTIME_PRIORITY_CLASS to OS schedulers
// is almost always a VERY BAD THING. This guard will allow people
// to easily disable this priority class feature in DAF.
// NOTE: The threads of the process preempt the threads of all other processes,
//       including operating system processes performing important tasks.
//       For example, a real - time process that executes for more than a
//       very brief interval can cause disk caches not to flush or cause
//       the mouse to be unresponsive.
// # define DAF_HAS_REALTIME_PRIORITY_CLASS
// Setting the THREAD_PRIORITY_TIME_CRITICAL to OS schedulers
// is almost always a VERY BAD THING. This guard will allow people
// to easily disable this priority feature in DAF.
// # define DAF_HAS_THREAD_PRIORITY_TIME_CRITICAL

/**** Make Windows behave like Linux threads when in cancellable event (ie wait/sleep) ***/

# if defined(ACE_LACKS_COND_T)
#  define DAF_USES_COND_T_WAITERS
# endif

# if defined(ACE_WIN32)

#  if defined(ACE_USES_WINCE_SEMA_SIMULATION) || defined (ACE_HAS_WTHREADS_CONDITION_VARIABLE)
#   undef DAF_USES_COND_T_WAITERS
#  else
#   define DAF_HAS_WAIT_FOR_TERMINATE_WTHREAD   1
#   define DAF_HAS_ABI_FORCED_UNWIND_EXCEPTION  1
    namespace abi
    {
        struct __forced_unwind {};
    }
#  endif

    // Exception catching for abi (linux)
# elif defined(ACE_HAS_PTHREADS)

#  include <cxxabi.h>
#  define DAF_HAS_ABI_FORCED_UNWIND_EXCEPTION 1

# endif

#endif


#if defined(DAF_HAS_ABI_FORCED_UNWIND_EXCEPTION) && (DAF_HAS_ABI_FORCED_UNWIND_EXCEPTION > 0)
# define DAF_CATCH_ALL catch (const abi::__forced_unwind &) { throw; } catch(...) 
# define DAF_CATCH_ALL_ACTION(ACTION) catch (const abi::__forced_unwind &) { ACTION; throw; } catch(...) 
#else
# define DAF_CATCH_ALL catch (...) 
# define DAF_CATCH_ALL_ACTION(ACTION) catch (...) 
#endif

#define DAF_HANDLES_THREAD_CLEANUP  1 // If we changed this to zero change the other in OS.cpp Thread_Adapter::invoke

#if defined(__ANDROID_API__)
# define DAF_LACKS_EFD_SEMAPHORE 1 // This affects the SignalHandler when using file descriptors
#endif
/********** TAF Configuration *********************/

/* Compile Time Options - Maybe need a TAFConfig.h eventually */
#define TAF_HAS_UNIQUE_DEFAULT_POA  1

#endif // DAF_CONFIG_H
