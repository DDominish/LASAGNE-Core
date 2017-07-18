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
#ifndef DAF_RENDEZVOUS_T_H
#define DAF_RENDEZVOUS_T_H

#include "Monitor.h"
#include "Semaphore.h"

#include <vector>
#include <algorithm>

namespace DAF
{
  /*
   * Barriers serve as synchronous points for groups of threads that
   * must occasionally wait for each other.  Barriers may support
   * any of several methods that accomplish this synchronisation.
   */

  /** @class RendezvousCommand
  * @brief Brief \todo{Brief}
  *
  * Detailed Description \todo{details}
  */
    template <typename T>
    struct RendezvousCommand : std::unary_function< std::vector<T>, void> {
        virtual typename result_type operator () (typename argument_type &) {
            /* Do Nothing */
        }
    };

    /** @class Rendezvous
    *@brief Specialised Barrier class
    *
    * A rendezvous is a barrier that:
    * <ul>
    *   <li> Unlike a CyclicBarrier, is not restricted to use
    *     with fixed-sized groups of threads.
    *     Any number of threads can attempt to enter a rendezvous,
    *     but only the predetermined number of parties enter
    *     and later become released from the rendezvous at any give time.
    *   <li> Enables each participating thread to exchange information
    *     with others at the rendezvous point. Each entering thread
    *     presents some object on entry to the rendezvous, and
    *     returns some object on release. The object returned is
    *     the result of a RendezvousFunction that is run once per
    *     rendezvous, (it is run by the last-entering thread). By
    *     default, the function applied is a rotation, so each
    *     thread returns the object given by the next (modulo parties)
    *     entering thread. This default function faciliates simple
    *     application of a common use of rendezvous, as exchangers.
    * </ul>
    * <p>
    * Rendezvous use an all-or-none breakage model
    * for failed synchronization attempts: If threads
    * leave a rendezvous point prematurely because of timeout
    * or interruption, others will also leave abnormally
    * (via BrokenBarrierException), until
    * the rendezvous is <code>restart</code>ed. This is usually
    * the simplest and best strategy for sharing knowledge
    * about failures among cooperating threads in the most
    * common usages contexts of Rendezvous.
    * <p>
    * While any positive number (including 1) of parties can
    * be handled, the most common case is to have two parties.
    * --> Doug Lee
    */
    template < typename T, typename F = RendezvousCommand<T> >
    class Rendezvous : protected Monitor
    {
        using Monitor::wait; // Hide Wait

    public:

        typedef T                               _value_type;
        typedef typename F::argument_type       _slots_type;

        typedef typename Monitor::_mutex_type   _monitor_type;

        /**
        * Create a Barrier for the indicated number of parties,
        */
        Rendezvous(int parties);

        /** \todo{Fill this in} */
        virtual ~Rendezvous(void);

        /** \todo{Fill this in} */
        bool    broken(void) const;

        /** \todo{Fill this in} */
        size_t  parties(void) const;

        /**
        * Wait msecs to complete a rendezvous.
        * @param t the item to present at rendezvous point.
        * By default, this item is exchanged with another.
        * @param msec The maximum time to wait.
        * @return an item given by some thread, and/or processed
        * by the rendezvousFunction.
        * @exception BrokenBarrierException
        * if any other thread
        * in any previous or current barrier
        * since either creation or the last <code>restart</code>
        * operation left the barrier
        * prematurely due to interruption or time-out. (If so,
        * the <code>broken</code> status is also set.)
        * Also returns as
        * broken if the RendezvousFunction encountered a run-time exception.
        * @exception TimeoutException if this thread timed out waiting for
        * the exchange. If the timeout occured while already in the
        * exchange, <code>broken</code> status is also set.
        */
        T   rendezvous(const T & t, const ACE_Time_Value * abstime = 0);
        T   rendezvous(const T & t, const ACE_Time_Value & abstime);
        T   rendezvous(const T & t, time_t msecs);

        /**
        * Wait For the Rendezvous to reset after an exchange
        * @return indicates <code>true</code> if the rendezvous was reset
        * ie all threads exited, <code>false</code> if an error occured.
        * Upon a Timeout occuring this will set the state to broken
        * and notify all existing waiters.
        */
        bool    waitReset(const ACE_Time_Value * abstime = 0);
        bool    waitReset(const ACE_Time_Value & abstime);
        bool    waitReset(time_t msecs);


        using Monitor::waiters;
        using Monitor::interrupt;
        using Monitor::interrupted;

    private:

        Semaphore       entryGate_;
        int             parties_, synch_, count_, resets_;
        bool            broken_, triggered_;
        _slots_type     slots_;
    };

    template <typename T, typename F>
    inline
    Rendezvous<T,F>::~Rendezvous(void)
    {
        this->interrupt();
    }

    template <typename T, typename F>
    inline T
    Rendezvous<T,F>::rendezvous(const T & t, const ACE_Time_Value & abstime)
    {
        return this->rendezvous(t, &abstime);
    }

    template <typename T, typename F>
    inline T
    Rendezvous<T,F>::rendezvous(const T & t, time_t msecs)
    {
        return this->rendezvous(t, DAF_OS::gettimeofday(ace_max(msecs, time_t(0))));
    }

    template <typename T, typename F>
    inline bool
    Rendezvous<T, F>::waitReset(const ACE_Time_Value & abstime)
    {
        return this->waitReset(&abstime);
    }

    template <typename T, typename F>
    inline bool
    Rendezvous<T, F>::waitReset(time_t msecs)
    {
        return this->waitReset(DAF_OS::gettimeofday(ace_max(msecs, time_t(0))));
    }

   /** @struct RendezvousRotator
    * @brief A common rotator rendezvous function.
    *
    * Rotates the array so that each thread returns an item presented by
    * some other thread (or itself, if parties is 1).
    */
    template <typename T>
    struct RendezvousRotator : RendezvousCommand<T> {
        virtual result_type operator () (argument_type &val);
    };

} // namespace DAF

#if defined (ACE_TEMPLATES_REQUIRE_SOURCE)
# include "Rendezvous_T.cpp"
#endif /* ACE_TEMPLATES_REQUIRE_SOURCE */

#if defined (ACE_TEMPLATES_REQUIRE_PRAGMA)
# pragma implementation ("Rendezvous_T.cpp")
#endif /* ACE_TEMPLATES_REQUIRE_PRAGMA */

#endif  // DAF_RENDEZVOUS_T_H
