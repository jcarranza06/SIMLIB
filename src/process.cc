/////////////////////////////////////////////////////////////////////////////
//! \file process.cc  Implementation of quasiparallel processes (coroutines)
//!
//! This module contains implementation of cooperative multitasking processes
//! for discrete simulation

//
// Copyright (c) 1991-2022 Petr Peringer
//
// This library is licensed under GNU Library GPL. See the file COPYING.
//

//
// Implementation of interruptable functions (non-preemptive, coroutine-like)
//
// We need code to save/restore process stack contents and working setjmp/longjmp
// This approach has advantage in smaller memory requirements.
//
// LIMITATIONS:
//  - do not use (e.g. using pointers) any locals in Process::Behavior() if it
//    is not in running state (stack contents is moved to heap)
//  - do not use too big locals in Process::Behavior() for performance reasons
//
// WARNING: dirty hacks inside
//

// TODO: add implementation with stack switching (not copying)
//       as compile-time option
// TODO: add implementation using C++20 coroutines?


////////////////////////////////////////////////////////////////////////////
// interface
//

#include "simlib.h"
#include "internal.h"

#include <csetjmp>
#include <cstring>

static_assert(sizeof(void*) >= 4, "not tested on <32bit systems");

////////////////////////////////////////////////////////////////////////////
// implementation
//

namespace simlib3 {

SIMLIB_IMPLEMENTATION;

////////////////////////////////////////////////////////////////////////////

/**
 * internal structure for storing of Process::Behavior() context
 * @ingroup process
 */
struct P_Context_t {
    jmp_buf status;     //!< stored SP, IP, and other registers
    size_t size;        //!< size of following array (allocated on heap)
    char stack[1];      //!< saved stack contents
};

////////////////////////////////////////////////////////////////////////////
// global variables (should be volatile)
// (P_ means Process)
// TODO: wrap to thread_local struct together with (almost) all globals
static jmp_buf P_DispatcherStatusBuffer; //!< setjmp() state before dispatch
static char *volatile P_StackBase = 0;   //!< global start of stack area
static char *volatile P_StackBase2 = 0;  //!< used for checking only

static P_Context_t *volatile P_Context = 0; //!< temporary global process state
static volatile size_t P_StackSize = 0;     //!< temporary global stack size

////////////////////////////////////////////////////////////////////////////
// Support for debugging:
////////////////////////////////////////////////////////////////////////////

#define EXTRA_DEBUG 0   // 0==off, 1==on

#if EXTRA_DEBUG

/// \def DEBUG_PROCESS
/// Macro for detailed debugging of Process switching code
#define DEBUG_PROCESS(n) DEBUG_PROCESS_f(n)

/// \fn DEBUG_PROCESS_f
/// We need? (FIXME) this non-inline function because optimization of volatile global
/// variable access after longjmp -> setjmp transition in 32bit i686 mode of
/// GCC 7 and newer versions
// TODO: add string table? remove?
[[gnu::noinline]] static void DEBUG_PROCESS_f(int n) {
    switch(n) {
    case 1:
        DEBUG(DBG_PROCESS,("| PROCESS_INTERRUPT ***** begin *****"));
        DEBUG(DBG_PROCESS,("|   %d) - stack size = %p", n, P_StackSize));
        break;
    case 2:
        break;
    case 3:
        break;
    case 4:
        DEBUG(DBG_PROCESS,("|   %d) PROCESS_SAVE_STACK: before setjmp() - context=%p", n, P_Context));
        break;
    case 5:
        DEBUG(DBG_PROCESS,("|   %d) PROCESS_SAVE_STACK: after setjmp()  - context=%p", n, P_Context));
        break;
    case 6:
        DEBUG(DBG_PROCESS,("|   %d) PROCESS_RESTORE: longjmp() back     - context=%p", n, P_Context));
        break;
    case 7:
        DEBUG(DBG_PROCESS,("| PROCESS_INTERRUPT ***** end *****"));
        break;

    case 0xB:
        DEBUG(DBG_PROCESS,("| b) PROCESS Shift SP to %p (or more)", P_StackBase2));
        break;
    case 0xC:
        DEBUG(DBG_PROCESS,("| c) PROCESS Before longjmp(%p,1)", P_Context->status));
        break;

    default:
        DEBUG(DBG_PROCESS,("|   %d) - context = %p", n, P_Context));
        break;
    }
}
#else
#define DEBUG_PROCESS(n)
#endif

////////////////////////////////////////////////////////////////////////////
// Specialized asymmetric stackfull coroutine implementation:
////////////////////////////////////////////////////////////////////////////

[[gnu::noinline]] static void PROCESS_INTERRUPT_f(); // special function

/// interrupt process behavior execution, continue after return
#define PROCESS_INTERRUPT()                                              \
{ /* This should be MACRO */                                            \
  /* if(!isCurrent())  SIMLIB_error("Can't interrupt..."); */           \
  this->_status = _INTERRUPTED;                                         \
  PROCESS_INTERRUPT_f();                                                 \
  this->_status = _RUNNING;                                             \
  this->_context = 0;                                                   \
}

/// does not save context
#define PROCESS_EXIT() \
    longjmp(P_DispatcherStatusBuffer, 2)  // jump to dispatcher

// FIXME: allocation/freeing memory is expensive, should be optimized

/// \def ALLOC_CONTEXT
/// allocate memory for process context, sz = size of stack area to save
[[gnu::noinline]] static void ALLOC_CONTEXT(size_t sz) noexcept try
{
    P_Context = (P_Context_t *) new char[sizeof(P_Context_t) + sz];
    P_Context->size = sz;
} catch(std::bad_alloc&) {
    SIMLIB_error("ALLOC_CONTEXT: no memory");
}

/// \fn FREE_CONTEXT
/// deallocate saved process context
[[gnu::noinline]] static void FREE_CONTEXT() noexcept {
    delete[] (char *)(P_Context);
    P_Context = 0;
}

////////////////////////////////////////////////////////////////////////////
/// Process constructor
/// sets state to PREPARED
Process::Process(Priority_t p) : Entity(p) {
  Dprintf(("Process::Process(%d)", p));
  _wait_until = false;
  _context = 0;                 // pointer to process context
  _status = _PREPARED;          // prepared for running
}

////////////////////////////////////////////////////////////////////////////
/// Process destructor
/// Sets state to TERMINATED and
/// removes process from queue/calendar/waituntil list.
/// TODO: Warn if this==Current? (e.g. "delete this;" in Behavior())
Process::~Process()
{
    Dprintf(("Process::~Process()"));

    //if(this==Current) SIMLIB_warning("Currently running process self-destructed");

    // destroy context data
    delete [] static_cast<char*>(_context);
    _context = 0;

    _status = _TERMINATED;

    if (_wait_until) {
        _WaitUntilRemove();     // remove from wait-until list
    }

    if (Where() != 0) {         // if waiting in queue
        Out();                  // remove from queue, no warning
    }

    if (!Idle()) {              // if process is scheduled
        SQS::Get(this);         // remove from calendar
    }
}

#if 1
////////////////////////////////////////////////////////////////////////////
/// Name of the process
/// Each process can be named, default is "Process#<number>"
std::string Process::Name() const
{
    const std::string name = SimObject::Name();
    if (!name.empty())
        return name;            // has explicit name
    else
        return SIMLIB_create_tmp_name("Process#%lu", _Ident);
}
#endif

////////////////////////////////////////////////////////////////////////////
/// Interrupt process behavior - this ensures WaitUntil tests
/// WARNING: use with care - it can run higher (or equal) priority processes
///          before continuing
/// TODO: Works almost like Wait(0), used only with WaitUntil
void Process::Interrupt()
{
    Dprintf(("Process#%lu.Interrupt()", _Ident));
    if (!isCurrent())
        return;                 // quasiparallel, TODO: Error?
    // continue after other processes WaitUntil checks
    // TODO: use >highest priority to eliminate problem
    Entity::Activate(); // schedule now - can run higher priority
                        //                processes before this one
    PROCESS_INTERRUPT();
    // TODO: return to previous priority
}

////////////////////////////////////////////////////////////////////////////
/// Activate process at time t
void Process::Activate(double t)
{
    Dprintf(("Process#%lu.Activate(%g)", _Ident, t));
    Entity::Activate(t);                        // (re)scheduling
    if (!isCurrent())
        return;
    PROCESS_INTERRUPT();
}

////////////////////////////////////////////////////////////////////////////
/// Wait for dtime
/// The same as Activate(Time+dtime)
void Process::Wait(double dtime)
{
    Dprintf(("Process#%lu.Wait(%g)", _Ident, dtime));
    Entity::Activate(double (Time) + dtime);    // (re)scheduling
    if (!isCurrent())
        return;
    PROCESS_INTERRUPT();
}

////////////////////////////////////////////////////////////////////////////
/// Seize facility f with optional priority of service sp
/// possibly waiting in input queue, if it is busy
void Process::Seize(Facility & f, ServicePriority_t sp /* = 0 */ )
{
    f.Seize(this, sp);          // polymorphic interface
}

////////////////////////////////////////////////////////////////////////////
/// Release facility f
/// possibly activate first waiting entity in queue
void Process::Release(Facility & f)
{
    f.Release(this);            // polymorphic interface
}

////////////////////////////////////////////////////////////////////////////
/// Enter - use cap capacity of store s
/// possibly waiting in input queue, if not enough free capacity
void Process::Enter(Store & s, unsigned long cap)
{
    s.Enter(this, cap);         // polymorphic interface
}

////////////////////////////////////////////////////////////////////////////
/// Leave - return cap capacity of store s
/// and enter first waiting entity from queue, which can use free capacity
void Process::Leave(Store & s, unsigned long cap)
{
    s.Leave(cap);               // polymorphic interface
    //TODO: should be parametrized: use first-only, first-usable, all-usable
}

////////////////////////////////////////////////////////////////////////////
/// Insert current process into queue
/// The process can be at most in single queue. Moves if in other queue.
void Process::Into(Queue & q)
{
    if (Where() != 0) {
        SIMLIB_warning("Process is already in (other) queue");
        Out();          // if already in queue then remove
    }
    q.Insert(this);     // polymorphic interface
}

////////////////////////////////////////////////////////////////////////////
/// Process deactivation
/// To continue the behavior it should be activated again
/// Warning: memory leak if not activated/deleted explicitly (FIXME)
void Process::Passivate()
{
    Dprintf(("Process#%lu.Passivate()", id()));
    Entity::Passivate();
    if (!isCurrent())
        return;         // passivated by other process
    PROCESS_INTERRUPT();
}

////////////////////////////////////////////////////////////////////////////
/// Terminate the process
/// If called by current process, self-destruct.
/// Remove from queue, unschedule from calendar.
/// Automatically free the memory of the process.
void Process::Terminate()
{
    Dprintf(("Process#%lu.Terminate()", _Ident));

    // remove from all queues  TODO: write special method for this
    if (Where() != 0) {         // Entity linked in queue
        Out();                  // remove from queue, no warning
    }
    if (!Idle())
        SQS::Get(this);         // remove from calendar

    // end of Process
    if (isCurrent()) {          // if currently running process
        _status = _TERMINATED;
        PROCESS_EXIT();          // jump back to dispatcher
    }
    else {
        _status = _TERMINATED;
        if (isAllocated())
            delete this;        // remove passive process
    }
}

////////////////////////////////////////////////////////////////////////////
#define CANARY1 (reinterpret_cast<long>(this)-1) // unaligned value is better


/// This is theoretical max stack size for single SIMLIB Process at time of
/// interruption (should never be too big, because of copying the stack contents)
/// FIXME: should be checked at Process::Behavior interrupt for more precise error message
#define MAX_PROCESS_STACK_SIZE  1000000UL

[[noreturn,gnu::noinline]] static void restore_context2(volatile char *) noexcept;

/// This elliminates the need for explicit stack pointer adjustment.
/// The local array should not be optimized away.
/// Parameters, return address and local variables will be overwritten.
/// Never returns.
[[noreturn,gnu::noinline]] static void restore_context(size_t data_size) noexcept
{
    volatile char skip[MAX_PROCESS_STACK_SIZE]; // should never be optimized
    //skip[MAX_PROCESS_STACK_SIZE-1]='*';
    if(data_size>MAX_PROCESS_STACK_SIZE)
        SIMLIB_error("Process stack size limit exceeded");
    DEBUG_PROCESS(0xB);
    if(skip > P_StackBase2)
        SIMLIB_error("Process stack error");
    restore_context2(skip);
}

/// This function overwrites stack contents. It never returns, see longjmp.
[[noreturn,gnu::noinline]] static void restore_context2(volatile char *) noexcept
{
            // c) Copy saved stack contents back to stack
            memcpy((void *) (P_StackBase - P_StackSize), P_Context->stack, P_StackSize);
            DEBUG_PROCESS(0xC);

            // 4) Restore proces status (registers: SP,IP,...)
            longjmp(P_Context->status, 1);
            // ===========================================================
            // never reach this point - longjmp never returns
}

/**
 * \fn Process::_Run
 * Process dispatch method
 *
 * The dispatcher starts/reactivates process Behavior() method
 *
 * IMPORTANT notes:
 * - Function should be called from single place in simulation-control
 *   algorithm, because it is sensitive to stack-frame position
 *   (it saves/restores the stack contents).
 *
 * This function:
 *  1) saves current context (position on stack and CPU registers)
 *  2) calls Behavior() like any other function
 *  3) interruption of Behavior() saves context+ and jumps back
 *  4) to continue Behavior(), do 1) and move stack pointer away
 *  5) copy saved stack content back to stack
 *  6) do longjmp() to restore Behavior() execution
 *
 * @ingroup process
 */
void Process::_Run() noexcept // no exceptions
{
    // WARNING: all local variables should be volatile (see setjmp manual)
    static const char * status_strings[] = {
        "unknown", "PREPARED", "RUNNING", "INTERRUPTED", "TERMINATED"
    };
    Dprintf(("%016p===Process#%lu._Run() status=%s", this, _Ident, status_strings[_status]));

    if (_status != _INTERRUPTED && _status != _PREPARED)
        SIMLIB_error(ProcessNotInitialized);

    // Mark the stack base address
    volatile long mylocal = CANARY1;     // on stack variable
    // Warning: DO NOT USE ANY OTHER LOCAL VARIABLES in this function!
    P_StackBase = (char*)(&mylocal + 1);

    //
    // STACK layout (stack grows down):
    //
    //                  |   ...    |
    //                  |          |
    // P_StackBase  +-> +----------+
    //              |   | mylocal  |
    //              |   +----------+
    //              |   |          |
    //              |   |   ...    |
    //     _size    |   | Arguments, return addresses, locals, etc.
    //              |   |   ...    |
    //              |   |          |
    //              |   +----------+
    //              |   | mylocal2 |
    //              +-> +----------+
    //              |   |          |
#   define STACK_RESERVED 0x080 // reserved area for "red zone" 128B ?
    //              |   |          |
    // P_StackBase2 +-> +----------+
    //                      ...
    // locals and return address of restore_context2
    //                      ...
    //                      ...
    // SIMLIB stack limit (defined by MAX_PROCESS_STACK_SIZE implementation defined constant)
    // (limit holds only when Process::Behavior is interrupted and it's context saved)
    //                      ...
    //                      ...
    //                      ...
    // OS stack size limit (e.g. 8MiB on Linux: ulimit -s)


#if EXTRA_DEBUG
    DEBUG(DBG_PROCESS,("| PROCESS_STACK_BASE=%016p", P_StackBase));
    // CHECK if the P_StackBase position is the same in each call
    static char *P_StackBase0=0;
    if(P_StackBase0==0)
        P_StackBase0=P_StackBase;
    else if (P_StackBase!=P_StackBase0)
        SIMLIB_error("Internal error: P_StackBase not constant");
#endif

    //  2) mark current CPU context (part of context)
    if (!setjmp(P_DispatcherStatusBuffer))
    {
        // setjmp returned after saving current status
        _status = _RUNNING;
        if (_context == 0) {    // process start
            DEBUG(DBG_PROCESS, ("| --- Process::Behavior() START "));
            Behavior();         // run behavior description
            DEBUG(DBG_PROCESS, ("| --- Process::Behavior() END "));
            _status = _TERMINATED;
            if(mylocal != CANARY1)
                SIMLIB_error("Process canary1 died after Behavior() return");
            // Remove from any queue
            if (Where() != 0) {         // Entity linked in queue
                Out();                  // Remove from queue, no warning
            }
            if (!Idle())
                SQS::Get(this);         // Remove from calendar
            //TODO: if(in any facility) error
        } else { // process was interrupted and has saved context
            DEBUG(DBG_PROCESS, ("| --- Process::Behavior() CONTINUE "));
            mylocal = 0; // for checking only - previous value should be saved and later restored
            // RESTORE_CONTEXT
            // a) Save local variables to global
            // This is important because of following stack manipulations.
            P_Context = (P_Context_t*) this->_context;
            P_StackSize = P_Context->size;

            // b) Shift stack pointer under the currently restored stack area
            // This is very important for next memcpy and longjmp
            // (stack grows down), we reserve some more space
            P_StackBase2 = P_StackBase - P_StackSize - STACK_RESERVED;

            restore_context(P_StackSize+STACK_RESERVED);
            // never reach this point - longjmp (called from restore_context) never returns

        }
    }
    else
    {   // setjmp: back from Behavior() - interrupted or terminated

        if(mylocal != CANARY1)
            SIMLIB_error("Process implementation canary1 died");

        if(!isTerminated()) {
            // Interrupted process
            // Store content in global variables back to attributes
            P_Context->size = P_StackSize;
            this->_context = P_Context;
            DEBUG(DBG_PROCESS,("| --- Process::Behavior() INTERRUPT %p.context=%p, size=%d", \
                                                this, P_Context, P_StackSize));
            P_Context = 0; // cleaning
        }
    }

    Dprintf(("%016p===Process#%lu._Run() RETURN status=%s", this, _Ident, status_strings[_status]));

    //TODO: MOVE to simulation control loop
    if (isTerminated() && isAllocated()) {
        // terminated process on heap
        DEBUG(DBG_PROCESS,("| Process %p ends and is deallocated now",this));
        delete this;    // destroy process
    }
    // return to simulation control
}


////////////////////////////////////////////////////////////////////////////
#define CANARY2 0xDEADBEEFUL
/**
 * \fn PROCESS_INTERRUPT_f
 * Special function called from Process::Behavior() directly or indirectly.
 *
 * This function:
 *  1) computes stack data size,
 *  2) allocates memory for stack data,
 *  3) saves stack data to allocated memory,
 *  4) saves CPU context using setjmp(), and
 *  5) interrupts execution of current function using longjmp()
 *     to process dispatcher code,
 *  == (now runs dispatcher and other code)
 *  6) continues execution after longjmp from dispatcher.
 *  7) frees memory allocated at 2)
 *
 * Warning: This function is critical to process switching code and
 *          should not be inlined! It is never called directly by SIMLIB user.
 *
 * @ingroup process
 */
[[gnu::noinline]] static void PROCESS_INTERRUPT_f()
{
    // SAVE THE STACK STATE of the thread

    // 1) compute stack context size  (from P_StackBase to next variable)
    volatile unsigned mylocal2 = CANARY2;       // on-stack variable
    // Warning: DO NOT USE ANY OTHER LOCAL VARIABLES in this function!
    P_StackSize = (size_t) (P_StackBase - (char *) (&mylocal2));
    DEBUG_PROCESS(1);

    // 2) allocate memory for current context
    ALLOC_CONTEXT(P_StackSize);

    // 3) save stack data (stack grows DOWN)
    memcpy(P_Context->stack, (P_StackBase - P_StackSize), P_StackSize);

    mylocal2 = 0;       // previous value is saved (will be restored later)

    // STACK CONTENTS SAVED

    // 4) save CPU context (see man setjmp for details)
    DEBUG_PROCESS(4);
    if (!setjmp(P_Context->status)) {
        ////////////////////////////////////////////////////////////////////
        // 5) Interrupt the execution of Behavior()
        DEBUG_PROCESS(5);
        longjmp(P_DispatcherStatusBuffer, 1);     // --> longjmp back to dispatcher
        // longjmp never returns
    }

    // Check canary on stack after restore
    if (mylocal2 != CANARY2)
        SIMLIB_error("Process switching canary2 died.");

    ////////////////////////////////////////////////////////////////////////
    // 6) Continue execution after longjmp from dispatcher
    // On stack data and CPU context were restored by dispatcher
    DEBUG_PROCESS(6);

    // 7) free memory of already restored context
    FREE_CONTEXT();
    DEBUG_PROCESS(7);
    // return and continue Process::Behavior() execution
}

} // namespace

