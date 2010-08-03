/*
 * Copyright (C) 2010 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void __attribute__((noreturn))
panic(const char* func, const char* format, ...)
{
    va_list  args;
    fprintf(stderr, "%s: ", func);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

static sem_t tm_sem;

#define  PANIC(...)   panic(__FUNCTION__,__VA_ARGS__)

static void __attribute__((noreturn))
error(int  errcode, const char* func, const char* format, ...)
{
    va_list  args;
    fprintf(stderr, "%s: ", func);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, " error=%d: %s\n", errcode, strerror(errcode));
    exit(1);
}

#define  ERROR(errcode,...)   error((errcode),__FUNCTION__,__VA_ARGS__)

#define  TZERO(cond)   \
    { int _ret = (cond); if (_ret != 0) ERROR(_ret,"%d:%s", __LINE__, #cond); }

#define  TEXPECT_INT(cond,val) \
    { int _ret = (cond); if (_ret != (val)) PANIC("%d:%s returned %d (%d expected)", __LINE__, #cond, _ret, (val)); }

#define  TTRUE(cond)   \
    { if (!(cond)) PANIC("%d:%s", __LINE__, #cond); }


typedef struct {
    pthread_mutex_t  mutex[1];
    double waitDelay;
} TState;

static void
time_sleep(double  delay)
{
    struct timespec ts;
    int             ret;

    ts.tv_sec  = (time_t)delay;
    ts.tv_nsec = (long)((delay - ts.tv_sec)*1e9);

    do {
        ret = nanosleep(&ts, &ts);
    } while (ret < 0 && errno == EINTR);
}

/* return current time in seconds as floating point value */
static double
time_now(void)
{
    struct timespec ts[1];

    clock_gettime(CLOCK_MONOTONIC, ts);
    return (double)ts->tv_sec + ts->tv_nsec/1e9;
}

static void set_mutexattr_type(pthread_mutexattr_t *attr, int type)
{
    int  newtype;
    TZERO(pthread_mutexattr_settype(attr, type));
    newtype = ~type;
    TZERO(pthread_mutexattr_gettype(attr, &newtype));
    TEXPECT_INT(newtype,type);
}

static void do_test_timedlock_1(pthread_mutexattr_t *attr)
{
    struct timespec abstime;

    pthread_mutex_t lock;

    TZERO(pthread_mutex_init(&lock, attr));
    /* add 2 secs to current time */
    clock_gettime(CLOCK_MONOTONIC, &abstime);
    abstime.tv_sec+=2;
    /* Lock a unlocked mutex using timedlock */
    TZERO(pthread_mutex_timedlock(&lock,&abstime));

    TZERO(pthread_mutex_unlock(&lock));
    TZERO(pthread_mutex_destroy(&lock));
 
}

static void do_test_timedlock_2(pthread_mutexattr_t *attr)
{
    struct timespec abstime;

    pthread_mutex_t lock;

    TZERO(pthread_mutex_init(&lock, attr));
    TZERO(pthread_mutex_lock(&lock));

    /* add 2 secs to current time */
    clock_gettime(CLOCK_MONOTONIC, &abstime);
    abstime.tv_sec+=2;

    /* Lock an already locked mutex */
 
    TEXPECT_INT(pthread_mutex_timedlock(&lock, &abstime ),ETIMEDOUT);

    TZERO(pthread_mutex_unlock(&lock));
    TZERO(pthread_mutex_destroy(&lock));
}

static void do_test_timedlock_rec(pthread_mutexattr_t *attr)
{
    struct timespec abstime;

    pthread_mutex_t lock;

    TZERO(pthread_mutex_init(&lock, attr));
    TZERO(pthread_mutex_lock(&lock));

    /* add 2 secs to current time */
    clock_gettime(CLOCK_MONOTONIC, &abstime);
    abstime.tv_sec+=2;

    /* Lock an already recursive locked mutex */
 
    TZERO(pthread_mutex_timedlock(&lock, &abstime ));

    TZERO(pthread_mutex_unlock(&lock));
    TZERO(pthread_mutex_unlock(&lock));
    TZERO(pthread_mutex_destroy(&lock));
}

static void do_test_timedlock_chk(pthread_mutexattr_t *attr)
{
    struct timespec abstime;
    pthread_mutex_t lock;

    TZERO(pthread_mutex_init(&lock, attr));
    TZERO(pthread_mutex_lock(&lock));

    /* add 2 secs to current time */
    clock_gettime(CLOCK_MONOTONIC, &abstime);
    abstime.tv_sec+=2;

    /* Lock an already recursive locked mutex */
 
    TEXPECT_INT(pthread_mutex_timedlock(&lock, &abstime ),EDEADLK);

    TZERO(pthread_mutex_unlock(&lock));
    TZERO(pthread_mutex_destroy(&lock));
}

static void* do_lock_for_seconds(void* arg)
{
    TState *s=arg;
    double t1;

    TZERO(pthread_mutex_trylock(s->mutex));

    sem_post(&tm_sem);

    time_sleep(s->waitDelay);
  
    t1 = time_now();

    TZERO(pthread_mutex_unlock(s->mutex));
    return (NULL);
}

static void do_test_threaded_lock(pthread_mutexattr_t *attr)
{
    TState s[1];
    void* dummy;
    pthread_t th;
    s->waitDelay=2;
    TZERO(pthread_mutex_init(s->mutex, attr));

    pthread_create(&th, NULL, do_lock_for_seconds, s);

    struct timespec abstime;

    clock_gettime(CLOCK_MONOTONIC, &abstime);

    abstime.tv_sec+=s->waitDelay*2;

    sem_wait(&tm_sem);

    TZERO(pthread_mutex_timedlock(s->mutex, &abstime));

    TZERO(pthread_mutex_unlock(s->mutex));
    TZERO(pthread_mutex_destroy(s->mutex));

    TZERO(pthread_join(th, &dummy));
}

static void test_MutexTimeout(int mutexType, int isShared)
{

    pthread_mutexattr_t attr[1];

    TZERO(pthread_mutexattr_init(attr));

    set_mutexattr_type(attr, mutexType);

    if  (isShared)
        TZERO(pthread_mutexattr_setpshared(attr, PTHREAD_PROCESS_SHARED));

    printf("   - Test: Timedlock 1\n");
    do_test_timedlock_1(attr);

    printf("   - Test: Threaded timedlock\n");
    do_test_threaded_lock(attr); 

    switch(mutexType)
    {
        case PTHREAD_MUTEX_NORMAL:
            do_test_timedlock_2(attr);
            printf("   - Test: Timed lock 2\n");
            break;

        case PTHREAD_MUTEX_RECURSIVE:
            printf("   - Test: Recursive lock\n");
            do_test_timedlock_rec(attr);
            break;

        case PTHREAD_MUTEX_ERRORCHECK:
            printf("   - Test: Errorcheck lock\n");
            do_test_timedlock_chk(attr);
            break;
    }
}


int main(){


    sem_init(&tm_sem, 0, 0);

    /* non-shared mutex */
    printf("Running non-shared mutex tests: \n");
    printf(" - PTHREAD_MUTEX_NORMAL\n");
    test_MutexTimeout(PTHREAD_MUTEX_NORMAL,0);
    printf(" - PTHREAD_MUTEX_RECURSIVE\n");
    test_MutexTimeout(PTHREAD_MUTEX_RECURSIVE,0);
    printf(" - PTHREAD_MUTEX_ERRORCHECK\n");
    test_MutexTimeout(PTHREAD_MUTEX_ERRORCHECK,0);
    /* shared mutex */
    printf("\nRunning shared mutex tests: \n");
    printf(" - PTHREAD_MUTEX_NORMAL\n");
    test_MutexTimeout(PTHREAD_MUTEX_NORMAL,1);
    printf(" - PTHREAD_MUTEX_RECURSIVE\n");
    test_MutexTimeout(PTHREAD_MUTEX_RECURSIVE,1);
    printf(" - PTHREAD_MUTEX_ERRORCHECK\n");
    test_MutexTimeout(PTHREAD_MUTEX_ERRORCHECK,1);
    return 0;
}
