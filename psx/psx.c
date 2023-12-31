/*
 * Copyright (c) 2019,20 Andrew G Morgan <morgan@kernel.org>
 *
 * This file contains a collection of routines that perform thread
 * synchronization to ensure that a whole process is running as a
 * single privilege entity - independent of the number of pthreads.
 *
 * The whole file would be unnecessary if glibc exported an explicit
 * psx_syscall()-like function that leveraged the nptl:setxid
 * mechanism to synchronize thread state over the whole process.
 */
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/psx_syscall.h>
#include <sys/syscall.h>

/*
 * psx_load_syscalls() is weakly defined so we can have it overriden
 * by libpsx if it is linked. Specifically, when libcap calls
 * psx_load_sycalls it will override their defaut values. As can be
 * seen here this present function is a no-op. However, if libpsx is
 * linked, the one present in that library (not being weak) will
 * replace this one.
 */
void psx_load_syscalls(long int (**syscall_fn)(long int,
					      long int, long int, long int),
		       long int (**syscall6_fn)(long int,
					       long int, long int, long int,
					       long int, long int, long int))
{
    *syscall_fn = psx_syscall3;
    *syscall6_fn = psx_syscall6;
}

/*
 * type to keep track of registered threads.
 */
typedef struct registered_thread_s {
    struct registered_thread_s *next, *prev;
    pthread_t thread;
    pthread_mutex_t mu;
    int pending;
} registered_thread_t;

static pthread_once_t psx_tracker_initialized = PTHREAD_ONCE_INIT;

typedef enum {
    _PSX_IDLE = 0,
    _PSX_SETUP = 1,
    _PSX_SYSCALL = 2,
    _PSX_CREATE = 3,
    _PSX_INFORK = 4
} psx_tracker_state_t;

/*
 * This global structure holds the global coordination state for
 * libcap's psx_posix_syscall() support.
 */
static struct psx_tracker_s {
    int has_forked;

    pthread_mutex_t state_mu;
    pthread_cond_t cond; /* this is only used to wait on 'state' changes */
    psx_tracker_state_t state;
    int (*creator)(pthread_t *thread, const pthread_attr_t *attr,
		   void *(*start_routine) (void *), void *arg);
    int initialized;
    int psx_sig;

    struct {
	long syscall_nr;
	long arg1, arg2, arg3, arg4, arg5, arg6;
	int six;
	int active;
    } cmd;

    struct sigaction sig_action;
    registered_thread_t *root;
} psx_tracker;

/*
 * psx_action_key is used for thread local storage of the thread's
 * registration. */
pthread_key_t psx_action_key;

/*
 * psx_do_registration called locked and creates a tracker entry for
 * the current thread with a TLS specific key pointing at the threads
 * specific tracker.
 */
static void psx_do_registration(void) {
    registered_thread_t *node = calloc(1, sizeof(registered_thread_t));
    pthread_mutex_init(&node->mu, NULL);
    node->thread = pthread_self();
    pthread_setspecific(psx_action_key, node);
    node->next = psx_tracker.root;
    if (node->next) {
	node->next->prev = node;
    }
    psx_tracker.root = node;
}

/*
 * psx_posix_syscall_actor performs the system call on the targeted
 * thread and signals it is no longer pending.
 */
static void psx_posix_syscall_actor(int signum, siginfo_t *info, void *ignore) {
    /* bail early if this isn't something we recognize */
    if (signum != psx_tracker.psx_sig || !psx_tracker.cmd.active ||
	info == NULL || info->si_code != SI_TKILL || info->si_pid != getpid()) {
	return;
    }

    if (!psx_tracker.cmd.six) {
	(void) syscall(psx_tracker.cmd.syscall_nr,
		       psx_tracker.cmd.arg1,
		       psx_tracker.cmd.arg2,
		       psx_tracker.cmd.arg3);
    } else {
	(void) syscall(psx_tracker.cmd.syscall_nr,
		       psx_tracker.cmd.arg1,
		       psx_tracker.cmd.arg2,
		       psx_tracker.cmd.arg3,
		       psx_tracker.cmd.arg4,
		       psx_tracker.cmd.arg5,
		       psx_tracker.cmd.arg6);
    }

    /*
     * This handler can only be called on registered threads which
     * have had this specific defined at start-up. (But see the
     * subsequent test.)
     */
    registered_thread_t *ref = pthread_getspecific(psx_action_key);
    if (ref) {
	pthread_mutex_lock(&ref->mu);
	ref->pending = 0;
	pthread_mutex_unlock(&ref->mu);
    } /*
       * else thread must be dying and its psx_action_key has already
       * been cleaned up.
       */
}

/*
 * Some forward declarations for the initialization
 * psx_syscall_start() routine.
 */
static void _psx_prepare_fork(void);
static void _psx_fork_completed(void);
static void _psx_forked_child(void);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
			  void *(*start_routine) (void *), void *arg);

/*
 * psx_syscall_start initializes the subsystem including initializing
 * the mutex.
 */
static void psx_syscall_start(void) {
    pthread_mutex_init(&psx_tracker.state_mu, NULL);
    pthread_cond_init(&psx_tracker.cond, NULL);
    pthread_key_create(&psx_action_key, NULL);
    psx_tracker.creator = (pthread_create == __wrap_pthread_create ?
			   __real_pthread_create : pthread_create);
    pthread_atfork(_psx_prepare_fork, _psx_fork_completed, _psx_forked_child);

    /*
     * glibc nptl picks from the SIGRTMIN end, so we pick from the
     * SIGRTMAX end
     */
    psx_tracker.psx_sig = SIGRTMAX;
    psx_tracker.sig_action.sa_sigaction = psx_posix_syscall_actor;
    sigemptyset(&psx_tracker.sig_action.sa_mask);
    psx_tracker.sig_action.sa_flags = SA_SIGINFO | SA_RESTART;;
    sigaction(psx_tracker.psx_sig, &psx_tracker.sig_action, NULL);

    psx_do_registration(); // register the main thread.

    psx_tracker.initialized = 1;
}

/*
 * This is the only way this library globally locks. Note, this is not
 * to be confused with psx_sig (interrupt) blocking - which is
 * performed around thread creation.
 */
static void psx_lock(void)
{
    pthread_once(&psx_tracker_initialized, psx_syscall_start);
    pthread_mutex_lock(&psx_tracker.state_mu);
}

/*
 * This is the only way this library unlocks.
 */
static void psx_unlock(void)
{
    pthread_mutex_unlock(&psx_tracker.state_mu);
}

/*
 * under lock perform a state transition.
 */
static void psx_new_state(psx_tracker_state_t was, psx_tracker_state_t is)
{
    psx_lock();
    while (psx_tracker.state != was) {
	pthread_cond_wait(&psx_tracker.cond, &psx_tracker.state_mu);
    }
    psx_tracker.state = is;
    if (is == _PSX_IDLE) {
	/* only announce newly idle states since that is all we wait for */
	pthread_cond_signal(&psx_tracker.cond);
    }
    psx_unlock();
}

long int psx_syscall3(long int syscall_nr,
		      long int arg1, long int arg2, long int arg3) {
    return psx_syscall(syscall_nr, arg1, arg2, arg3);
}

long int psx_syscall6(long int syscall_nr,
		      long int arg1, long int arg2, long int arg3,
		      long int arg4, long int arg5, long int arg6) {
    return psx_syscall(syscall_nr, arg1, arg2, arg3, arg4, arg5, arg6);
}

static void _psx_prepare_fork(void) {
    /*
     * obtain global lock - we don't want any syscalls while the fork
     * is occurring since it may interfere with the preparation for
     * the fork.
     */
    psx_new_state(_PSX_IDLE, _PSX_INFORK);
}

static void _psx_fork_completed(void) {
    /*
     * The only way we can get here is if state is _PSX_INFORK and was
     * previously _PSX_IDLE. Now that the fork has completed, the
     * parent can continue as if it hadn't happened - the forked child
     * does not tie its security state to that of the parent process
     * and threads.
     *
     * We don't strictly need to change the psx_tracker.state since we
     * hold the mutex over the fork, but we do to make deadlock
     * debugging easier.
     */
    psx_new_state(_PSX_INFORK, _PSX_IDLE);
}

static void _psx_forked_child(void) {
    /*
     * The only way we can get here is if state is _PSX_INFORK and was
     * previously _PSX_IDLE. However, none of the registered threads
     * exist in this newly minted child process, so we have to reset
     * the tracking structure to avoid any confusion. We also skuttle
     * any chance of the PSX API working on more than one thread in
     * the child by leaving the state as _PSX_INFORK. We do support
     * all psx_syscall()s by reverting to them being direct in the
     * fork()ed child.
     *
     * We do this because the glibc man page for fork() suggests that
     * only a subset of things will work post fork(). Specifically,
     * only a "async-signal-safe functions (see signal- safety(7))
     * until such time as it calls execve(2)" can be relied upon. That
     * man page suggests that you can't expect mutexes to work: "not
     * async-signal-safe because it uses pthread_mutex_lock(3)
     * internally.".
     */
    registered_thread_t *next, *old_root;
    old_root = psx_tracker.root;
    psx_tracker.root = NULL;

    psx_tracker.has_forked = 1;

    for (; old_root; old_root = next) {
	next = old_root->next;
	memset(old_root, 0, sizeof(*old_root));
	free(old_root);
    }
}

/*
 * called locked to unregister a node from the tracker.
 */
static void psx_do_unregister(registered_thread_t *node) {
    if (psx_tracker.root == node) {
	psx_tracker.root = node->next;
    }
    if (node->next) {
	node->next->prev = node->prev;
    }
    if (node->prev) {
	node->prev->next = node->next;
    }
    pthread_mutex_destroy(&node->mu);
    memset(node, 0, sizeof(*node));
    free(node);
}

/*
 * psx_register can be used to explicitly register a thread once
 * created. In general, it shouldn't be needed. Further, it should
 * never be used to register the main thread.
 */
void psx_register(void) {
    psx_lock();
    psx_do_registration();
    psx_unlock();
}

typedef struct {
    void *(*fn)(void *);
    void *arg;
    sigset_t sigbits;
} psx_starter_t;

/*
 * _psx_start_fn is a trampolene for the intended start function, it
 * is called both blocked and locked, but releases the (b)lock before
 * calling starter->fn. Before releasing the (b)lock the TLS specific
 * attribute(s) are initialized for use by the interrupt handler under
 * the psx mutex, so it doesn't race with an interrupt received by
 * this thread and the interrupt handler does not need to poll for
 * that specific attribute to be present (which is problematic during
 * thread shutdown).
 */
static void *_psx_start_fn(void *data) {
    psx_do_registration();
    psx_new_state(_PSX_CREATE, _PSX_IDLE);

    psx_starter_t *starter = data;
    pthread_sigmask(SIG_SETMASK, &starter->sigbits, NULL);

    void *(*fn)(void *) = starter->fn;
    void *arg = starter->arg;
    memset(data, 0, sizeof(*starter));
    free(data);

    return fn(arg);
}

/*
 * __wrap_pthread_create is the wrapped destination of all regular
 * pthread_create calls.
 */
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
			  void *(*start_routine) (void *), void *arg) {
    psx_starter_t *starter = calloc(1, sizeof(psx_starter_t));
    starter->fn = start_routine;
    starter->arg = arg;
    /*
     * Until we are in the _PSX_IDLE state and locked, we must not
     * block the psx_sig interrupt for this parent thread. Arrange
     * that parent thread and newly created one can restore signal
     * mask.
     */
    sigset_t sigbit, orig_sigbits;
    sigemptyset(&sigbit);
    pthread_sigmask(SIG_UNBLOCK, &sigbit, &starter->sigbits);
    sigaddset(&sigbit, psx_tracker.psx_sig);
    pthread_sigmask(SIG_UNBLOCK, &sigbit, &orig_sigbits);

    psx_new_state(_PSX_IDLE, _PSX_CREATE);

    /*
     * until the child thread has been blessed with its own TLS
     * specific attribute(s) we prevent either the parent thread or
     * the new one from experiencing a PSX interrupt.
     */
    pthread_sigmask(SIG_BLOCK, &sigbit, NULL);

    int ret = psx_tracker.creator(thread, attr, _psx_start_fn, starter);
    if (ret == -1) {
	psx_new_state(_PSX_CREATE, _PSX_IDLE);
	memset(starter, 0, sizeof(*starter));
	free(starter);
    } /* else unlock happens in _psx_start_fn */

    /* the parent can once again receive psx interrupt signals */
    pthread_sigmask(SIG_SETMASK, &orig_sigbits, NULL);

    return ret;
}

/*
 * psx_pthread_create is a wrapper for pthread_create() that registers
 * the newly created thread. If your threads are created already, they
 * can be individually registered with psx_register().
 */
int psx_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
		       void *(*start_routine) (void *), void *arg) {
    return __wrap_pthread_create(thread, attr, start_routine, arg);
}

/*
 * __psx_immediate_syscall does one syscall using the current
 * process.
 */
static long int __psx_immediate_syscall(long int syscall_nr,
					int count, long int *arg) {
    psx_tracker.cmd.syscall_nr = syscall_nr;
    psx_tracker.cmd.arg1 = count > 0 ? arg[0] : 0;
    psx_tracker.cmd.arg2 = count > 1 ? arg[1] : 0;
    psx_tracker.cmd.arg3 = count > 2 ? arg[2] : 0;

    if (count > 3) {
	psx_tracker.cmd.six = 1;
	psx_tracker.cmd.arg1 = arg[3];
	psx_tracker.cmd.arg2 = count > 4 ? arg[4] : 0;
	psx_tracker.cmd.arg3 = count > 5 ? arg[5] : 0;
	return syscall(syscall_nr,
		      psx_tracker.cmd.arg1,
		      psx_tracker.cmd.arg2,
		      psx_tracker.cmd.arg3,
		      psx_tracker.cmd.arg4,
		      psx_tracker.cmd.arg5,
		      psx_tracker.cmd.arg6);
    }

    psx_tracker.cmd.six = 0;
    return syscall(syscall_nr, psx_tracker.cmd.arg1,
		   psx_tracker.cmd.arg2, psx_tracker.cmd.arg3);
}

/*
 * __psx_syscall performs the syscall on the current thread and if no
 * error is detected it ensures that the syscall is also performed on
 * all (other) registered threads. The return code is the value for
 * the first invocation. It uses a trick to figure out how many
 * arguments the user has supplied. The other half of the trick is
 * provided by the macro psx_syscall() in the <sys/psx_syscall.h>
 * file. The trick is the 7th optional argument (8th over all) to
 * __psx_syscall is the count of arguments supplied to psx_syscall.
 *
 * User:
 *                       psx_syscall(nr, a, b);
 * Expanded by macro to:
 *                       __psx_syscall(nr, a, b, 6, 5, 4, 3, 2, 1, 0);
 * The eighth arg is now ------------------------------------^
 */
long int __psx_syscall(long int syscall_nr, ...) {
    long int arg[7];
    int i;

    va_list aptr;
    va_start(aptr, syscall_nr);
    for (i = 0; i < 7; i++) {
	arg[i] = va_arg(aptr, long int);
    }
    va_end(aptr);

    int count = arg[6];
    if (count < 0 || count > 6) {
	errno = EINVAL;
	return -1;
    }

    if (psx_tracker.has_forked) {
	return __psx_immediate_syscall(syscall_nr, count, arg);
    }

    psx_new_state(_PSX_IDLE, _PSX_SETUP);

    long int ret;

    ret = __psx_immediate_syscall(syscall_nr, count, arg);;
    if (ret == -1 || !psx_tracker.initialized) {
	psx_new_state(_PSX_SETUP, _PSX_IDLE);
	goto defer;
    }

    int restore_errno = errno;

    psx_new_state(_PSX_SETUP, _PSX_SYSCALL);
    psx_tracker.cmd.active = 1;

    pthread_t self = pthread_self();
    registered_thread_t *next = NULL, *ref;

    psx_lock();
    for (ref = psx_tracker.root; ref; ref = next) {
	next = ref->next;
	if (ref->thread == self) {
	    continue;
	}
	pthread_mutex_lock(&ref->mu);
	ref->pending = 1;
	pthread_mutex_unlock(&ref->mu);
	if (pthread_kill(ref->thread, psx_tracker.psx_sig) == 0) {
	    continue;
	}
	/*
	 * need to remove invalid thread id from linked list
	 */
	psx_do_unregister(ref);
    }
    psx_unlock();

    for (;;) {
	int waiting = 0;
	psx_lock();
	for (ref = psx_tracker.root; ref; ref = next) {
	    next = ref->next;
	    if (ref->thread == self) {
		continue;
	    }

	    pthread_mutex_lock(&ref->mu);
	    int pending = ref->pending;
	    pthread_mutex_unlock(&ref->mu);
	    if (!pending || pthread_kill(ref->thread, 0) == 0) {
		waiting += pending;
		continue;
	    }
	    /*
	     * need to remove invalid thread id from linked list
	     */
	    psx_do_unregister(ref);
	}
	psx_unlock();
	if (!waiting) {
	    break;
	}
	sched_yield();
    }

    errno = restore_errno;
    psx_tracker.cmd.active = 0;
    psx_new_state(_PSX_SYSCALL, _PSX_IDLE);

defer:
    return ret;
}
