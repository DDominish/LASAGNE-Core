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
#ifndef DAF_RENDEZVOUS_T_CPP
#define DAF_RENDEZVOUS_T_CPP

#include "Rendezvous_T.h"

#include <ace/Min_Max.h>

namespace DAF
{
    template <typename T> typename RendezvousRotator<T>::result_type
    RendezvousRotator<T>::operator () (typename RendezvousRotator<T>::argument_type & val)
    {
        if (!val.empty()) {
            val.push_back(val.front()); val.erase(val.begin());
        }
    }

    /********************************************************************/

    template <typename T, typename F>
    Rendezvous<T,F>::Rendezvous(int parties, _function_type & function) : Monitor()
        , rendezvousSemaphore_  (ace_max(0, parties))
        , rendezvousFunction_   (function)
        , rendezvousActive_     (0)
        , parties_              (0)
        , count_                (0)
        , broken_               (false)
        , triggered_            (false)
    {
        if ((this->parties_ = this->rendezvousSemaphore_.permits()) == 0) {
            DAF_THROW_EXCEPTION(InitializationException);
        }
        this->rendezvousSlots_.reserve(this->parties_ + 2); // Reserve enough space for manipulation (+2)
    }

    template <typename T, typename F>
    Rendezvous<T, F>::~Rendezvous(void)
    {
        this->interrupt();
        for (ACE_Guard<_mutex_type> mon(*this); this->count_ > 0;) {
            Monitor::wait(); // Wait for threads to exit
        }
    }

    template <typename T, typename F> int
    Rendezvous<T, F>::interrupt(void)
    {
        return Monitor::interrupt() + this->rendezvousSemaphore_.interrupt() ? -1 : 0;
    }

    template <typename T, typename F> bool
    Rendezvous<T, F>::broken(void) const
    {
        return this->broken_;
    }

    template <typename T, typename F> int
    Rendezvous<T, F>::parties(void) const
    {
        return this->parties_;
    }

    template <typename T, typename F> T
    Rendezvous<T, F>::rendezvous(const T & t, const ACE_Time_Value * abstime)
    {
        ACE_GUARD_REACTION(_mutex_type, mon, *this, DAF_THROW_EXCEPTION(LockFailureException));

        if (this->interrupted()) {
            DAF_THROW_EXCEPTION(InterruptedException);
        }
        else if ((this->triggered_ || this->broken_) ? false : this->rendezvousSemaphore_.permits() > 0) {
            if (this->interrupted() ? DAF_OS::last_error(EINTR) : this->rendezvousSemaphore_.acquire(abstime)) {
                switch (DAF_OS::last_error()) {
                case EINTR: DAF_THROW_EXCEPTION(InterruptedException);
                case ETIME: DAF_THROW_EXCEPTION(TimeoutException);
                default:    DAF_THROW_EXCEPTION(InternalException);
                }
            }
        }
        else {
            DAF_THROW_EXCEPTION(IllegalStateException);
        }

        int index = this->count_++; this->rendezvousSlots_.push_back(t); // Put value into container at end

        ++this->rendezvousActive_; // Count our activity

        for (;;) try {

            if (this->interrupted()) {
                DAF_THROW_EXCEPTION(InterruptedException);
            }
            else if (this->broken_) {
                DAF_THROW_EXCEPTION(BrokenBarrierException);
            }
            else if (this->triggered_) {

                T t_rtn = this->rendezvousSlots_[index];

                if (--this->count_ == 0) {
                    this->rendezvousSlots_.clear(); this->broadcast();
                }

                return t_rtn;
            }
            else if (this->rendezvousSemaphore_.permits()) {
                if (Monitor::wait(abstime)) {
                    switch (this->interrupted() ? EINTR : DAF_OS::last_error()) {
                    case EINTR: continue;
                    case ETIME:

                        if (this->broken_ || this->triggered_) {
                            continue;
                        }

                        DAF_THROW_EXCEPTION(TimeoutException);

                    default:
                        this->broken_ = true;
                        this->broadcast();
                    }
                }
            }
            else if (this->triggered_ ? false : true) {
                this->rendezvousFunction_(this->rendezvousSlots_);
                this->triggered_ = true;
                this->broadcast();
            }
        }
        catch (...) {
            ACE_Errno_Guard g(errno); ACE_UNUSED_ARG(g);
            this->broken_ = true;
            if (--this->count_ == 0) {
                this->rendezvousSlots_.clear();
            }
            this->broadcast();
            throw;
        }
    }

    template <typename T, typename F> int
    Rendezvous<T, F>::wait(const ACE_Time_Value * abstime) const
    {
        ACE_GUARD_REACTION(_mutex_type, mon, *this, DAF_THROW_EXCEPTION(LockFailureException));

        while (this->rendezvousActive_ ? this->count_ > 0 : true) {

            if (this->interrupted() || Monitor::wait(abstime)) {
                switch (this->interrupted() ? DAF_OS::last_error(EINTR) : DAF_OS::last_error()) {
                case ETIME:
                    if (this->rendezvousActive_ && 0 >= this->count_) {
                        return 0;
                    }

                    // Fall through

                default: return -1; // Just return with errno
                }
            }
        }

        return 0;
    }

    /*********************************************************************************************/

    //template <typename T, typename F>
    //Rendezvous<T, F>::RendezvousSemaphore::RendezvousSemaphore(int permits) : Semaphore(permits)
    //{
    //}

    //template <typename T, typename F>
    //Rendezvous<T, F>::RendezvousSemaphore::~RendezvousSemaphore(void)
    //{
    //    this->interrupt();
    //}

    //template <typename T, typename F> int
    //Rendezvous<T, F>::RendezvousSemaphore::parties(void) const
    //{
    //    return this->parties_;
    //}

    //template <typename T, typename F> int
    //Rendezvous<T, F>::RendezvousSemaphore::acquire(const ACE_Time_Value * abstime)
    //{
    //    int index = int(this->parties_ - this->permits());

    //    if (Semaphore::acquire(abstime)) {
    //        switch (this->interrupted() ? EINTR : DAF_OS::last_error()) {
    //        case EINTR: DAF_THROW_EXCEPTION(InterruptedException);
    //        case ETIME: DAF_THROW_EXCEPTION(TimeoutException);
    //        default:    DAF_THROW_EXCEPTION(BrokenBarrierException);
    //        }
    //    }

    //    return index;
    //}

    //template <typename T, typename F> int
    //Rendezvous<T, F>::RendezvousSemaphore::release(void)
    //{
    //    Semaphore::release(); return int(this->parties_ - this->permits());
    //}


} // namespace DAF

#endif  // DAF_RENDEZVOUS_T_CPP
