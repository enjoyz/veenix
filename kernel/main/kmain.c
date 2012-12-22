#include "types.h"
#include "globals.h"
#include "kernel.h"

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/cpuid.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/tty/virtterm.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"

#include "test/kshell/kshell.h"

GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);

static context_t bootstrap_context;

/**
 * This is the first real C function ever called. It performs a lot of
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
        GDB_CALL_HOOK(boot);
	
	dbg_init();

        dbgq(DBG_CORE, "Kernel binary:\n");
        dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
        dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
        dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);

        page_init();

        pt_init();
        slab_init();
        pframe_init();

        acpi_init();
        apic_init();
        intr_init();

        gdt_init();

        /* initialize slab allocators */
#ifdef __VM__
        anon_init();
        shadow_init();
#endif
        vmmap_init();
        proc_init();
        kthread_init();

#ifdef __DRIVERS__
        bytedev_init();
        blockdev_init();
#endif

        void *bstack = page_alloc();
        pagedir_t *bpdir = pt_get();
        KASSERT(NULL != bstack && "Ran out of memory while booting.");
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
bootstrap(int arg1, void *arg2)
{
        /* necessary to finalize page table information */
        pt_template_init();

	/* NOT_YET_IMPLEMENTED("PROCS: bootstrap");    */
		
		proc_t *start_proc=proc_create("idle");
		kthread_t *init_thread=kthread_create(start_proc,idleproc_run, 0,NULL);
		curproc=start_proc;
		curthr=init_thread;
		context_make_active(&curthr->kt_ctx);
		panic("weenix returned to bootstrap()!!! BAD!!!\n");
		return NULL;	
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
idleproc_run(int arg1, void *arg2)
{
		
        int status;
        pid_t child;      
        /* create init proc */
        kthread_t *initthr = initproc_create();
   
        init_call_all();
        
		GDB_CALL_HOOK(initialized);

        /* Create other kernel threads (in order) */

#ifdef __VFS__
        /* Once you have VFS remember to set the current working directory
         * of the idle and init processes */

        /* Here you need to make the null, zero, and tty devices using mknod */
        /* You can't do this until you have VFS, check the include/drivers/dev.h
         * file for macros with the device ID's you will need to pass to mknod */

          curproc->p_cwd=vfs_root_vn;
          initthr->kt_proc->p_cwd=vfs_root_vn;
          vref(curproc->p_cwd);
#endif

        /* Finally, enable interrupts (we want to make sure interrupts
         * are enabled AFTER all drivers are initialized) */
        intr_enable();
		/* Run initproc */
      
		sched_make_runnable(initthr);
		
        /* Now wait for it */
        child = do_waitpid(-1, 0, &status);
        KASSERT(PID_INIT == child);
#ifdef __MTP__
        kthread_reapd_shutdown();
#endif
 
#ifdef __VFS__
        /* Shutdown the vfs: */
        dbg_print("weenix: vfs shutdown...\n");
        vput(curproc->p_cwd);
        if (vfs_shutdown())
                panic("vfs shutdown FAILED!!\n");

#endif

        /* Shutdown the pframe system */
#ifdef __S5FS__
        pframe_shutdown();
#endif

        dbg_print("\nweenix: halted cleanly!\n");
        GDB_CALL_HOOK(shutdown);
        hard_shutdown();
        return NULL;
}
 
/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *
initproc_create(void)
{
  	/*      NOT_YET_IMPLEMENTED("PROCS: initproc_create");
      		return NULL;*/
	
	proc_t *init_proc=proc_create("init_proc");
	kthread_t *init_thread=kthread_create(init_proc,initproc_run, 0,NULL);
	KASSERT(init_proc->p_pid==PID_INIT && "\nThe init process pid is wrong\n");

	return init_thread;
}

/** 
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/bin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */

kmutex_t mm1;
proc_t *p1, *p2, *p3,*p13,*p14,*p15,*p16,*p20;
kthread_t *k1, *k2, *k3,* k13,*k14,*k15,*k16,*k20;
int test_var=0,test_var1=0,flag_dead=0;

static void *
mynewfunc3()
{
	dbg_print("\nThis is Mutexfun3, called after Mutexfucn1 if the mutex is still LOCKED else should be called after Mutexfunc2");
	dbg_print("\n\nMutexfunc3 is now executing");
	dbg_print("\nMutexfunc3 finished executing");
    return NULL;

}
static void *
mynewfunc2()
{
		int i;
		dbgq(DBG_TEST, "\nThis is Mutexfunc2 should  execute only after UNLOCKED!");

	kmutex_lock(&mm1);
		
					dbg_print("\nMutexfunc2 is now executing");
			dbg_print("\nMutexfunc2 finished executing\n");
   kmutex_unlock(&mm1);
    
    return NULL;

}



static void *
mynewfunc1()
{
	dbg_print("\n\n\tMUTEX TEST\n\n");
	int i;
	   dbgq(DBG_TEST, "\n\nThis is first function(Mutexfunc1) that should be called for mutex");
   kmutex_lock(&mm1);
   	dbgq(DBG_TEST, "\nMutexfunc1 is now executing & locked the mutex");
     for( i=1;i<=100;i++)
    {
    	dbg_print("\n%d",i);
    	if(i==50)
    	{
    		dbgq(DBG_TEST, "\nKeeping the lock in the Mutexfunc1 and calling Mutexfunc2");
    		sched_make_runnable(curthr);
    		sched_switch();
    	}
    }	
    dbgq(DBG_TEST,"\nMutexfunc1 finished executing");
    kmutex_unlock(&mm1);
    dbgq(DBG_TEST, "\nMutex Unlocking!");
    
	
    
    return NULL;
} 


static void *
mynewfunc4()
{
	
	test_var++;
	dbg_print("\nthe function ran for the %d time by the %s",test_var,curproc->p_comm);
	if(test_var==5)
	dbgq(DBG_TESTPASS, "\n\n\tInorder child creation and scheduling is fine.\n\n");
	
	return NULL;
} 

static void *
mynewfunc5()
{
	
	test_var1++;
	dbg_print("\nthe function ran for the %d time by the %s",test_var1,curproc->p_comm);
	if(test_var1==5)
	dbgq(DBG_TESTPASS, "\n\n\tOut of order child creation and scheduling is fine.\n\n");
	
	return NULL;
} 

static void *
mynewfunc7()
{
	
	dbg_print("\nCurrent Process:%s is executing and its parent is:%s",curproc->p_comm,curproc->p_pproc->p_comm); 
	if(flag_dead==1)
	KASSERT(curproc->p_pid==PID_INIT);
	
	return NULL;
} 
static void *
mynewfunc6()
{
	
	
	p14 = proc_create("Zombie_test_child1");
    k14 = kthread_create(p14, mynewfunc7, 0, 0);
    
   	p15 = proc_create("Zombie_test_child2");
    k15 = kthread_create(p15, mynewfunc7, 0, 0);
    
  	p16 = proc_create("Zombie_test_child3");
    k16 = kthread_create(p16, mynewfunc7, 0, 0);
    
    dbg_print("\n%s is created and its parent is:%s",p14->p_comm,curproc->p_comm); 
    dbg_print("\n%s is created and its parent is:%s",p15->p_comm,curproc->p_comm); 
    dbg_print("\n%s is created and its parent is:%s",p16->p_comm,curproc->p_comm); 
    
    sched_make_runnable(k14);
	sched_make_runnable(k15);
	sched_make_runnable(k16);   
	
	dbg_print("\nKilling the process %s",curproc->p_comm);
	proc_kill(p20,0); 
	flag_dead=1;
	return NULL;
} 





 

 
static void *
initproc_run(int arg1, void *arg2)
{
        /*NOT_YET_IMPLEMENTED("PROCS: initproc_run");*/
      	
      	/*    TEST FOR MUTEX CONDITION          */
      	
      	dbg_print("\nIn the INIT process now...\n");
      	KASSERT(curproc->p_pid==PID_INIT);
      	
      	
      	kmutex_init(&mm1);
      
      	
		    proc_t* p1 = proc_create("Mutexfunc1");
        kthread_t* k1 = kthread_create(p1, mynewfunc1, arg1, arg2);
        
       	proc_t* p2 = proc_create("Mutexfunc2");
        kthread_t* k2 = kthread_create(p2, mynewfunc2, arg1, arg2);
        
      	proc_t* p3 = proc_create("Mutexfunc3");
        kthread_t* k3 = kthread_create(p3, mynewfunc3, arg1, arg2);
        
        
        sched_make_runnable(k1);
        sched_make_runnable(k2);
        sched_make_runnable(k3);
        dbgq(DBG_TESTPASS, "\n\n\tMutex Conditions working fine.\n\n");
       /*  END OF TEST FOR MUTEX CONDITION          */   
       do_waitpid(-1,0,0);  
     
     	/*  TEST FOR INORDER EXECUTION   */
     	
     	proc_t* p4 = proc_create("Inorder_function1");
        kthread_t* k4 = kthread_create(p4, mynewfunc4, arg1, arg2);
       
        proc_t* p5 = proc_create("Inorder_function2");
        kthread_t* k5 = kthread_create(p5, mynewfunc4, arg1, arg2);
       
        proc_t* p6 = proc_create("Inorder_function3");
        kthread_t* k6 = kthread_create(p6, mynewfunc4, arg1, arg2);
        proc_t* p7 = proc_create("Inorder_function4");
        kthread_t* k7 = kthread_create(p7, mynewfunc4, arg1, arg2);
        proc_t* p8 = proc_create("Inorder_function5");
        kthread_t* k8 = kthread_create(p8, mynewfunc4, arg1, arg2);
       
        dbgq(DBG_TEST, "\n\n\tInorder child creation test starting.\n\n");
        
        
        
         sched_make_runnable(k4);
         sched_make_runnable(k5);
         sched_make_runnable(k6);
         sched_make_runnable(k7);
         sched_make_runnable(k8);
         
         do_waitpid(-1,0,0);
         
         
         
        proc_t *  p13 = proc_create("out_of_order_function5");
        kthread_t * k13 = kthread_create(p13, mynewfunc5, arg1, arg2);
         
        proc_t * p9 = proc_create("out_of_order_function1");
        kthread_t * k9 = kthread_create(p9, mynewfunc5, arg1, arg2);
        
      proc_t *   p11 = proc_create("out_of_order_function3");
      kthread_t *   k11 = kthread_create(p11, mynewfunc5, arg1, arg2);
       
      proc_t *   p10 = proc_create("out_of_order_function2");
      kthread_t *  k10 = kthread_create(p10, mynewfunc5, arg1, arg2);
       
       
      proc_t *  p12 = proc_create("out_of_order_function4");
        kthread_t * k12 = kthread_create(p12, mynewfunc5, arg1, arg2);
        
       
        dbgq(DBG_TEST, "\n\n\tInorder child creation test starting.\n\n");
         sched_make_runnable(k13);
         sched_make_runnable(k9);
         sched_make_runnable(k11);
         sched_make_runnable(k10);
         sched_make_runnable(k12);
         
         
         
         do_waitpid(p11->p_pid,0,0);
         do_waitpid(p10->p_pid,0,0);
         do_waitpid(p9->p_pid,0,0);
         do_waitpid(p13->p_pid,0,0);
         do_waitpid(p12->p_pid,0,0);
         
         
        dbg_print("\n\n\n\tZombie Process test case\n\n\n");
         
          p20 = proc_create("Zombies_test_parent");
        k20 = kthread_create(p20, mynewfunc6, arg1, arg2);
       
       sched_make_runnable(k20);
        
        do_waitpid(p20->p_pid,0,0);
        dbgq(DBG_TESTPASS, "\n\n\tZombies Process test case working!\n\n");
        
        
        do_waitpid(-1,0,0);
        return NULL;

}
    
/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
        vt_print_shutdown();
#endif
        __asm__ volatile("cli; hlt");
}
