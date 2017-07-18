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
    template <typename T> void
    RendezvousRotator<T>::operator () (std::vector<T> &val)
    {
        if (!val.empty()) try {
            val.push_back(val.front()); val.erase(val.begin());
        } DAF_CATCH_ALL { /* Ignore Error */ }
    }

    /********************************************************************/

    template <typename T, typename F>
    Rendezvous<T,F>::Rendezvous(int parties)
        : entryGate_    (parties)
        , parties_      (parties)
        , synch_        (0)
        , count_        (0)
        , resets_       (0)
        , broken_       (false)
        , shutdown_     (false)
        , triggered_    (false)
    {
        if ( parties == 0 ) throw InitializationException("DAF::Rendezvous Initialization Error parties == 0");

        this->slots_.reserve(parties + 2);  // Reserve enough space for manipulation (+2)
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
        while (!this->interrupted()) {

            ACE_GUARD_READTION(_mutex_type, mon, *this, DAF_THROW_EXCEPTION(LockFailureException));

            if (this->entryGate_.acquire(abstime)) {
                switch (this->interrupted() ? EINTR : DAF_OS::last_error()) {
                case EINTR: continue;
                case ETIME: DAF_THROW_EXCEPTION(TimeoutException);

                default: return -1;
                }
            }

            int index = this->count_++; this->slots_.push_back(t); this->resets_ = this->count_;

            while (!(this->broken_ || this->triggered_)) try {
                if (this->count_ == this->parties_) {
                    F()(this->slots_); this->triggered_ = true; break;
                }
                else if (this->wait(abstime)) {
                    switch (this->interrupted() ? EINTR : DAF_OS::last_error()) {
                    case EINTR: DAF_THROW_EXCEPTION(InterruptedException);
                    case ETIME: DAF_THROW_EXCEPTION(TimeoutException);
                    default:    DAF_THROW_EXCEPTION(BrokenBarrierException);
                    }
                }
            }
            catch (...) {
                ACE_Errno_Guard g(errno); ACE_UNUSED_ARG(g);
                --this->count_; --this->resets_;
                this->broken_ = true;
                this->broadcast();
                throw;
            }

        }

        DAF_THROW_EXCEPTION(InterruptedException);
    }
    //    const ACE_Time_Value end_time(DAF_OS::gettimeofday(msec));

    //    bool timeout    = false;
    //    size_t index    = this->count_;

    //    T t_rtn(t);

    //    this->slots_.push_back(t);
    //    this->resets_ = ++this->count_;

    //    while (!(this->broken_ || this->triggered_)) try {
    //        if (this->shutdown_) {
    //            this->broken_ = true;
    //        } else if (this->count_ != this->parties_) {
    //            if (end_time > DAF_OS::gettimeofday()) {
    //                this->wait(end_time);
    //            } else timeout = this->broken_ = true;
    //        } else {
    //            F()(this->slots_); this->triggered_ = true;
    //        }
    //    } catch(...) { // JB: Deliberate catch(...) - DON'T replace with DAF_CATCH_ALL
    //        --this->resets_;
    //        --this->count_;
    //        this->broken_ = true;
    //        this->notifyAll();
    //        throw;
    //    }

    //    if (index >= this->slots_.size()) {
    //        this->broken_ = true;
    //    } else {
    //        t_rtn = this->slots_[index];
    //    }

    //    bool broken = this->broken_, shutdown = this->shutdown_; // Get state onto the stack

    //    this->notifyAll();

    //    if (--this->resets_ == 0) {
    //        this->slots_.clear();
    //        this->entryGate_.release(int(this->count_));
    //        this->broken_ = this->triggered_ = false; ++this->synch_;
    //        this->count_ = 0;
    //    }

    //    if (timeout) {
    //        throw TimeoutException();
    //    } else if (shutdown) {
    //        throw IllegalThreadStateException();
    //    } else if (broken) {
    //        throw BrokenBarrierException();
    //    }

    //    return t_rtn;
    //}

    template <typename T, typename F> bool
    Rendezvous<T, F>::waitReset(const ACE_Time_Value * abstime)
    {
        for (size_t synch = this->synch_; this->slots_.size();) try {

            ACE_GUARD_READTION(_mutex_type, mon, *this, DAF_THROW_EXCEPTION(LockFailureException));


            if (this->synch_ == synch) {
                if (this->triggered_ || this->broken_) {
                    this->broadcast();
                }
                else if (this->wait(abstime)) {
                    this->broken_ = true;
                    this->broadcast();
                    return false;
                }
            } else break;

        } catch (...) { // JB: Deliberate catch(...) - DON'T replace with DAF_CATCH_ALL
            this->broken_ = true; this->broadcast(); throw;
        }

        return true;
    }

} // namespace DAF

#endif  // DAF_RENDEZVOUS_T_CPP
