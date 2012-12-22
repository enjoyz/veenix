#include "globals.h"
#include "errno.h"

#include "main/interrupt.h"

#include "proc/sched.h"
#include "proc/kthread.h"

#include "util/init.h"
#include "util/debug.h"

static ktqueue_t kt_runq;

static __attribute__((unused)) void
sched_init(void)
{
        sched_queue_init(&kt_runq);
}
init_func(sched_init);



/*** PRIVATE KTQUEUE MANIPULATION FUNCTIONS ***/
/**
 * Enqueues a thread onto a queue.
 *
 * @param q the queue to enqueue the thread onto
 * @param thr the thread to enqueue onto the queue
 */
static void
ktqueue_enqueue(ktqueue_t *q, kthread_t *thr)
{
        KASSERT(!thr->kt_wchan);
        list_insert_head(&q->tq_list, &thr->kt_qlink);
        thr->kt_wchan = q;
        q->tq_size++;
}

/**
 * Dequeues a thread from the queue.
 *
 * @param q the queue to dequeue a thread from
 * @return the thread dequeued from the queue
 */
static kthread_t *
ktqueue_dequeue(ktqueue_t *q)
{
        kthread_t *thr;
        list_link_t *link;

        if (list_empty(&q->tq_list))
                return NULL;

        link = q->tq_list.l_prev;
        thr = list_item(link, kthread_t, kt_qlink);
        list_remove(link);
        thr->kt_wchan = NULL;

        q->tq_size--;

        return thr;
}

/**
 * Removes a given thread from a queue.
 *
 * @param q the queue to remove the thread from
 * @param thr the thread to remove from the queue
 */
static void
ktqueue_remove(ktqueue_t *q, kthread_t *thr)
{
        KASSERT(thr->kt_qlink.l_next && thr->kt_qlink.l_prev);
        list_remove(&thr->kt_qlink);
        thr->kt_wchan = NULL;
        q->tq_size--;
}

/*** PUBLIC KTQUEUE MANIPULATION FUNCTIONS ***/
void
sched_queue_init(ktqueue_t *q)
{
        list_init(&q->tq_list);
        q->tq_size = 0;
}

int
sched_queue_empty(ktqueue_t *q)
{
        return list_empty(&q->tq_list);
}

/*
 * Updates the thread's state and enqueues it on the given
 * queue. Returns when the thread has been woken up with wakeup_on or
 * broadcast_on.
 *
 * Use the private queue manipulation functions above.
 ***********************************************************************
 * Causes the current thread to enter into an uncancellable sleep on
 * the given queue.
 *
 * @param q the queue to sleep on
 */
 
 
void
sched_sleep_on(ktqueue_t *q)
{
       /* NOT_YET_IMPLEMENTED("PROCS: sched_sleep_on");	*/
       KASSERT(curthr->kt_state!=KT_SLEEP && "\nThe thread is been marked sleep alreadt\n");
       curthr->kt_state=KT_SLEEP;
       ktqueue_enqueue(q, curthr);
       sched_switch(); 
}

/*
 * Similar to sleep on, but the sleep can be cancelled.
 *
 * Don't forget to check the kt_cancelled flag at the correct times.
 *
 * Use the private queue manipulation functions above.
 **********************************************************************
 * Causes the current thread to enter into a cancellable sleep on the
 * given queue.
 *
 * @param q the queue to sleep on
 * @return -EINTR if the thread was cancelled and 0 otherwise
 
 */
int
sched_cancellable_sleep_on(ktqueue_t *q)
{
       /* NOT_YET_IMPLEMENTED("PROCS: sched_cancellable_sleep_on");
        return 0; */
       KASSERT(curthr->kt_state!=KT_SLEEP_CANCELLABLE && "\nThe thread is been marked sleep alreadt\n");
       	dbgq(DBG_SCHED,"\nMaking the current thread %s, sleep\n",curthr->kt_proc->p_comm);
        curthr->kt_state=KT_SLEEP_CANCELLABLE;
        ktqueue_enqueue(q, curthr);
        sched_switch();
    	if(curthr->kt_cancelled==1)
		{
			return -EINTR;
		}
		else       	
       		return 0; 
}
/*
 * Wakes a single thread from sleep if there are any waiting on the
 * queue.
 *
 * @param q the q to wakeup a thread from
 * @return NULL if q is empty and a thread waiting on the q otherwise
*/

kthread_t *
sched_wakeup_on(ktqueue_t *q)
{
       /* NOT_YET_IMPLEMENTED("PROCS: sched_wakeup_on");
        return NULL; */
        if(sched_queue_empty(q))
        return NULL;
       kthread_t *temp_wakeup_thr=ktqueue_dequeue(q);
      dbgq(DBG_SCHED,"\nWaking up process %s, sleep\n",temp_wakeup_thr->kt_proc->p_comm);
       sched_make_runnable(temp_wakeup_thr);
        return temp_wakeup_thr;
}

/*
 * Wake up all threads running on the queue.
 * @param q the queue to wake up threads from
*/

void
sched_broadcast_on(ktqueue_t *q)
{
        /*NOT_YET_IMPLEMENTED("PROCS: sched_broadcast_on"); */
        dbgq(DBG_SCHED,"\nWaking up all the threads..\n");
        while(!sched_queue_empty(q))
        {
        	sched_make_runnable(ktqueue_dequeue(q));
        }
}

/*
 * If the thread's sleep is cancellable, we set the kt_cancelled
 * flag and remove it from the queue. Otherwise, we just set the
 * kt_cancelled flag and leave the thread on the queue.
 *
 * Remember, unless the thread is in the KT_NO_STATE or KT_EXITED
 * state, it should be on some queue. Otherwise, it will never be run
 * again.
 *********************************************************************
 * Cancel the given thread from the queue it sleeps on.
 *
 * @param the thread to cancel sleep from
 */
void
sched_cancel(struct kthread *kthr)
{
       /* NOT_YET_IMPLEMENTED("PROCS: sched_cancel");*/
       KASSERT(kthr->kt_state!=KT_SLEEP && "\nThe thread is already asleep\n");
         if(kthr->kt_state==KT_SLEEP_CANCELLABLE)
	   	 {
	   	 		dbgq(DBG_SCHED,"\nCancelling the thread %s..\n",kthr->kt_proc->p_comm);
	   			kthr->kt_cancelled=1;
				ktqueue_remove(kthr->kt_wchan, kthr);
				sched_make_runnable(kthr);
		 }
		 else
		 {
		 		dbgq(DBG_SCHED,"\nCancelling the thread %s..\n",kthr->kt_proc->p_comm);
		 	kthr->kt_cancelled=1;
		 }
}

/*
 * In this function, you will be modifying the run queue, which can
 * also be modified from an interrupt context. In order for thread
 * contexts and interrupt contexts to play nicely, 
 
 1)you need to mask
 * all interrupts before reading or modifying the run queue and
 * re-enable interrupts when you are done. This is analagous to
 * locking a mutex before modifying a data structure shared between
 * threads. Masking interrupts is accomplished by setting the IPL to
 * high.
 *
 * Once you have masked interrupts, 
 
 2) you need to remove a thread from
 * the run queue and switch into its context from the currently
 * executing context.
 *
  3) If there are no threads on the run queue (assuming you do not have
 * any bugs), then all kernel threads are waiting for an interrupt
 * (for example, when reading from a block device, a kernel thread
 * will wait while the block device seeks). You will need to re-enable
 * interrupts and wait for one to occur in the hopes that a thread
 * gets put on the run queue from the interrupt context.
 *
 * The proper way to do this is with the intr_wait call. See
 * interrupt.h for more details on intr_wait.
 *
 * Note: When waiting for an interrupt, don't forget to modify the
 * IPL. If the IPL of the currently executing thread masks the
 * interrupt you are waiting for, the interrupt will never happen, and
 * your run queue will remain empty. This is very subtle, but
 * _EXTREMELY_ important.
 *
 * Note: Don't forget to set curproc and curthr. When sched_switch
 * returns, a different thread should be executing than the thread
 * which was executing when sched_switch was called.
 *
 * Note: The IPL is process specific.
 */
void
sched_switch(void)
{
        /*NOT_YET_IMPLEMENTED("PROCS: sched_switch");
		*/
		
		uint8_t temp_ipl=intr_getipl();	
		intr_setipl(IPL_HIGH); 
		
		while(sched_queue_empty(&kt_runq))
		{
			intr_setipl(IPL_LOW);
			intr_wait();
			intr_setipl(IPL_HIGH);
		}
		
		kthread_t *old_thread=curthr;
		/****************** MY_KASSERT ****************/
		KASSERT(old_thread!=NULL && "The current thread is invalid.");
		/**********************************************/
		proc_t *old_proc=curproc;
		KASSERT(!sched_queue_empty(&kt_runq) && "Nothing is there in the runq");
		curthr=ktqueue_dequeue(&kt_runq);
		/* shd we enqueue back? */
		
		curproc=curthr->kt_proc;
		/*curproc->p_state=PROC_RUNNING;*/

		intr_setipl(temp_ipl);
		context_switch(&old_thread->kt_ctx, &curthr->kt_ctx);
		
}

/*
 * Since we are modifying the run queue, we _MUST_ set the IPL to high
 * so that no interrupts happen at an inopportune moment.

 * Remember to restore the original IPL before you return from this
 * function. Otherwise, we will not get any interrupts after returning
 * from this function.
 *
 * Using intr_disable/intr_enable would be equally as effective as
 * modifying the IPL in this case. However, in some cases, we may want
 * more fine grained control, making modifying the IPL more
 * suitable. We modify the IPL here for consistency.
 */
void
sched_make_runnable(kthread_t *thr)
{
    /*   NOT_YET_IMPLEMENTED("PROCS: sched_make_runnable"); 
		*/
		uint8_t temp_ipl=intr_getipl();	
		intr_setipl(IPL_HIGH);		
		thr->kt_state=KT_RUN;
		ktqueue_enqueue(&kt_runq,thr);
		intr_setipl(temp_ipl);
			
}
