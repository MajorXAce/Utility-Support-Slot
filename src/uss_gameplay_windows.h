/* Concrete monotonic clock/thread for the persistent gameplay loop.
 * Native player/input/UI policy and controller remain explicit required inputs. */
#ifndef USS_GAMEPLAY_WINDOWS_H
#define USS_GAMEPLAY_WINDOWS_H
#include "uss_gameplay_loop.h"
static inline uint64_t uss_gameplay_windows_clock(void* user)
{(void)user;return GetTickCount64();}
static inline unsigned uss_gameplay_windows_thread(void* user)
{(void)user;return GetCurrentThreadId();}
static inline UssGameplayLoopIo uss_gameplay_windows_io(void* user,
    int (*policy)(void*,UssCoreFlags*),
    unsigned (*control)(void*,UssCoreLifecycle*,const UssCoreObservation*,uint64_t))
{
    UssGameplayLoopIo io={user,uss_gameplay_windows_clock,uss_gameplay_windows_thread,policy,control,NULL,NULL,NULL,NULL};return io;
}
#endif
