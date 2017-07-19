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
        , parties_              (0)
        , count_                (0)
        , resets_               (0)
        , synch_                (0)
        , broken_               (false)
        , triggered_            (false)

    {
        if ((this->parties_ = this->rendezvousSemaphore_.permits()) != 0) {
            this->rendezvousSlots_.reserve(this->parties_ + 2); return; // Reserve enough space for manipulation (+2)
        }
        DAF_THROW_EXCEPTION(InitializationException);
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

        if (this->rendezvousSemaphore_.acquire(abstime)) {
            switch (this->interrupted() ? EINTR : DAF_OS::last_error()) {
            case EINTR: DAF_THROW_EXCEPTION(InterruptedException);
            case ETIME: DAF_THROW_EXCEPTION(TimeoutException);
            default:    DAF_THROW_EXCEPTION(BrokenBarrierException);
            }
        }

        int index = this->resets_ = ++this->count_; this->rendezvousSlots_[--index] = t; // Put value into container at our index

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
                    this->broken_ = this->triggered_ = false;
                    this->rendezvousSemaphore_.release(this->resets_); this->resets_ = 0;
                    ++this->synch_; this->broadcast();
                }

                return t_rtn;
            }
            else if (this->rendezvousSemaphore_.permits()) {
                if (this->wait(abstime)) {
                    switch (this->interrupted() ? EINTR : DAF_OS::last_error()) {
                    case EINTR: continue;
                    case ETIME: DAF_THROW_EXCEPTION(TimeoutException);
                    default:    this->broken_ = true; this->broadcast();
                    }
                }
            }
            else {
                this->rendezvousFunction_(this->rendezvousSlots_);
                this->triggered_ = true;
                this->broadcast();
            }
        }
        catch (...) {
            ACE_Errno_Guard g(errno); ACE_UNUSED_ARG(g);
            this->broken_ = true;
            if (--this->count_ == 0) {
                this->broken_ = this->triggered_ = false;
                this->rendezvousSemaphore_.release(this->resets_); this->resets_ = 0;
                ++this->synch_;
            }
            this->broadcast(); throw;
        }
    }

    template <typename T, typename F> bool
    Rendezvous<T, F>::waitReset(const ACE_Time_Value * abstime)
    {
        ACE_GUARD_REACTION(_mutex_type, mon, *this, DAF_THROW_EXCEPTION(LockFailureException));

        for (int synch = this->synch_;;) try {

            if (this->interrupted()) {
                DAF_THROW_EXCEPTION(InterruptedException);
            }
            else if (this->resets_ && this->synch_ == synch) {
                if (this->triggered_ || this->broken_) {
                    this->broadcast();
                }
                if (this->wait(abstime)) {
                    int last_error = DAF_OS::last_error();
                    {
                        ACE_Errno_Guard g(errno); ACE_UNUSED_ARG(g);

                        switch (this->interrupted() ? EINTR : last_error) {
                        case EINTR: DAF_THROW_EXCEPTION(InterruptedException);
                        }

                        this->broken_ = true; this->broadcast();
                    }
                    return false;
                }
            } else break;

        } catch (...) { // JB: Deliberate catch(...) - DON'T replace with DAF_CATCH_ALL
            ACE_Errno_Guard g(errno); ACE_UNUSED_ARG(g);
            this->broken_ = true;
            this->broadcast();
            throw;
        }

        return true;
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
