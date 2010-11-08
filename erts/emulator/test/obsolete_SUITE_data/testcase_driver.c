/* ``The contents of this file are subject to the Erlang Public License,
 * Version 1.1, (the "License"); you may not use this file except in
 * compliance with the License. You should have received a copy of the
 * Erlang Public License along with this software. If not, it can be
 * retrieved via the world wide web at http://www.erlang.org/.
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Initial Developer of the Original Code is Ericsson Utvecklings AB.
 * Portions created by Ericsson are Copyright 1999, Ericsson Utvecklings
 * AB. All Rights Reserved.''
 * 
 *     $Id$
 */

#include "testcase_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>

#ifdef __WIN32__
#undef HAVE_VSNPRINTF
#define HAVE_VSNPRINTF 1
#define vsnprintf _vsnprintf
#endif

#ifndef HAVE_VSNPRINTF
#define HAVE_VSNPRINTF 0
#endif

#define COMMENT_BUF_SZ 4096

#define TESTCASE_FAILED		0
#define TESTCASE_SKIPPED	1
#define TESTCASE_SUCCEEDED	2

typedef struct {
    TestCaseState_t visible;
    int port;
    int result;
    jmp_buf done_jmp_buf;
    char *comment;
    char comment_buf[COMMENT_BUF_SZ];
} InternalTestCaseState_t;

long testcase_drv_start(int port, char *command);
int testcase_drv_stop(long drv_data);
int testcase_drv_run(long drv_data, char *buf, int len);

static DriverEntry testcase_drv_entry = { 
    NULL,
    testcase_drv_start,
    testcase_drv_stop,
    testcase_drv_run
};


int DRIVER_INIT(testcase_drv)(void *arg)
{
    testcase_drv_entry.driver_name = testcase_name();
    return (int) &testcase_drv_entry;
}

long
testcase_drv_start(int port, char *command)
{
    InternalTestCaseState_t *itcs = (InternalTestCaseState_t *)
	driver_alloc(sizeof(InternalTestCaseState_t));
    if (!itcs) {
	return -1;
    }

    itcs->visible.testcase_name = testcase_name();
    itcs->visible.extra = NULL;
    itcs->port = port;
    itcs->result = TESTCASE_FAILED;
    itcs->comment = "";

    return (long) itcs;
}

int
testcase_drv_stop(long drv_data)
{
    testcase_cleanup((TestCaseState_t *) drv_data);
    driver_free((void *) drv_data);
    return 0;
}

int
testcase_drv_run(long drv_data, char *buf, int len)
{
    InternalTestCaseState_t *itcs = (InternalTestCaseState_t *) drv_data;
    DriverTermData result_atom;
    DriverTermData msg[12];

    itcs->visible.command = buf;
    itcs->visible.command_len = len;

    if (setjmp(itcs->done_jmp_buf) == 0) {
	testcase_run((TestCaseState_t *) itcs);
	itcs->result = TESTCASE_SUCCEEDED;
    }

    switch (itcs->result) {
    case TESTCASE_SUCCEEDED:
	result_atom = driver_mk_atom("succeeded");
	break;
    case TESTCASE_SKIPPED:
	result_atom = driver_mk_atom("skipped");
	break;
    case TESTCASE_FAILED:
    default:
	result_atom = driver_mk_atom("failed");
	break;
    }

    msg[0] = ERL_DRV_ATOM;
    msg[1] = (DriverTermData) result_atom;

    msg[2] = ERL_DRV_PORT;
    msg[3] = driver_mk_port(itcs->port);

    msg[4] = ERL_DRV_ATOM;
    msg[5] = driver_mk_atom(itcs->visible.testcase_name);
 
    msg[6] = ERL_DRV_STRING;
    msg[7] = (DriverTermData) itcs->comment;
    msg[8] = (DriverTermData) strlen(itcs->comment);

    msg[9] = ERL_DRV_TUPLE;
    msg[10] = (DriverTermData) 4;

    driver_output_term(itcs->port, msg, 11);
    return 0;
}

int
testcase_assertion_failed(TestCaseState_t *tcs,
			  char *file, int line, char *assertion)
{
    testcase_failed(tcs, "%s:%d: Assertion failed: \"%s\"",
		    file, line, assertion);
    return 0;
}

void
testcase_printf(TestCaseState_t *tcs, char *frmt, ...)
{
    InternalTestCaseState_t *itcs = (InternalTestCaseState_t *) tcs;
    DriverTermData msg[12];
    va_list va;
    va_start(va, frmt);
#if HAVE_VSNPRINTF
    vsnprintf(itcs->comment_buf, COMMENT_BUF_SZ, frmt, va);
#else
    vsprintf(itcs->comment_buf, frmt, va);
#endif
    va_end(va);

    msg[0] = ERL_DRV_ATOM;
    msg[1] = (DriverTermData) driver_mk_atom("print");

    msg[2] = ERL_DRV_PORT;
    msg[3] = driver_mk_port(itcs->port);

    msg[4] = ERL_DRV_ATOM;
    msg[5] = driver_mk_atom(itcs->visible.testcase_name);

    msg[6] = ERL_DRV_STRING;
    msg[7] = (DriverTermData) itcs->comment_buf;
    msg[8] = (DriverTermData) strlen(itcs->comment_buf);

    msg[9] = ERL_DRV_TUPLE;
    msg[10] = (DriverTermData) 4;

    driver_output_term(itcs->port, msg, 11);
}


void testcase_succeeded(TestCaseState_t *tcs, char *frmt, ...)
{
    InternalTestCaseState_t *itcs = (InternalTestCaseState_t *) tcs;
    va_list va;
    va_start(va, frmt);
#if HAVE_VSNPRINTF
    vsnprintf(itcs->comment_buf, COMMENT_BUF_SZ, frmt, va);
#else
    vsprintf(itcs->comment_buf, frmt, va);
#endif
    va_end(va);

    itcs->result = TESTCASE_SUCCEEDED;
    itcs->comment = itcs->comment_buf;

    longjmp(itcs->done_jmp_buf, 1);
}

void testcase_skipped(TestCaseState_t *tcs, char *frmt, ...)
{
    InternalTestCaseState_t *itcs = (InternalTestCaseState_t *) tcs;
    va_list va;
    va_start(va, frmt);
#if HAVE_VSNPRINTF
    vsnprintf(itcs->comment_buf, COMMENT_BUF_SZ, frmt, va);
#else
    vsprintf(itcs->comment_buf, frmt, va);
#endif
    va_end(va);

    itcs->result = TESTCASE_SKIPPED;
    itcs->comment = itcs->comment_buf;

    longjmp(itcs->done_jmp_buf, 1);
}

void testcase_failed(TestCaseState_t *tcs, char *frmt, ...)
{
    InternalTestCaseState_t *itcs = (InternalTestCaseState_t *) tcs;
    char buf[10];
    size_t bufsz = sizeof(buf);
    va_list va;
    va_start(va, frmt);
#if HAVE_VSNPRINTF
    vsnprintf(itcs->comment_buf, COMMENT_BUF_SZ, frmt, va);
#else
    vsprintf(itcs->comment_buf, frmt, va);
#endif
    va_end(va);

    itcs->result = TESTCASE_FAILED;
    itcs->comment = itcs->comment_buf;

    if (erl_drv_getenv("ERL_ABORT_ON_FAILURE", buf, &bufsz) == 0
	&& strcmp("true", buf) == 0) {
	fprintf(stderr, "Testcase \"%s\" failed: %s\n",
		itcs->visible.testcase_name, itcs->comment);
	abort();
    }

    longjmp(itcs->done_jmp_buf, 1);
}

void *testcase_alloc(size_t size)
{
    return driver_alloc(size);
}

void *testcase_realloc(void *ptr, size_t size)
{
    return driver_realloc(ptr, size);
}

void testcase_free(void *ptr)
{
    driver_free(ptr);
}
