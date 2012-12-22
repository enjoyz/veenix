#include "kernel.h"
#include "config.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/proc.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/mm.h"
#include "mm/mman.h"

#include "vm/vmmap.h"

#include "fs/vfs.h"
#include "fs/vfs_syscall.h"
#include "fs/vnode.h"
#include "fs/file.h"

proc_t *curproc = NULL; /* global */
static slab_allocator_t *proc_allocator = NULL;

static list_t _proc_list;
static proc_t *proc_initproc = NULL; /* Pointer to the init process (PID 1) */

void
proc_init()
{
        list_init(&_proc_list);
        proc_allocator = slab_allocator_create("proc", sizeof(proc_t));
        KASSERT(proc_allocator != NULL);
}

static pid_t next_pid = 0;

/**
 * Returns the next available PID.
 *
 * Note: Where n is the number of running processes, this algorithm is
 * worst case O(n^2). As long as PIDs never wrap around it is O(n).
 *
 * @return the next available PID
 */
static int
_proc_getid()
{
        proc_t *p;
        pid_t pid = next_pid;
        while (1) {
failed:
                list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                        if (p->p_pid == pid) {
                                if ((pid = (pid + 1) % PROC_MAX_COUNT) == next_pid) {
                                        return -1;
                                } else {
                                        goto failed;
                                }
                        }
                } list_iterate_end();
                next_pid = (pid + 1) % PROC_MAX_COUNT;
                return pid;
        }
}

/*
 * The new process, although it isn't really running since it has no
 * threads, should be in the PROC_RUNNING state.
 *
 * Don't forget to set proc_initproc when you create the init
 * process. You will need to be able to reference the init process
 * when reparenting processes to the init process.
 */
proc_t *
proc_create(char *name)
{
		proc_t *temp_proc=slab_obj_alloc(proc_allocator);
		temp_proc->p_pid=_proc_getid();
		strcpy(temp_proc->p_comm, name);
		list_init(&temp_proc->p_threads);
		list_init(&temp_proc->p_children);
		list_link_init(&temp_proc->p_list_link);
		list_link_init(&temp_proc->p_child_link);
		temp_proc->p_pagedir=pt_create_pagedir();
		temp_proc->p_state = PROC_RUNNING;
		sched_queue_init(&temp_proc->p_wait);
		temp_proc->p_pproc=NULL;
	
			list_insert_tail(&_proc_list, &temp_proc->p_list_link);
			if(temp_proc->p_pid>0)
			{	
				list_insert_tail(&curproc->p_children, &temp_proc->p_child_link);
				temp_proc->p_pproc=curproc;
			}
		
		if(temp_proc->p_pid==1)
		proc_initproc=temp_proc;
		return temp_proc;
		
		/* NOT_YET_IMPLEMENTED("PROCS: proc_create");*/  
}

/**
 * Cleans up as much as the process as can be done from within the
 * process. This involves:
 *    - Closing all open files (VFS)
 *    - Cleaning up VM mappings (VM)
 *  
 
 *	  - Waking up its parent if it is waiting
 *    - Reparenting any children to the init process
 *    - Setting its status and state appropriately
 *
 * The parent will finish destroying the process within do_waitpid (make
 * sure you understand why it cannot be done here). Until the parent
 * finishes destroying it, the process is informally called a 'zombie'
 * process.
 *
 * This is also where any children of the current process should be
 * reparented to the init process (unless, of course, the current
 * process is the init process. However, the init process should not
 * have any children at the time it exits).
 *
 * Note: You do _NOT_ have to special case the idle process. It should
 * never exit this way.
 *
 * @param status the status to exit the process with
 *****************************************************
 
 
 */
void
proc_cleanup(int status)
{
	
    /*    NOT_YET_IMPLEMENTED("PROCS: proc_cleanup");*/
    proc_t *child;
    curproc->p_state=PROC_DEAD;
    curproc->p_status=status;
    list_remove(&curthr->kt_plink);
      
   	/* waking up MY parent */
   	if(!sched_queue_empty(&curproc->p_pproc->p_wait))
    {
    	dbgq(DBG_PROC,"\nWaking up the process:%s\n",curproc->p_comm);
    	(void)sched_wakeup_on(&curproc->p_pproc->p_wait);
    }
    if(curproc->p_pid>1)
    {
    	if(curproc->p_state==PROC_DEAD)
    	{
			KASSERT(curproc->p_state=PROC_DEAD);
			
			list_iterate_begin(&curproc->p_children, child,proc_t, p_child_link) 
				{
						dbgq(DBG_PROC,"\nReparenting all the children of a dead process to Init...\n");	
						child->p_pproc=proc_initproc;
						list_insert_tail(&proc_initproc->p_children, &child->p_child_link);
							
				}list_iterate_end();
		}
    }
   	sched_switch();   
}

/*
 * This has nothing to do with signals and kill(1).
 *
 * Calling this on the current process is equivalent to calling
 * do_exit().
 *
 * In Weenix, this is only called from proc_kill_all.
 *******************************************************
  * Stops another process from running again by cancelling all its
 * threads.
 *
 * @param p the process to kill
 * @param status the status the process should exit with
 */
void
proc_kill(proc_t *p, int status)
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_kill");*/
        if(p==curproc)
        {
        	curproc->p_state=PROC_DEAD;
			dbgq(DBG_PROC,"\nKillng the current process:%s\n",curproc->p_comm);
        	do_exit(status);
        }
        else
        {
        	dbgq(DBG_PROC,"\nKillng the process:%s\n",p->p_comm);
        	KASSERT(p->p_state!=PROC_DEAD && "\nThe process is already dead");
		    
		    kthread_t * it_thread;	
		    list_iterate_begin(&p->p_threads, it_thread, kthread_t, kt_plink) 
			{
				kthread_cancel(it_thread, &status);	
			}list_iterate_end();
			
			 list_remove(&p->p_child_link);
		    list_remove(&p->p_list_link);
		    p->p_pproc=NULL;         /* our parent process */
			p->p_status=status;        /* exit status */
		    p->p_state=PROC_DEAD;         /* running/sleeping/etc. */ 
		
		}      
		
		KASSERT(p->p_state==PROC_DEAD && "the process is not dead");
}

/*
 * Remember, proc_kill on the current process will _NOT_ return.
 * Don't kill direct children of the idle process.
 *
 * In Weenix, this is only called by sys_halt.
 ************************************************
  * Kill every process except for the idle process.
 */
void
proc_kill_all()
{
       	proc_t *child;
       	list_iterate_begin(&_proc_list, child,proc_t, p_list_link) 
		{
			proc_kill(child, 0);
							
		}list_iterate_end();
       	
}

proc_t *
proc_lookup(int pid)
{
        proc_t *p;
        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                if (p->p_pid == pid) {
                        return p;
                }
        } list_iterate_end();
        return NULL;
}

list_t *
proc_list()
{
        return &_proc_list;
}

/*
 * This function is only called from kthread_exit.
 *
 * Unless you are implementing MTP, this just means that the process
 * needs to be cleaned up and a new thread needs to be scheduled to
 * run. 
 	
 	MTP- If you are implementing MTP, a single thread exiting does not
 * necessarily mean that the process should be exited.
 *********************************************************************
 * Alerts the process that the currently executing thread has just
 * exited.
 *
 * @param retval the return value for the current thread
 */
void
proc_thread_exited(void *retval)
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_thread_exited");*/
        
       	/*curthr->kt_retval=retval;
        curthr->kt_wchan=NULL;
        curthr->kt_proc->p_state=PROC_DEAD;*/
        /*list_remove(&curthr->kt_plink);
        */
        
        proc_cleanup(0);
       
}

/* If pid is -1 dispose of one of the exited children of the current
 * process and return its exit status in the status argument, or if
 * all children of this process are still running, then this function
 * blocks on its own p_wait queue until one exits.
 *
 * If pid is greater than 0 and the given pid is a child of the
 * current process then wait for the given pid to exit and dispose
 * of it.
 *
 * If the current process has no children, or the given pid is not
 * a child of the current process return -ECHILD.
 *
 * Pids other than -1 and positive numbers are not supported.
 * Options other than 0 are` not supported.
 *********************************************************************************
  * This function implements the waitpid(2) system call.
 *
 * @param pid see waitpid man page, only -1 or positive numbers are supported
 * @param options see waitpid man page, only 0 is supported
 * @param status used to return the exit status of the child
 *
 * @return the pid of the child process which was cleaned up, or
 * -ECHILD if there are no children of this process
 */
pid_t
do_waitpid(pid_t pid, int options, int *status)
{
       /* NOT_YET_IMPLEMENTED("PROCS: do_waitpid");
        return 0;*/	
        if(pid==1)
        {
        	KASSERT(curproc->p_pid!=1 && "\n\nThe current process is 1 and is the child of the idle\n\n");
        }
        
        proc_t *child;
		int flag_test=0;
		if(pid>0 || pid==-1)
			flag_test=1;
		KASSERT(options==0 && "The option value is not zero!!");
		KASSERT(flag_test==1 && "Invalid PID running!!");
		
	 	if(pid==-1)
	    {
	    
	    	while(1)
            {
            	list_iterate_begin(&curproc->p_children, child, proc_t, p_child_link) 
		 		{
		 		
  				
		 			if(child->p_state==PROC_DEAD)
				 	{				 	
					 	pid_t temp_pid=child->p_pid;
					 	/*child->p_status = *status;*/
					 	dbg_print("\nFree the PCB and TCB\n");
					 	kthread_t *child_thr;
					 	 list_iterate_begin(&child->p_threads, child_thr, kthread_t, kt_plink) 
						 {
								kthread_destroy(child_thr);
													 	
						 } list_iterate_end();
						 slab_obj_free(proc_allocator, child);
					 	 return temp_pid;
				 	}
				} list_iterate_end();
				sched_sleep_on(&curproc->p_wait);
            }
        }
		if(pid>0)
		{
			
			while(1){
			list_iterate_begin(&curproc->p_children, child, proc_t, p_child_link) 
			{
				if(pid==child->p_pid)
				{
					KASSERT(child->p_pid>0 && "\nWrong process entering\n");
					if(child->p_state!=PROC_DEAD)
						sched_sleep_on(&curproc->p_wait);
					return pid;
				}
			}list_iterate_end();
			/*return -ECHILD;*/
			}
			return -ECHILD;
		}		
		if(list_empty(&curproc->p_children))
 		return -ECHILD;
 		return -ECHILD;
 				
}

/*
 * Cancel all threads, join with them, and exit from the current
 * thread.
 *
 * @param status the exit status of the process
 ********************************************************
  * This function implements the _exit(2) system call.
 *
 * @param status the exit status of the process
 */
void
do_exit(int status)
{
		/*panic("do exit");
        NOT_YET_IMPLEMENTED("PROCS: do_exit");
       */
       KASSERT(curthr->kt_state!=KT_EXITED && "\nThe thread is already been marked as killed\n");
       kthread_exit(&status);
       
       
}

size_t
proc_info(const void *arg, char *buf, size_t osize)
{
        const proc_t *p = (proc_t *) arg;
        size_t size = osize;
        proc_t *child;

        KASSERT(NULL != p);
        KASSERT(NULL != buf);

        iprintf(&buf, &size, "pid:          %i\n", p->p_pid);
        iprintf(&buf, &size, "name:         %s\n", p->p_comm);
        if (NULL != p->p_pproc) {
                iprintf(&buf, &size, "parent:       %i (%s)\n",
                        p->p_pproc->p_pid, p->p_pproc->p_comm);
        } else {
                iprintf(&buf, &size, "parent:       -\n");
        }

#ifdef __MTP__
        int count = 0;
        kthread_t *kthr;
        list_iterate_begin(&p->p_threads, kthr, kthread_t, kt_plink) {
                ++count;
        } list_iterate_end();
        iprintf(&buf, &size, "thread count: %i\n", count);
#endif

        if (list_empty(&p->p_children)) {
                iprintf(&buf, &size, "children:     -\n");
        } else {
                iprintf(&buf, &size, "children:\n");
        }
   
        list_iterate_begin(&p->p_children, child, proc_t, p_child_link) {
                iprintf(&buf, &size, "     %i (%s)\n", child->p_pid, child->p_comm);
        } list_iterate_end();

        iprintf(&buf, &size, "status:       %i\n", p->p_status);
        iprintf(&buf, &size, "state:        %i\n", p->p_state);

#ifdef __VFS__
#ifdef __GETCWD__
        if (NULL != p->p_cwd) {
                char cwd[256];
                lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                iprintf(&buf, &size, "cwd:          %-s\n", cwd);
        } else {
                iprintf(&buf, &size, "cwd:          -\n");
        }
#endif /* __GETCWD__ */
#endif

#ifdef __VM__
        iprintf(&buf, &size, "start brk:    0x%p\n", p->p_start_brk);
        iprintf(&buf, &size, "brk:          0x%p\n", p->p_brk);
#endif

        return size;
}

size_t
proc_list_info(const void *arg, char *buf, size_t osize)
{
        size_t size = osize;
        proc_t *p;

        KASSERT(NULL == arg);
        KASSERT(NULL != buf);

#if defined(__VFS__) && defined(__GETCWD__)
        iprintf(&buf, &size, "%5s %-13s %-18s %-s\n", "PID", "NAME", "PARENT", "CWD");
#else
        iprintf(&buf, &size, "%5s %-13s %-s\n", "PID", "NAME", "PARENT");
#endif

        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                char parent[64];
                if (NULL != p->p_pproc) {
                        snprintf(parent, sizeof(parent),
                                 "%3i (%s)", p->p_pproc->p_pid, p->p_pproc->p_comm);
                } else {
                        snprintf(parent, sizeof(parent), "  -");
                }

#if defined(__VFS__) && defined(__GETCWD__)
                if (NULL != p->p_cwd) {
                        char cwd[256];
                        lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                        iprintf(&buf, &size, " %3i  %-13s %-18s %-s\n",
                                p->p_pid, p->p_comm, parent, cwd);
                } else {
                        iprintf(&buf, &size, " %3i  %-13s %-18s -\n",
                                p->p_pid, p->p_comm, parent);
                }
#else
                iprintf(&buf, &size, " %3i  %-13s %-s\n",
                        p->p_pid, p->p_comm, parent);
#endif
        } list_iterate_end();
        return size;
}
