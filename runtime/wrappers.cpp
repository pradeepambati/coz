#include <pthread.h>
#include <signal.h>

#include "real.h"
#include "profiler.h"

/// Type of a thread entry function
typedef void* (*thread_fn_t)(void*);

/**
 * Struct to call the real thread function and pass the given argument
 */
struct thread_wrapper {
private:
  thread_fn_t _fn;
  void* _arg;
public:
  thread_wrapper(thread_fn_t fn, void* arg) : _fn(fn), _arg(arg) {}
  void* run() { return _fn(_arg); }
};

/**
 * Prevent profiled applications from registering a handler for the profiler's sampling signal 
 */
extern "C" sighandler_t signal(int signum, sighandler_t handler) {
  if(signum == CycleSampleSignal) {
    return NULL;
  } else {
    return Real::signal()(signum, handler);
  }
}

/**
 * Prevent profiled applications from registering a handler for the profiler's sampling signal
 */
extern "C" int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact) {
  if(signum == CycleSampleSignal) {
    return 0;
  } else if(act != NULL && sigismember(&act->sa_mask, CycleSampleSignal)) {
    struct sigaction my_act = *act;
    sigdelset(&my_act.sa_mask, CycleSampleSignal);
    return Real::sigaction()(signum, &my_act, oldact);
  } else {
    return Real::sigaction()(signum, act, oldact);
  }
}

/**
 * Intercept calls to sigprocmask to ensure the sampling signal is left unmasked
 */
extern "C" int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  if(how == SIG_BLOCK || how == SIG_SETMASK) {
    if(set != NULL && sigismember(set, CycleSampleSignal)) {
      sigset_t myset = *set;
      sigdelset(&myset, CycleSampleSignal);
      return Real::sigprocmask()(how, &myset, oldset);
    }
  }
  
  return Real::sigprocmask()(how, set, oldset);
}

/**
 * Entry point for all new threads
 */
void* thread_entry(void* arg) {
  // Copy the wrapped thread function and argument
  thread_wrapper* wrapper = (thread_wrapper*)arg;
  thread_wrapper local_wrapper = *wrapper;
  // Delete the allocated wrapper object
  delete wrapper;
  // Register this thread with causal
  threadInit();
  // Run the real thread function
  void* result = local_wrapper.run();
  // Exit
  pthread_exit(result);
}

/**
 * Intercept calls to create threads
 */
extern "C" int pthread_create(pthread_t* thread, const pthread_attr_t* attr, thread_fn_t fn, void* arg) {
  void* arg_wrapper = (void*)new thread_wrapper(fn, arg);
  int result = Real::pthread_create()(thread, attr, thread_entry, arg_wrapper);
  return result;
}

/**
 * Intercept all thread exits
 */
extern "C" void pthread_exit(void* result) {
	threadShutdown();
	Real::pthread_exit()(result);
}

/**
 * Intercept calls to exit() to ensure shutdown() is run first
 */
extern "C" void exit(int status) {
  profilerShutdown();
  Real::exit()(status);
}

/**
 * Intercept calls to _exit() to ensure shutdown() is run first
 */
extern "C" void _exit(int status) {
  profilerShutdown();
	Real::_exit()(status);
}

/**
 * Intercept calls to _Exit() to ensure shutdown() is run first
 */
extern "C" void _Exit(int status) {
  profilerShutdown();
  Real::_Exit()(status);
}
