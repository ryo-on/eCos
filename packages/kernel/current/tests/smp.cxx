//==========================================================================
//
//        smp.cxx
//
//        SMP tests
//
//==========================================================================
//####COPYRIGHTBEGIN####
//                                                                          
// -------------------------------------------                              
// The contents of this file are subject to the Red Hat eCos Public License 
// Version 1.1 (the "License"); you may not use this file except in         
// compliance with the License.  You may obtain a copy of the License at    
// http://www.redhat.com/                                                   
//                                                                          
// Software distributed under the License is distributed on an "AS IS"      
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the 
// License for the specific language governing rights and limitations under 
// the License.                                                             
//                                                                          
// The Original Code is eCos - Embedded Configurable Operating System,      
// released September 30, 1998.                                             
//                                                                          
// The Initial Developer of the Original Code is Red Hat.                   
// Portions created by Red Hat are                                          
// Copyright (C) 1998, 1999, 2000 Red Hat, Inc.                             
// All Rights Reserved.                                                     
// -------------------------------------------                              
//                                                                          
//####COPYRIGHTEND####
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):     nickg
// Contributors:  nickg
// Date:          2001-06-18
// Description:   Some basic SMP tests.
//
//####DESCRIPTIONEND####
//==========================================================================

#include <pkgconf/kernel.h>
#include <pkgconf/hal.h>

#if 1
#include <cyg/kernel/sched.hxx>
#include <cyg/kernel/thread.hxx>
#include <cyg/kernel/thread.inl>
#include <cyg/kernel/mutex.hxx>
#include <cyg/kernel/sema.hxx>
#include <cyg/kernel/sched.inl>
#include <cyg/kernel/clock.hxx>
#include <cyg/kernel/clock.inl>
#endif

#include <cyg/kernel/kapi.h>

#include <cyg/infra/testcase.h>
#include <cyg/infra/diag.h>

#define NTHREADS 1
#include "testaux.hxx"

//==========================================================================

#if defined(CYGPKG_KERNEL_SMP_SUPPORT) &&       \
    defined(CYGFUN_KERNEL_API_C) &&             \
    defined(CYGSEM_KERNEL_SCHED_MLQUEUE) &&     \
    defined(CYGVAR_KERNEL_COUNTERS_CLOCK) &&    \
    !defined(CYGPKG_HAL_I386_LINUX) &&          \
    !defined(CYGDBG_INFRA_DIAG_USE_DEVICE) &&   \
    (CYGNUM_KERNEL_SCHED_PRIORITIES > 12)

//==========================================================================

#define STACK_SIZE CYGNUM_HAL_STACK_SIZE_TYPICAL

#define NTHREADS_MAX (CYGNUM_KERNEL_CPU_MAX*3)

static int ncpus = CYGNUM_KERNEL_CPU_MAX;
static int nthread = NTHREADS_MAX;

static char stacks[NTHREADS_MAX][STACK_SIZE];
static cyg_thread test_threads[NTHREADS_MAX];
static cyg_handle_t threads[NTHREADS_MAX];

static volatile cyg_uint32 cpu_run[CYGNUM_KERNEL_CPU_MAX];
static volatile int failed = false;
static volatile cyg_handle_t cpu_thread[CYGNUM_KERNEL_CPU_MAX];

static volatile cyg_uint32 slicerun[NTHREADS_MAX][CYGNUM_KERNEL_CPU_MAX];


static cyg_mutex_t mx;


//==========================================================================
// Compute a name for a thread
char *
thread_name(char *basename, int indx) {
    return "<<NULL>>";  // Not currently used
}

//==========================================================================

void 
test_thread_cpu(CYG_ADDRESS id)
{
    for(;;)
        cpu_run[CYG_KERNEL_CPU_THIS()] = true;
}

//==========================================================================
// First test: just run as many threads as CPUs and check that we
// get to run on each CPU.

void run_smp_test_cpus()
{
    int i;

    CYG_TEST_INFO( "CPU Test: Check CPUs functional");
    
    // Init flags.
    for (i = 0;  i < ncpus;  i++)
        cpu_run[i] = false;
    
    // Set my priority higher than any I plan to create
    cyg_thread_set_priority(cyg_thread_self(), 2);

    for (i = 0;  i < ncpus;  i++) {
        cyg_thread_create(10,              // Priority - just a number
                          test_thread_cpu, // entry
                          i,               // index
                          thread_name("thread", i),     // Name
                          &stacks[i][0],   // Stack
                          STACK_SIZE,      // Size
                          &threads[i],     // Handle
                          &test_threads[i] // Thread data structure
            );
        cyg_thread_resume( threads[i]);
    }

    // Just wait a while, until the threads have all run for a bit.
    cyg_thread_delay( 1 );

    // Delete all the threads
    for (i = 0;  i < ncpus;  i++) {
        cyg_thread_delete(threads[i]);
    }

    // And check that a thread ran on each CPU
    for (i = 0;  i < ncpus;  i++) {
        CYG_TEST_CHECK( cpu_run[i], "CPU didn't run");
        if( !cpu_run[i] )
            failed++;
    }

    CYG_TEST_INFO( "CPU Test: done");
}


//==========================================================================

void 
test_thread_pri(CYG_ADDRESS id)
{
    for(;;)
    {
        cpu_thread[CYG_KERNEL_CPU_THIS()] = cyg_thread_self();
    }
}

//==========================================================================
// Second test: Run a thread on each CPU and then by manipulating the
// priorities, get the current thread to migrate to each CPU in turn.

void run_smp_test_pri()
{
    int i;

    CYG_TEST_INFO( "Pri Test: Check set_priority functionality");
    
    // Init flags.
    for (i = 0;  i < ncpus;  i++)
        cpu_run[i] = false;
    
    // Set my priority higher than any I plan to creat
    cyg_thread_set_priority(cyg_thread_self(), 2);

    for (i = 0;  i < ncpus;  i++) {
        cyg_thread_create(10,              // Priority - just a number
                          test_thread_pri, // entry
                          i,               // index
                          thread_name("thread", i),     // Name
                          &stacks[i][0],   // Stack
                          STACK_SIZE,      // Size
                          &threads[i],     // Handle
                          &test_threads[i] // Thread data structure
            );
        cyg_thread_resume( threads[i]);
    }

    cyg_handle_t cthread = threads[0];
    cyg_thread_set_priority(cthread, 25);
    
    // Just wait a while, until the threads have all run for a bit.
    cyg_thread_delay( 1 );

    for (i = 0;  i < ncpus*5;  i++)
    {
        HAL_SMP_CPU_TYPE cpu = i % CYG_KERNEL_CPU_COUNT();
        
        if( cpu != CYG_KERNEL_CPU_THIS() )
        {
            // At this point we have the current thread running on a
            // CPU at priority 2, ncpus-1 threads running at priority
            // 10 and the last thread (cthread) in the run queue at
            // priority 25.
            
            // Pick a thread on a different CPU
            cyg_handle_t dthread = cpu_thread[cpu];

            // Change the priority of the victim thread to 20. It is
            // still higher priority than cthread so it will continue
            // running.
            
            cyg_thread_set_priority(dthread, 20);

            // Now change our priority to 15. We are still higher
            // priority that cthread so we will still run.
            
            cyg_thread_set_priority(cyg_thread_self(), 15);

            // Finally change the priority of cthread to 10. This will
            // cause it to preempt us on the current CPU. In turn we
            // will preempt dthread on its CPU.

            // NOTE: This relies somewhat on the SMP scheduler doing
            // what we expect here. Specifically, that it will preempt
            // the current thread with cthread locally. A more
            // sophisticated scheduler might decide that the most
            // efficient thing to do is to preempt dthread with
            // cthread remotely, leaving the current thread where it
            // is. If we ever bother to implement this, then this test
            // will need to change.
            
            cyg_thread_set_priority(cthread, 10);

            // Spin here a while until the scheduler sorts itself out.
            
            for( int j = 0; j < 1000; j++ );

            // Indicate that we have run on this CPU
            cpu_run[CYG_KERNEL_CPU_THIS()]++;

            // Restore our priority to 2 and depress dthread to 25 and
            // make it the new cthread.
            
            cyg_thread_set_priority(cyg_thread_self(), 2);
            cyg_thread_set_priority(dthread, 25);
            cthread = dthread;
        }
    }

    
    // Delete all the threads
    for (i = 0;  i < ncpus;  i++) {
        cyg_thread_delete(threads[i]);
    }

    // And check that a thread ran on each CPU
    for (i = 0;  i < ncpus;  i++) {
        CYG_TEST_CHECK( cpu_run[i], "CPU didn't run");
        if( !cpu_run[i] )
            failed++;
    }

    CYG_TEST_INFO( "PRI Test: done");
}

//==========================================================================

void 
test_thread_timeslice(CYG_ADDRESS id)
{
    for(;;)
        slicerun[id][CYG_KERNEL_CPU_THIS()]++;
}

//==========================================================================
// First test: just run as many threads as CPUs and check that we
// get to run on each CPU.

void run_smp_test_timeslice()
{
    int i;

    CYG_TEST_INFO( "Timeslice Test: Check timeslicing works");
    
    // Init flags.
    for (i = 0;  i < nthread;  i++)
        for( int j = 0; j < ncpus; j++ )
            slicerun[i][j] = 0;
    
    // Set my priority higher than any I plan to create
    cyg_thread_set_priority(cyg_thread_self(), 2);

    for (i = 0;  i < nthread;  i++) {
        cyg_thread_create(10,              // Priority - just a number
                          test_thread_timeslice, // entry
                          i,               // index
                          thread_name("thread", i),     // Name
                          &stacks[i][0],   // Stack
                          STACK_SIZE,      // Size
                          &threads[i],     // Handle
                          &test_threads[i] // Thread data structure
            );
        cyg_thread_resume( threads[i]);
    }

    // Just wait a while, until the threads have all run for a bit.
    cyg_thread_delay( 20 );

    // Delete all the threads
    for (i = 0;  i < nthread;  i++) {
        cyg_thread_suspend(threads[i]);
    }

    
    // And check that a thread ran on each CPU

    cyg_uint32 cpu_total[ncpus];
    cyg_uint32 cpu_threads[ncpus];
    cyg_uint32 thread_total[nthread];
    
    diag_printf(" Thread ");
    for( int j = 0; j < ncpus; j++ )
    {
        cpu_total[j] = 0;
        cpu_threads[j] = 0;
        diag_printf("   CPU %2d",j);
    }
    diag_printf("   Total\n");
    for (i = 0;  i < nthread;  i++)
    {
        thread_total[i] = 0;
        diag_printf("     %2d ",i);
        for( int j = 0; j < ncpus; j++ )
        {
            thread_total[i] += slicerun[i][j];
            cpu_total[j] += slicerun[i][j];
            if( slicerun[i][j] > 0 )
                cpu_threads[j]++;
            diag_printf(" %8d",slicerun[i][j]);
        }
        diag_printf("%8d\n",thread_total[i]);
    }
    diag_printf(" Total  ");
    for( int j = 0; j < ncpus; j++ )
        diag_printf(" %8d",cpu_total[j]);
    diag_printf("\n");
    diag_printf("Threads ");
    for( int j = 0; j < ncpus; j++ )
    {
        diag_printf(" %8d",cpu_threads[j]);
        if( cpu_threads[j] < 2 )
            failed++;
    }
    diag_printf("\n");

    // Delete all the threads
    for (i = 0;  i < nthread;  i++) {
        cyg_thread_delete(threads[i]);
    }

    CYG_TEST_INFO( "Timeslice Test: done");
}


//==========================================================================

void 
run_smp_tests(CYG_ADDRESS id)
{
    cyg_mutex_init( &mx );

    for( int i = 0; i < 10; i++ )
    {
        run_smp_test_cpus();
        run_smp_test_pri();
        run_smp_test_timeslice();
    }

    if( failed )
        CYG_TEST_FAIL_FINISH("SMP tests failed\n");
    
    CYG_TEST_PASS_FINISH("SMP tests OK");    
}

//==========================================================================

void smp_main( void )
{
    CYG_TEST_INIT();

    // Work out how many CPUs we actually have.
    ncpus = CYG_KERNEL_CPU_COUNT();

    new_thread(run_smp_tests, 0);

    cyg_scheduler_start();
}

//==========================================================================

#ifdef CYGSEM_HAL_STOP_CONSTRUCTORS_ON_FLAG
externC void
cyg_hal_invoke_constructors();
#endif

externC void
cyg_start( void )
{
#ifdef CYGSEM_HAL_STOP_CONSTRUCTORS_ON_FLAG
    cyg_hal_invoke_constructors();
#endif
    smp_main();
}   

//==========================================================================

#else // CYGPKG_KERNEL_SMP_SUPPORT etc.

externC void
cyg_start( void )
{
    CYG_TEST_INIT();
    CYG_TEST_NA("SMP test requires:\n"
                "CYGPKG_KERNEL_SMP_SUPPORT &&\n"
                "CYGFUN_KERNEL_API_C && \n"
                "CYGSEM_KERNEL_SCHED_MLQUEUE &&\n"
                "CYGVAR_KERNEL_COUNTERS_CLOCK &&\n"
                "!CYGPKG_HAL_I386_LINUX &&\n"
                "!CYGDBG_INFRA_DIAG_USE_DEVICE &&\n"
                "(CYGNUM_KERNEL_SCHED_PRIORITIES > 12)\n");
}
#endif // CYGPKG_KERNEL_SMP_SUPPORT etc.

//==========================================================================
// EOF tm_basic.cxx
