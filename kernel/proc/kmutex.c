#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"

/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

void
kmutex_init(kmutex_t *mtx)
{
        /*NOT_YET_IMPLEMENTED("PROCS: kmutex_init");*/
        dbg_print("\nInitializing the Mutex\n");
        sched_queue_init(&mtx->km_waitq);       /* wait queue */
        mtx->km_holder=NULL;    
        
}

/*
 * This should block the current thread (by sleeping on the mutex's
 * wait queue) if the mutex is already taken.
 *
 * No thread should ever try to lock a mutex it already has locked.
 */
void
kmutex_lock(kmutex_t *mtx)
{
       /* NOT_YET_IMPLEMENTED("PROCS: kmutex_lock");*/
		if(mtx->km_holder!=NULL)
		{
			sched_sleep_on(&mtx->km_waitq);
		}
		else
		{
				KASSERT(curthr!=mtx->km_holder && "the current thread already has the mutex");
				mtx->km_holder=curthr;
		}	
}

/*
 * This should do the same as kmutex_lock, but use a cancellable sleep
 * instead.
 ************************************************************************
 * Locks the specified mutex, but puts the current thread into a
 * cancellable sleep if the function blocks.
 *
 * Note: This function may block.
 *
 * Note: These locks are not re-entrant.
 *
 * @param mtx the mutex to lock
 * @return 0 if the current thread now holds the mutex and -EINTR if
 * the sleep was cancelled and this thread does not hold the mutex
 
 
 */
int
kmutex_lock_cancellable(kmutex_t *mtx)
{
        /*NOT_YET_IMPLEMENTED("PROCS: kmutex_lock_cancellable");
        return 0;*/
        
        if(mtx->km_holder!=NULL)
		{
			int temp=sched_cancellable_sleep_on(&mtx->km_waitq);
			
			return -EINTR;
		}
		else
		{
				KASSERT(curthr!=mtx->km_holder && "the current thread already has the mutex");
				mtx->km_holder=curthr;
				return 0;
		}	
 }

/*
 * If there are any threads waiting to take a lock on the mutex, one
 * should be woken up and given the lock.
 *
 * Note: This should _NOT_ be a blocking operation!
 *
 * Note: Don't forget to add the new owner of the mutex back to the
 * run queue.
 *
 * Note: Make sure that the thread on the head of the mutex's wait
 * queue becomes the new owner of the mutex.
 *
 * @param mtx the mutex to unlock
 */
void
kmutex_unlock(kmutex_t *mtx)
{
        /*NOT_YET_IMPLEMENTED("PROCS: kmutex_unlock");*/
        if(sched_queue_empty(&mtx->km_waitq))
        {
        	mtx->km_holder=NULL;
        }
        else
        {
        	mtx->km_holder=sched_wakeup_on(&mtx->km_waitq);
        	/*sched_switch();*/
        	
        }
}
