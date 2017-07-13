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
#ifndef DAF_SYNCHVALUE_T_CPP
#define DAF_SYNCHVALUE_T_CPP

#include "SynchValue_T.h"

#include <ace/Min_Max.h>

namespace DAF
{
    template <typename T> inline
    SynchValue_T<T>::SynchValue_T(const T &value) : Monitor()
        , value_(value), valueSemaphore_(0)
    {
    }

    template <typename T>
    SynchValue_T<T>::~SynchValue_T(void)
    {
        this->interrupt();
    }

    template <typename T> T
    SynchValue_T<T>::getValue(void) const
    {
        ACE_GUARD_RETURN(_mutex_type, val_guard, this->valueLock_, this->value_); return this->value_;
    }

    template <typename T> int
    SynchValue_T<T>::setValue(const T & value)
    {
        ACE_GUARD_REACTION(_mutex_type, val_guard, this->valueLock_, DAF_THROW_EXCEPTION(LockFailureException));

        if (this->interrupted()) {
            DAF_THROW_EXCEPTION(InterruptedException);
        }

        int result = 0, waiter_count = 0;

        {
            ACE_GUARD_REACTION(_mutex_type, guard, *this, DAF_THROW_EXCEPTION(LockFailureException));

            this->value_ = value;

            if ((waiter_count = this->waiters()) > 0) {
                this->broadcast();
            }
        }

        while (waiter_count-- > 0) {
            if (this->valueSemaphore_.acquire()) {
                result = -1;
            }
        }

        return result;
    }

    template <typename T> int
    SynchValue_T<T>::waitValue(const T & value, const ACE_Time_Value * abstime) const
    {
        ACE_GUARD_REACTION(_mutex_type, guard, *this, DAF_THROW_EXCEPTION(LockFailureException));

        for (;;) {

            if (this->interrupted()) {
                DAF_THROW_EXCEPTION(InterruptedException);
            }
            else if (this->value_ == value) {
                return 0;
            }
            else {  // Register Atomic-Signallable Semaphore Waiter and wait for next value

                ValueSemaphoreWaiterGuard waiterGuard(this->valueSemaphore_); ACE_UNUSED_ARG(waiterGuard);

                if (this->wait(abstime)) {
                    int last_error = DAF_OS::last_error();
                    switch (this->interrupted() ? DAF_OS::last_error(EINTR) : last_error) {
                    case 0:     continue; // Maybe woken up without error??
                    case EINTR: DAF_THROW_EXCEPTION(InterruptedException);
                    case ETIME:
                        if (this->value_ == value) {
                            return 0; // All Good
                        }

                        DAF_THROW_EXCEPTION(TimeoutException);
                    }

                    DAF_OS::last_error(last_error); return -1;
                }
            }
        }

        return -1; // Keep the compiler happy - Should never get here
    }

    /** Wait on a Latched Value until abs time value tv */
    template <typename T> inline int
    SynchValue_T<T>::waitValue(const T & value, const ACE_Time_Value & abstime) const
    {
        return this->waitValue(value, &abstime);
    }

    /** Wait on a Latched Value for upto msec */
    template <typename T> inline int
    SynchValue_T<T>::waitValue(const T & value, time_t msecs) const
    {
        return this->waitValue(value, DAF_OS::gettimeofday(ace_max(msecs, time_t(0))));
    }

    /***********************************************************************************/

    template <typename T> inline
    SynchValue_T<T>::ValueSemaphore::ValueSemaphore(int permits) : Semaphore(permits)
        , valueWaiters_(0)
    {}

    template <typename T> inline
    SynchValue_T<T>::ValueSemaphore::~ValueSemaphore(void)
    {
        this->interrupt();
    }

    template <typename T> inline int
    SynchValue_T<T>::ValueSemaphore::acquireWaiter(void)
    {
        ACE_Errno_Guard g(errno); ACE_UNUSED_ARG(g); // Preserve errno
        return ++this->valueWaiters_;
    }

    template <typename T> inline int
    SynchValue_T<T>::ValueSemaphore::releaseWaiter(void)
    {
        ACE_Errno_Guard g(errno); ACE_UNUSED_ARG(g); // Preserve errno
        int result = this->release(); --this->valueWaiters_; return result;
    }

    /***********************************************************************************/

    template <typename T> inline
    SynchValue_T<T>::ValueSemaphore::ValueWaiters::ValueWaiters(int valueWaiters) : Monitor()
        , valueWaiters_(valueWaiters)
    {
    }

    template <typename T>
    SynchValue_T<T>::ValueSemaphore::ValueWaiters::~ValueWaiters(void)
    {
        ACE_GUARD(_mutex_type, mon, *this);
        for (const ACE_Time_Value abstime(DAF_OS::gettimeofday(DAF_MSECS_ONE_SECOND)); this->valueWaiters_ > 0;) { // Wait 1 Second
            if (this->wait(abstime) && DAF_OS::last_error() == ETIME) {
                break;
            }
        }
    }

    template <typename T> int
    SynchValue_T<T>::ValueSemaphore::ValueWaiters::operator ++ () // Prefix
    {
        ACE_GUARD_RETURN(_mutex_type, mon, *this, ++this->valueWaiters_); // Just increment value on lock failure
        int value = ++this->valueWaiters_; this->signal(); return value;
    }

    template <typename T> int
    SynchValue_T<T>::ValueSemaphore::ValueWaiters::operator -- () // Prefix
    {
        ACE_GUARD_RETURN(_mutex_type, mon, *this, --this->valueWaiters_); // Just decrement value on lock failure
        int value = --this->valueWaiters_; this->signal(); return value;
    }

} // namespace DAF

#endif // DAF_SYNCHVALUE_T_CPP
