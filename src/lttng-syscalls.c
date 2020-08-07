/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-syscalls.c
 *
 * LTTng syscall probes.
 *
 * Copyright (C) 2010-2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/err.h>
#include <linux/bitmap.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/seq_file.h>
#include <linux/stringify.h>
#include <linux/file.h>
#include <linux/anon_inodes.h>
#include <linux/fcntl.h>
#include <linux/mman.h>
#include <asm/ptrace.h>
#include <asm/syscall.h>

#include <lttng/bitfield.h>
#include <wrapper/tracepoint.h>
#include <wrapper/file.h>
#include <wrapper/rcu.h>
#include <wrapper/syscall.h>
#include <lttng/events.h>
#include <lttng/utils.h>

#ifndef CONFIG_COMPAT
# ifndef is_compat_task
#  define is_compat_task()	(0)
# endif
#endif

/* in_compat_syscall appears in kernel 4.6. */
#ifndef in_compat_syscall
 #define in_compat_syscall()	is_compat_task()
#endif

enum sc_type {
	SC_TYPE_ENTRY,
	SC_TYPE_EXIT,
	SC_TYPE_COMPAT_ENTRY,
	SC_TYPE_COMPAT_EXIT,
};

#define SYSCALL_ENTRY_TOK		syscall_entry_
#define COMPAT_SYSCALL_ENTRY_TOK	compat_syscall_entry_
#define SYSCALL_EXIT_TOK		syscall_exit_
#define COMPAT_SYSCALL_EXIT_TOK		compat_syscall_exit_

#define SYSCALL_ENTRY_STR		__stringify(SYSCALL_ENTRY_TOK)
#define COMPAT_SYSCALL_ENTRY_STR	__stringify(COMPAT_SYSCALL_ENTRY_TOK)
#define SYSCALL_EXIT_STR		__stringify(SYSCALL_EXIT_TOK)
#define COMPAT_SYSCALL_EXIT_STR		__stringify(COMPAT_SYSCALL_EXIT_TOK)

static
void syscall_entry_event_probe(void *__data, struct pt_regs *regs, long id);
static
void syscall_exit_event_probe(void *__data, struct pt_regs *regs, long ret);

/*
 * Forward declarations for old kernels.
 */
struct mmsghdr;
struct rlimit64;
struct oldold_utsname;
struct old_utsname;
struct sel_arg_struct;
struct mmap_arg_struct;
struct file_handle;
struct user_msghdr;

/*
 * Forward declaration for kernels >= 5.6
 */
struct timex;
struct timeval;
struct itimerval;
struct itimerspec;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
typedef __kernel_old_time_t time_t;
#endif

#ifdef IA32_NR_syscalls
#define NR_compat_syscalls IA32_NR_syscalls
#else
#define NR_compat_syscalls NR_syscalls
#endif

/*
 * Create LTTng tracepoint probes.
 */
#define LTTNG_PACKAGE_BUILD
#define CREATE_TRACE_POINTS
#define TP_MODULE_NOINIT
#define TRACE_INCLUDE_PATH instrumentation/syscalls/headers

#define PARAMS(args...)	args

/* Handle unknown syscalls */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM syscalls_unknown
#include <instrumentation/syscalls/headers/syscalls_unknown.h>
#undef TRACE_SYSTEM

#define SC_ENTER

#undef sc_exit
#define sc_exit(...)
#undef sc_in
#define sc_in(...)	__VA_ARGS__
#undef sc_out
#define sc_out(...)
#undef sc_inout
#define sc_inout(...)	__VA_ARGS__

/* Hijack probe callback for system call enter */
#undef TP_PROBE_CB
#define TP_PROBE_CB(_template)		&syscall_entry_event_probe
#define SC_LTTNG_TRACEPOINT_EVENT(_name, _proto, _args, _fields) \
	LTTNG_TRACEPOINT_EVENT(syscall_entry_##_name, PARAMS(_proto), PARAMS(_args), \
		PARAMS(_fields))
#define SC_LTTNG_TRACEPOINT_EVENT_CODE(_name, _proto, _args, _locvar, _code_pre, _fields, _code_post) \
	LTTNG_TRACEPOINT_EVENT_CODE(syscall_entry_##_name, PARAMS(_proto), PARAMS(_args), \
		PARAMS(_locvar), PARAMS(_code_pre),				\
		PARAMS(_fields), PARAMS(_code_post))
#define SC_LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS(_name, _fields) \
	LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS(syscall_entry_##_name, PARAMS(_fields))
#define SC_LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS(_template, _name)		\
	LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS(syscall_entry_##_template, syscall_entry_##_name)
/* Enumerations only defined at first inclusion. */
#define SC_LTTNG_TRACEPOINT_ENUM(_name, _values) \
	LTTNG_TRACEPOINT_ENUM(_name, PARAMS(_values))
#undef TRACE_SYSTEM
#define TRACE_SYSTEM syscall_entry_integers
#define TRACE_INCLUDE_FILE syscalls_integers
#include <instrumentation/syscalls/headers/syscalls_integers.h>
#undef TRACE_INCLUDE_FILE
#undef TRACE_SYSTEM
#define TRACE_SYSTEM syscall_entry_pointers
#define TRACE_INCLUDE_FILE syscalls_pointers
#include <instrumentation/syscalls/headers/syscalls_pointers.h>
#undef TRACE_INCLUDE_FILE
#undef TRACE_SYSTEM
#undef SC_LTTNG_TRACEPOINT_ENUM
#undef SC_LTTNG_TRACEPOINT_EVENT_CODE
#undef SC_LTTNG_TRACEPOINT_EVENT
#undef SC_LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS
#undef SC_LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS
#undef TP_PROBE_CB
#undef _TRACE_SYSCALLS_INTEGERS_H
#undef _TRACE_SYSCALLS_POINTERS_H

/* Hijack probe callback for compat system call enter */
#define TP_PROBE_CB(_template)		&syscall_entry_event_probe
#define LTTNG_SC_COMPAT
#define SC_LTTNG_TRACEPOINT_EVENT(_name, _proto, _args, _fields) \
	LTTNG_TRACEPOINT_EVENT(compat_syscall_entry_##_name, PARAMS(_proto), PARAMS(_args), \
		PARAMS(_fields))
#define SC_LTTNG_TRACEPOINT_EVENT_CODE(_name, _proto, _args, _locvar, _code_pre, _fields, _code_post) \
	LTTNG_TRACEPOINT_EVENT_CODE(compat_syscall_entry_##_name, PARAMS(_proto), PARAMS(_args), \
		PARAMS(_locvar), PARAMS(_code_pre), PARAMS(_fields), PARAMS(_code_post))
#define SC_LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS(_name, _fields) \
	LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS(compat_syscall_entry_##_name, PARAMS(_fields))
#define SC_LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS(_template, _name)		\
	LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS(compat_syscall_entry_##_template, \
		compat_syscall_entry_##_name)
/* Enumerations only defined at inital inclusion (not here). */
#define SC_LTTNG_TRACEPOINT_ENUM(_name, _values)
#define TRACE_SYSTEM compat_syscall_entry_integers
#define TRACE_INCLUDE_FILE compat_syscalls_integers
#include <instrumentation/syscalls/headers/compat_syscalls_integers.h>
#undef TRACE_INCLUDE_FILE
#undef TRACE_SYSTEM
#define TRACE_SYSTEM compat_syscall_entry_pointers
#define TRACE_INCLUDE_FILE compat_syscalls_pointers
#include <instrumentation/syscalls/headers/compat_syscalls_pointers.h>
#undef TRACE_INCLUDE_FILE
#undef TRACE_SYSTEM
#undef SC_LTTNG_TRACEPOINT_ENUM
#undef SC_LTTNG_TRACEPOINT_EVENT_CODE
#undef SC_LTTNG_TRACEPOINT_EVENT
#undef SC_LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS
#undef SC_LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS
#undef TP_PROBE_CB
#undef _TRACE_SYSCALLS_INTEGERS_H
#undef _TRACE_SYSCALLS_POINTERS_H
#undef LTTNG_SC_COMPAT

#undef SC_ENTER

#define SC_EXIT

#undef sc_exit
#define sc_exit(...)		__VA_ARGS__
#undef sc_in
#define sc_in(...)
#undef sc_out
#define sc_out(...)		__VA_ARGS__
#undef sc_inout
#define sc_inout(...)		__VA_ARGS__

/* Hijack probe callback for system call exit */
#define TP_PROBE_CB(_template)		&syscall_exit_event_probe
#define SC_LTTNG_TRACEPOINT_EVENT(_name, _proto, _args, _fields) \
	LTTNG_TRACEPOINT_EVENT(syscall_exit_##_name, PARAMS(_proto), PARAMS(_args), \
		PARAMS(_fields))
#define SC_LTTNG_TRACEPOINT_EVENT_CODE(_name, _proto, _args, _locvar, _code_pre, _fields, _code_post) \
	LTTNG_TRACEPOINT_EVENT_CODE(syscall_exit_##_name, PARAMS(_proto), PARAMS(_args), \
		PARAMS(_locvar), PARAMS(_code_pre), PARAMS(_fields), PARAMS(_code_post))
#define SC_LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS(_name, _fields) \
	LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS(syscall_exit_##_name, PARAMS(_fields))
#define SC_LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS(_template, _name) 		\
	LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS(syscall_exit_##_template,	\
		syscall_exit_##_name)
/* Enumerations only defined at inital inclusion (not here). */
#define SC_LTTNG_TRACEPOINT_ENUM(_name, _values)
#define TRACE_SYSTEM syscall_exit_integers
#define TRACE_INCLUDE_FILE syscalls_integers
#include <instrumentation/syscalls/headers/syscalls_integers.h>
#undef TRACE_INCLUDE_FILE
#undef TRACE_SYSTEM
#define TRACE_SYSTEM syscall_exit_pointers
#define TRACE_INCLUDE_FILE syscalls_pointers
#include <instrumentation/syscalls/headers/syscalls_pointers.h>
#undef TRACE_INCLUDE_FILE
#undef TRACE_SYSTEM
#undef SC_LTTNG_TRACEPOINT_ENUM
#undef SC_LTTNG_TRACEPOINT_EVENT_CODE
#undef SC_LTTNG_TRACEPOINT_EVENT
#undef SC_LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS
#undef SC_LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS
#undef TP_PROBE_CB
#undef _TRACE_SYSCALLS_INTEGERS_H
#undef _TRACE_SYSCALLS_POINTERS_H


/* Hijack probe callback for compat system call exit */
#define TP_PROBE_CB(_template)		&syscall_exit_event_probe
#define LTTNG_SC_COMPAT
#define SC_LTTNG_TRACEPOINT_EVENT(_name, _proto, _args, _fields) \
	LTTNG_TRACEPOINT_EVENT(compat_syscall_exit_##_name, PARAMS(_proto), PARAMS(_args), \
		PARAMS(_fields))
#define SC_LTTNG_TRACEPOINT_EVENT_CODE(_name, _proto, _args, _locvar, _code_pre, _fields, _code_post) \
	LTTNG_TRACEPOINT_EVENT_CODE(compat_syscall_exit_##_name, PARAMS(_proto), PARAMS(_args), \
		PARAMS(_locvar), PARAMS(_code_pre), PARAMS(_fields), PARAMS(_code_post))
#define SC_LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS(_name, _fields) \
	LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS(compat_syscall_exit_##_name, PARAMS(_fields))
#define SC_LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS(_template, _name)		\
	LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS(compat_syscall_exit_##_template, \
		compat_syscall_exit_##_name)
/* Enumerations only defined at inital inclusion (not here). */
#define SC_LTTNG_TRACEPOINT_ENUM(_name, _values)
#define TRACE_SYSTEM compat_syscall_exit_integers
#define TRACE_INCLUDE_FILE compat_syscalls_integers
#include <instrumentation/syscalls/headers/compat_syscalls_integers.h>
#undef TRACE_INCLUDE_FILE
#undef TRACE_SYSTEM
#define TRACE_SYSTEM compat_syscall_exit_pointers
#define TRACE_INCLUDE_FILE compat_syscalls_pointers
#include <instrumentation/syscalls/headers/compat_syscalls_pointers.h>
#undef TRACE_INCLUDE_FILE
#undef TRACE_SYSTEM
#undef SC_LTTNG_TRACEPOINT_ENUM
#undef SC_LTTNG_TRACEPOINT_EVENT_CODE
#undef SC_LTTNG_TRACEPOINT_EVENT
#undef SC_LTTNG_TRACEPOINT_EVENT_CLASS_NOARGS
#undef SC_LTTNG_TRACEPOINT_EVENT_INSTANCE_NOARGS
#undef TP_PROBE_CB
#undef _TRACE_SYSCALLS_INTEGERS_H
#undef _TRACE_SYSCALLS_POINTERS_H
#undef LTTNG_SC_COMPAT

#undef SC_EXIT

#undef TP_MODULE_NOINIT
#undef LTTNG_PACKAGE_BUILD
#undef CREATE_TRACE_POINTS

struct trace_syscall_entry {
	void *event_func;
	void *trigger_func;
	const struct lttng_event_desc *desc;
	const struct lttng_event_field *fields;
	unsigned int nrargs;
};

#define CREATE_SYSCALL_TABLE

#define SC_ENTER

#undef sc_exit
#define sc_exit(...)

#undef TRACE_SYSCALL_TABLE
#define TRACE_SYSCALL_TABLE(_template, _name, _nr, _nrargs)	\
	[ _nr ] = {						\
		.event_func = __event_probe__syscall_entry_##_template, \
		.trigger_func = __trigger_probe__syscall_entry_##_template, \
		.nrargs = (_nrargs),				\
		.fields = __event_fields___syscall_entry_##_template, \
		.desc = &__event_desc___syscall_entry_##_name,	\
	},

/* Event syscall enter tracing table */
static const struct trace_syscall_entry sc_table[] = {
#include <instrumentation/syscalls/headers/syscalls_integers.h>
#include <instrumentation/syscalls/headers/syscalls_pointers.h>
};

#undef TRACE_SYSCALL_TABLE
#define TRACE_SYSCALL_TABLE(_template, _name, _nr, _nrargs)	\
	[ _nr ] = {						\
		.event_func = __event_probe__compat_syscall_entry_##_template, \
		.trigger_func = __trigger_probe__compat_syscall_entry_##_template, \
		.nrargs = (_nrargs),				\
		.fields = __event_fields___compat_syscall_entry_##_template, \
		.desc = &__event_desc___compat_syscall_entry_##_name, \
	},

/* Event compat syscall enter table */
const struct trace_syscall_entry compat_sc_table[] = {
#include <instrumentation/syscalls/headers/compat_syscalls_integers.h>
#include <instrumentation/syscalls/headers/compat_syscalls_pointers.h>
};

#undef SC_ENTER

#define SC_EXIT

#undef sc_exit
#define sc_exit(...)		__VA_ARGS__

#undef TRACE_SYSCALL_TABLE
#define TRACE_SYSCALL_TABLE(_template, _name, _nr, _nrargs)	\
	[ _nr ] = {						\
		.event_func = __event_probe__syscall_exit_##_template, \
		.trigger_func = __trigger_probe__syscall_exit_##_template, \
		.nrargs = (_nrargs),				\
		.fields = __event_fields___syscall_exit_##_template, \
		.desc = &__event_desc___syscall_exit_##_name, \
	},

/* Event syscall exit table */
static const struct trace_syscall_entry sc_exit_table[] = {
#include <instrumentation/syscalls/headers/syscalls_integers.h>
#include <instrumentation/syscalls/headers/syscalls_pointers.h>
};

#undef TRACE_SYSCALL_TABLE
#define TRACE_SYSCALL_TABLE(_template, _name, _nr, _nrargs)	\
	[ _nr ] = {						\
		.event_func = __event_probe__compat_syscall_exit_##_template, \
		.trigger_func = __trigger_probe__compat_syscall_exit_##_template, \
		.nrargs = (_nrargs),				\
		.fields = __event_fields___compat_syscall_exit_##_template, \
		.desc = &__event_desc___compat_syscall_exit_##_name, \
	},

/* Event compat syscall exit table */
const struct trace_syscall_entry compat_sc_exit_table[] = {
#include <instrumentation/syscalls/headers/compat_syscalls_integers.h>
#include <instrumentation/syscalls/headers/compat_syscalls_pointers.h>
};

#undef SC_EXIT

#undef CREATE_SYSCALL_TABLE

struct lttng_syscall_filter {
	DECLARE_BITMAP(sc, NR_syscalls);
	DECLARE_BITMAP(sc_compat, NR_compat_syscalls);
};

static void syscall_entry_event_unknown(struct lttng_event *event,
	struct pt_regs *regs, unsigned int id)
{
	unsigned long args[LTTNG_SYSCALL_NR_ARGS];

	lttng_syscall_get_arguments(current, regs, args);
	if (unlikely(in_compat_syscall()))
		__event_probe__compat_syscall_entry_unknown(event, id, args);
	else
		__event_probe__syscall_entry_unknown(event, id, args);
}

static __always_inline
void syscall_entry_call_func(void *func, unsigned int nrargs, void *data,
		struct pt_regs *regs)
{
	switch (nrargs) {
	case 0:
	{
		void (*fptr)(void *__data) = func;

		fptr(data);
		break;
	}
	case 1:
	{
		void (*fptr)(void *__data, unsigned long arg0) = func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(data, args[0]);
		break;
	}
	case 2:
	{
		void (*fptr)(void *__data,
			unsigned long arg0,
			unsigned long arg1) = func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(data, args[0], args[1]);
		break;
	}
	case 3:
	{
		void (*fptr)(void *__data,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2) = func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(data, args[0], args[1], args[2]);
		break;
	}
	case 4:
	{
		void (*fptr)(void *__data,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3) = func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(data, args[0], args[1], args[2], args[3]);
		break;
	}
	case 5:
	{
		void (*fptr)(void *__data,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3,
			unsigned long arg4) = func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(data, args[0], args[1], args[2], args[3], args[4]);
		break;
	}
	case 6:
	{
		void (*fptr)(void *__data,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3,
			unsigned long arg4,
			unsigned long arg5) = func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(data, args[0], args[1], args[2],
			args[3], args[4], args[5]);
		break;
	}
	default:
		break;
	}
}

void syscall_entry_event_probe(void *__data, struct pt_regs *regs, long id)
{
	struct lttng_channel *chan = __data;
	struct lttng_syscall_filter *filter;
	struct lttng_event *event, *unknown_event;
	const struct trace_syscall_entry *table, *entry;
	size_t table_len;

	filter = lttng_rcu_dereference(chan->sc_filter);

	if (unlikely(in_compat_syscall())) {
		if (filter) {
			if (id < 0 || id >= NR_compat_syscalls
				|| !test_bit(id, filter->sc_compat)) {
				/* System call filtered out. */
				return;
			}
		}
		table = compat_sc_table;
		table_len = ARRAY_SIZE(compat_sc_table);
		unknown_event = chan->sc_compat_unknown;
	} else {
		if (filter) {
			if (id < 0 || id >= NR_syscalls
				|| !test_bit(id, filter->sc)) {
				/* System call filtered out. */
				return;
			}
		}
		table = sc_table;
		table_len = ARRAY_SIZE(sc_table);
		unknown_event = chan->sc_unknown;
	}
	if (unlikely(id < 0 || id >= table_len)) {
		syscall_entry_event_unknown(unknown_event, regs, id);
		return;
	}
	if (unlikely(in_compat_syscall()))
		event = chan->compat_sc_table[id];
	else
		event = chan->sc_table[id];
	if (unlikely(!event)) {
		syscall_entry_event_unknown(unknown_event, regs, id);
		return;
	}
	entry = &table[id];
	WARN_ON_ONCE(!entry);

	syscall_entry_call_func(entry->event_func, entry->nrargs, event, regs);
}

void syscall_entry_trigger_probe(void *__data, struct pt_regs *regs, long id)
{
	struct lttng_trigger_group *trigger_group = __data;
	const struct trace_syscall_entry *entry;
	struct list_head *dispatch_list;
	struct lttng_trigger *iter;
	size_t table_len;


	if (unlikely(in_compat_syscall())) {
		table_len = ARRAY_SIZE(compat_sc_table);
		if (unlikely(id < 0 || id >= table_len)) {
			return;
		}
		entry = &compat_sc_table[id];
		dispatch_list = &trigger_group->trigger_compat_syscall_dispatch[id];
	} else {
		table_len = ARRAY_SIZE(sc_table);
		if (unlikely(id < 0 || id >= table_len)) {
			return;
		}
		entry = &sc_table[id];
		dispatch_list = &trigger_group->trigger_syscall_dispatch[id];
	}

	/* TODO handle unknown syscall */

	list_for_each_entry_rcu(iter, dispatch_list, u.syscall.node) {
		BUG_ON(iter->u.syscall.syscall_id != id);
		syscall_entry_call_func(entry->trigger_func, entry->nrargs, iter, regs);
	}
}

static void syscall_exit_event_unknown(struct lttng_event *event,
	struct pt_regs *regs, int id, long ret)
{
	unsigned long args[LTTNG_SYSCALL_NR_ARGS];

	lttng_syscall_get_arguments(current, regs, args);
	if (unlikely(in_compat_syscall()))
		__event_probe__compat_syscall_exit_unknown(event, id, ret,
			args);
	else
		__event_probe__syscall_exit_unknown(event, id, ret, args);
}

void syscall_exit_event_probe(void *__data, struct pt_regs *regs, long ret)
{
	struct lttng_channel *chan = __data;
	struct lttng_syscall_filter *filter;
	struct lttng_event *event, *unknown_event;
	const struct trace_syscall_entry *table, *entry;
	size_t table_len;
	long id;

	filter = lttng_rcu_dereference(chan->sc_filter);

	id = syscall_get_nr(current, regs);
	if (unlikely(in_compat_syscall())) {
		if (filter) {
			if (id < 0 || id >= NR_compat_syscalls
				|| !test_bit(id, filter->sc_compat)) {
				/* System call filtered out. */
				return;
			}
		}
		table = compat_sc_exit_table;
		table_len = ARRAY_SIZE(compat_sc_exit_table);
		unknown_event = chan->compat_sc_exit_unknown;
	} else {
		if (filter) {
			if (id < 0 || id >= NR_syscalls
				|| !test_bit(id, filter->sc)) {
				/* System call filtered out. */
				return;
			}
		}
		table = sc_exit_table;
		table_len = ARRAY_SIZE(sc_exit_table);
		unknown_event = chan->sc_exit_unknown;
	}
	if (unlikely(id < 0 || id >= table_len)) {
		syscall_exit_event_unknown(unknown_event, regs, id, ret);
		return;
	}
	if (unlikely(in_compat_syscall()))
		event = chan->compat_sc_exit_table[id];
	else
		event = chan->sc_exit_table[id];
	if (unlikely(!event)) {
		syscall_exit_event_unknown(unknown_event, regs, id, ret);
		return;
	}
	entry = &table[id];
	WARN_ON_ONCE(!entry);

	switch (entry->nrargs) {
	case 0:
	{
		void (*fptr)(void *__data, long ret) = entry->event_func;

		fptr(event, ret);
		break;
	}
	case 1:
	{
		void (*fptr)(void *__data,
			long ret,
			unsigned long arg0) = entry->event_func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(event, ret, args[0]);
		break;
	}
	case 2:
	{
		void (*fptr)(void *__data,
			long ret,
			unsigned long arg0,
			unsigned long arg1) = entry->event_func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(event, ret, args[0], args[1]);
		break;
	}
	case 3:
	{
		void (*fptr)(void *__data,
			long ret,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2) = entry->event_func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(event, ret, args[0], args[1], args[2]);
		break;
	}
	case 4:
	{
		void (*fptr)(void *__data,
			long ret,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3) = entry->event_func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(event, ret, args[0], args[1], args[2], args[3]);
		break;
	}
	case 5:
	{
		void (*fptr)(void *__data,
			long ret,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3,
			unsigned long arg4) = entry->event_func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(event, ret, args[0], args[1], args[2], args[3], args[4]);
		break;
	}
	case 6:
	{
		void (*fptr)(void *__data,
			long ret,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3,
			unsigned long arg4,
			unsigned long arg5) = entry->event_func;
		unsigned long args[LTTNG_SYSCALL_NR_ARGS];

		lttng_syscall_get_arguments(current, regs, args);
		fptr(event, ret, args[0], args[1], args[2],
			args[3], args[4], args[5]);
		break;
	}
	default:
		break;
	}
}

/*
 * noinline to diminish caller stack size.
 * Should be called with sessions lock held.
 */
static
int fill_event_table(const struct trace_syscall_entry *table, size_t table_len,
	struct lttng_event **chan_table, struct lttng_channel *chan,
	void *filter, enum sc_type type)
{
	const struct lttng_event_desc *desc;
	unsigned int i;

	/* Allocate events for each syscall, insert into table */
	for (i = 0; i < table_len; i++) {
		struct lttng_kernel_event ev;
		desc = table[i].desc;

		if (!desc) {
			/* Unknown syscall */
			continue;
		}
		/*
		 * Skip those already populated by previous failed
		 * register for this channel.
		 */
		if (chan_table[i])
			continue;
		memset(&ev, 0, sizeof(ev));
		switch (type) {
		case SC_TYPE_ENTRY:
			strncpy(ev.name, SYSCALL_ENTRY_STR,
				LTTNG_KERNEL_SYM_NAME_LEN);
			break;
		case SC_TYPE_EXIT:
			strncpy(ev.name, SYSCALL_EXIT_STR,
				LTTNG_KERNEL_SYM_NAME_LEN);
			break;
		case SC_TYPE_COMPAT_ENTRY:
			strncpy(ev.name, COMPAT_SYSCALL_ENTRY_STR,
				LTTNG_KERNEL_SYM_NAME_LEN);
			break;
		case SC_TYPE_COMPAT_EXIT:
			strncpy(ev.name, COMPAT_SYSCALL_EXIT_STR,
				LTTNG_KERNEL_SYM_NAME_LEN);
			break;
		default:
			BUG_ON(1);
			break;
		}
		strncat(ev.name, desc->name,
			LTTNG_KERNEL_SYM_NAME_LEN - strlen(ev.name) - 1);
		ev.name[LTTNG_KERNEL_SYM_NAME_LEN - 1] = '\0';
		ev.instrumentation = LTTNG_KERNEL_SYSCALL;
		chan_table[i] = _lttng_event_create(chan, &ev, filter,
						desc, ev.instrumentation);
		WARN_ON_ONCE(!chan_table[i]);
		if (IS_ERR(chan_table[i])) {
			/*
			 * If something goes wrong in event registration
			 * after the first one, we have no choice but to
			 * leave the previous events in there, until
			 * deleted by session teardown.
			 */
			return PTR_ERR(chan_table[i]);
		}
	}
	return 0;
}

/*
 * Should be called with sessions lock held.
 */
int lttng_syscalls_register_event(struct lttng_channel *chan, void *filter)
{
	struct lttng_kernel_event ev;
	int ret;

	wrapper_vmalloc_sync_mappings();

	if (!chan->sc_table) {
		/* create syscall table mapping syscall to events */
		chan->sc_table = kzalloc(sizeof(struct lttng_event *)
					* ARRAY_SIZE(sc_table), GFP_KERNEL);
		if (!chan->sc_table)
			return -ENOMEM;
	}
	if (!chan->sc_exit_table) {
		/* create syscall table mapping syscall to events */
		chan->sc_exit_table = kzalloc(sizeof(struct lttng_event *)
					* ARRAY_SIZE(sc_exit_table), GFP_KERNEL);
		if (!chan->sc_exit_table)
			return -ENOMEM;
	}


#ifdef CONFIG_COMPAT
	if (!chan->compat_sc_table) {
		/* create syscall table mapping compat syscall to events */
		chan->compat_sc_table = kzalloc(sizeof(struct lttng_event *)
					* ARRAY_SIZE(compat_sc_table), GFP_KERNEL);
		if (!chan->compat_sc_table)
			return -ENOMEM;
	}

	if (!chan->compat_sc_exit_table) {
		/* create syscall table mapping compat syscall to events */
		chan->compat_sc_exit_table = kzalloc(sizeof(struct lttng_event *)
					* ARRAY_SIZE(compat_sc_exit_table), GFP_KERNEL);
		if (!chan->compat_sc_exit_table)
			return -ENOMEM;
	}
#endif
	if (!chan->sc_unknown) {
		const struct lttng_event_desc *desc =
			&__event_desc___syscall_entry_unknown;

		memset(&ev, 0, sizeof(ev));
		strncpy(ev.name, desc->name, LTTNG_KERNEL_SYM_NAME_LEN);
		ev.name[LTTNG_KERNEL_SYM_NAME_LEN - 1] = '\0';
		ev.instrumentation = LTTNG_KERNEL_SYSCALL;
		chan->sc_unknown = _lttng_event_create(chan, &ev, filter,
						desc,
						ev.instrumentation);
		WARN_ON_ONCE(!chan->sc_unknown);
		if (IS_ERR(chan->sc_unknown)) {
			return PTR_ERR(chan->sc_unknown);
		}
	}

	if (!chan->sc_compat_unknown) {
		const struct lttng_event_desc *desc =
			&__event_desc___compat_syscall_entry_unknown;

		memset(&ev, 0, sizeof(ev));
		strncpy(ev.name, desc->name, LTTNG_KERNEL_SYM_NAME_LEN);
		ev.name[LTTNG_KERNEL_SYM_NAME_LEN - 1] = '\0';
		ev.instrumentation = LTTNG_KERNEL_SYSCALL;
		chan->sc_compat_unknown = _lttng_event_create(chan, &ev, filter,
						desc,
						ev.instrumentation);
		WARN_ON_ONCE(!chan->sc_unknown);
		if (IS_ERR(chan->sc_compat_unknown)) {
			return PTR_ERR(chan->sc_compat_unknown);
		}
	}

	if (!chan->compat_sc_exit_unknown) {
		const struct lttng_event_desc *desc =
			&__event_desc___compat_syscall_exit_unknown;

		memset(&ev, 0, sizeof(ev));
		strncpy(ev.name, desc->name, LTTNG_KERNEL_SYM_NAME_LEN);
		ev.name[LTTNG_KERNEL_SYM_NAME_LEN - 1] = '\0';
		ev.instrumentation = LTTNG_KERNEL_SYSCALL;
		chan->compat_sc_exit_unknown = _lttng_event_create(chan, &ev,
						filter, desc,
						ev.instrumentation);
		WARN_ON_ONCE(!chan->compat_sc_exit_unknown);
		if (IS_ERR(chan->compat_sc_exit_unknown)) {
			return PTR_ERR(chan->compat_sc_exit_unknown);
		}
	}

	if (!chan->sc_exit_unknown) {
		const struct lttng_event_desc *desc =
			&__event_desc___syscall_exit_unknown;

		memset(&ev, 0, sizeof(ev));
		strncpy(ev.name, desc->name, LTTNG_KERNEL_SYM_NAME_LEN);
		ev.name[LTTNG_KERNEL_SYM_NAME_LEN - 1] = '\0';
		ev.instrumentation = LTTNG_KERNEL_SYSCALL;
		chan->sc_exit_unknown = _lttng_event_create(chan, &ev, filter,
						desc, ev.instrumentation);
		WARN_ON_ONCE(!chan->sc_exit_unknown);
		if (IS_ERR(chan->sc_exit_unknown)) {
			return PTR_ERR(chan->sc_exit_unknown);
		}
	}

	ret = fill_event_table(sc_table, ARRAY_SIZE(sc_table),
			chan->sc_table, chan, filter, SC_TYPE_ENTRY);
	if (ret)
		return ret;
	ret = fill_event_table(sc_exit_table, ARRAY_SIZE(sc_exit_table),
			chan->sc_exit_table, chan, filter, SC_TYPE_EXIT);
	if (ret)
		return ret;

#ifdef CONFIG_COMPAT
	ret = fill_event_table(compat_sc_table, ARRAY_SIZE(compat_sc_table),
			chan->compat_sc_table, chan, filter,
			SC_TYPE_COMPAT_ENTRY);
	if (ret)
		return ret;
	ret = fill_event_table(compat_sc_exit_table, ARRAY_SIZE(compat_sc_exit_table),
			chan->compat_sc_exit_table, chan, filter,
			SC_TYPE_COMPAT_EXIT);
	if (ret)
		return ret;
#endif
	if (!chan->sys_enter_registered) {
		ret = lttng_wrapper_tracepoint_probe_register("sys_enter",
				(void *) syscall_entry_event_probe, chan);
		if (ret)
			return ret;
		chan->sys_enter_registered = 1;
	}
	/*
	 * We change the name of sys_exit tracepoint due to namespace
	 * conflict with sys_exit syscall entry.
	 */
	if (!chan->sys_exit_registered) {
		ret = lttng_wrapper_tracepoint_probe_register("sys_exit",
				(void *) syscall_exit_event_probe, chan);
		if (ret) {
			WARN_ON_ONCE(lttng_wrapper_tracepoint_probe_unregister("sys_enter",
				(void *) syscall_entry_event_probe, chan));
			return ret;
		}
		chan->sys_exit_registered = 1;
	}
	return ret;
}

/*
 * Should be called with sessions lock held.
 */
int lttng_syscalls_register_trigger(struct lttng_trigger_enabler *trigger_enabler, void *filter)
{
	struct lttng_trigger_group *group = trigger_enabler->group;
	unsigned int i;
	int ret = 0;

	wrapper_vmalloc_sync_mappings();

	if (!group->trigger_syscall_dispatch) {
		group->trigger_syscall_dispatch = kzalloc(sizeof(struct list_head)
					* ARRAY_SIZE(sc_table), GFP_KERNEL);
		if (!group->trigger_syscall_dispatch)
			return -ENOMEM;

		/* Initialize all list_head */
		for (i = 0; i < ARRAY_SIZE(sc_table); i++)
			INIT_LIST_HEAD(&group->trigger_syscall_dispatch[i]);
	}

#ifdef CONFIG_COMPAT
	if (!group->trigger_compat_syscall_dispatch) {
		group->trigger_compat_syscall_dispatch = kzalloc(sizeof(struct list_head)
					* ARRAY_SIZE(compat_sc_table), GFP_KERNEL);
		if (!group->trigger_syscall_dispatch)
			return -ENOMEM;

		/* Initialize all list_head */
		for (i = 0; i < ARRAY_SIZE(compat_sc_table); i++)
			INIT_LIST_HEAD(&group->trigger_compat_syscall_dispatch[i]);
	}
#endif

	if (!group->sys_enter_registered) {
		ret = lttng_wrapper_tracepoint_probe_register("sys_enter",
				(void *) syscall_entry_trigger_probe, group);
		if (ret)
			return ret;
		group->sys_enter_registered = 1;
	}

	return ret;
}

static int create_matching_triggers(struct lttng_trigger_enabler *trigger_enabler,
		void *filter, const struct trace_syscall_entry *table,
		size_t table_len, bool is_compat)
{
	struct lttng_trigger_group *group = trigger_enabler->group;
	const struct lttng_event_desc *desc;
	uint64_t id = trigger_enabler->id;
	uint64_t error_counter_index = trigger_enabler->error_counter_index;
	unsigned int i;
	int ret = 0;

	/* iterate over all syscall and create trigger that match */
	for (i = 0; i < table_len; i++) {
		struct lttng_trigger *trigger;
		struct lttng_kernel_trigger trigger_param;
		struct hlist_head *head;
		int found = 0;

		desc = table[i].desc;
		if (!desc) {
			/* Unknown syscall */
			continue;
		}

		if (!lttng_desc_match_enabler(desc,
				lttng_trigger_enabler_as_enabler(trigger_enabler)))
			continue;

		/*
		 * Check if already created.
		 */
		head = utils_borrow_hash_table_bucket(group->triggers_ht.table,
			LTTNG_TRIGGER_HT_SIZE, desc->name);
		lttng_hlist_for_each_entry(trigger, head, hlist) {
			if (trigger->desc == desc
				&& trigger->id == trigger_enabler->id)
				found = 1;
		}
		if (found)
			continue;

		memset(&trigger_param, 0, sizeof(trigger_param));
		strncat(trigger_param.name, desc->name,
			LTTNG_KERNEL_SYM_NAME_LEN - strlen(trigger_param.name) - 1);
		trigger_param.name[LTTNG_KERNEL_SYM_NAME_LEN - 1] = '\0';
		trigger_param.instrumentation = LTTNG_KERNEL_SYSCALL;

		trigger = _lttng_trigger_create(desc, id, error_counter_index,
			group, &trigger_param, filter,
			trigger_param.instrumentation);
		if (IS_ERR(trigger)) {
			printk(KERN_INFO "Unable to create trigger %s\n",
				desc->name);
			ret = -ENOMEM;
			goto end;
		}

		trigger->u.syscall.syscall_id = i;
		trigger->u.syscall.is_compat = is_compat;
	}
end:
	return ret;

}

int lttng_syscals_create_matching_triggers(struct lttng_trigger_enabler *trigger_enabler, void *filter)
{
	int ret;

	ret = create_matching_triggers(trigger_enabler, filter, sc_table,
		ARRAY_SIZE(sc_table), false);
	if (ret)
		goto end;

	ret = create_matching_triggers(trigger_enabler, filter, compat_sc_table,
		ARRAY_SIZE(compat_sc_table), true);
end:
	return ret;
}

/*
 * Unregister the syscall trigger probes from the callsites.
 */
int lttng_syscalls_unregister_trigger(struct lttng_trigger_group *trigger_group)
{
	int ret;

	/*
	 * Only register the trigger probe on the `sys_enter` callsite for now.
	 * At the moment, we don't think it's desirable to have one fired
	 * trigger for the entry and one for the exit of a syscall.
	 */
	if (trigger_group->sys_enter_registered) {
		ret = lttng_wrapper_tracepoint_probe_unregister("sys_enter",
				(void *) syscall_entry_trigger_probe, trigger_group);
		if (ret)
			return ret;
		trigger_group->sys_enter_registered = 0;
	}

	kfree(trigger_group->trigger_syscall_dispatch);
#ifdef CONFIG_COMPAT
	kfree(trigger_group->trigger_compat_syscall_dispatch);
#endif
	return 0;
}

int lttng_syscalls_unregister_event(struct lttng_channel *chan)
{
	int ret;

	if (!chan->sc_table)
		return 0;
	if (chan->sys_enter_registered) {
		ret = lttng_wrapper_tracepoint_probe_unregister("sys_enter",
				(void *) syscall_entry_event_probe, chan);
		if (ret)
			return ret;
		chan->sys_enter_registered = 0;
	}
	if (chan->sys_exit_registered) {
		ret = lttng_wrapper_tracepoint_probe_unregister("sys_exit",
				(void *) syscall_exit_event_probe, chan);
		if (ret)
			return ret;
		chan->sys_exit_registered = 0;
	}
	/* lttng_event destroy will be performed by lttng_session_destroy() */
	kfree(chan->sc_table);
	kfree(chan->sc_exit_table);
#ifdef CONFIG_COMPAT
	kfree(chan->compat_sc_table);
	kfree(chan->compat_sc_exit_table);
#endif
	kfree(chan->sc_filter);
	return 0;
}

static
int get_syscall_nr(const char *syscall_name)
{
	int syscall_nr = -1;
	int i;

	for (i = 0; i < ARRAY_SIZE(sc_table); i++) {
		const struct trace_syscall_entry *entry;

		entry = &sc_table[i];
		if (!entry->desc)
			continue;

		if (!strcmp(syscall_name, entry->desc->name)) {
			syscall_nr = i;
			break;
		}
	}
	return syscall_nr;
}

static
int get_compat_syscall_nr(const char *syscall_name)
{
	int syscall_nr = -1;
	int i;

	for (i = 0; i < ARRAY_SIZE(compat_sc_table); i++) {
		const struct trace_syscall_entry *entry;

		entry = &compat_sc_table[i];
		if (!entry->desc)
			continue;

		if (!strcmp(syscall_name, entry->desc->name)) {
			syscall_nr = i;
			break;
		}
	}
	return syscall_nr;
}

static
uint32_t get_sc_tables_len(void)
{
	return ARRAY_SIZE(sc_table) + ARRAY_SIZE(compat_sc_table);
}

int lttng_syscall_filter_enable_event(struct lttng_channel *chan,
		const char *name)
{
	int syscall_nr, compat_syscall_nr, ret;
	struct lttng_syscall_filter *filter;

	WARN_ON_ONCE(!chan->sc_table);

	if (!name) {
		/* Enable all system calls by removing filter */
		if (chan->sc_filter) {
			filter = chan->sc_filter;
			rcu_assign_pointer(chan->sc_filter, NULL);
			synchronize_trace();
			kfree(filter);
		}
		chan->syscall_all = 1;
		return 0;
	}

	if (!chan->sc_filter) {
		if (chan->syscall_all) {
			/*
			 * All syscalls are already enabled.
			 */
			return -EEXIST;
		}
		filter = kzalloc(sizeof(struct lttng_syscall_filter),
				GFP_KERNEL);
		if (!filter)
			return -ENOMEM;
	} else {
		filter = chan->sc_filter;
	}
	syscall_nr = get_syscall_nr(name);
	compat_syscall_nr = get_compat_syscall_nr(name);
	if (syscall_nr < 0 && compat_syscall_nr < 0) {
		ret = -ENOENT;
		goto error;
	}
	if (syscall_nr >= 0) {
		if (test_bit(syscall_nr, filter->sc)) {
			ret = -EEXIST;
			goto error;
		}
		bitmap_set(filter->sc, syscall_nr, 1);
	}
	if (compat_syscall_nr >= 0) {
		if (test_bit(compat_syscall_nr, filter->sc_compat)) {
			ret = -EEXIST;
			goto error;
		}
		bitmap_set(filter->sc_compat, compat_syscall_nr, 1);
	}
	if (!chan->sc_filter)
		rcu_assign_pointer(chan->sc_filter, filter);
	return 0;

error:
	if (!chan->sc_filter)
		kfree(filter);
	return ret;
}

int lttng_syscall_filter_enable_trigger(struct lttng_trigger *trigger)
{
	struct lttng_trigger_group *group = trigger->group;
	unsigned int syscall_id = trigger->u.syscall.syscall_id;
	struct list_head *dispatch_list;

	if (trigger->u.syscall.is_compat)
		dispatch_list = &group->trigger_compat_syscall_dispatch[syscall_id];
	else
		dispatch_list = &group->trigger_syscall_dispatch[syscall_id];

	list_add_rcu(&trigger->u.syscall.node, dispatch_list);

	return 0;
}

int lttng_syscall_filter_disable_event(struct lttng_channel *chan,
		const char *name)
{
	int syscall_nr, compat_syscall_nr, ret;
	struct lttng_syscall_filter *filter;

	WARN_ON_ONCE(!chan->sc_table);

	if (!chan->sc_filter) {
		if (!chan->syscall_all)
			return -EEXIST;
		filter = kzalloc(sizeof(struct lttng_syscall_filter),
				GFP_KERNEL);
		if (!filter)
			return -ENOMEM;
		/* Trace all system calls, then apply disable. */
		bitmap_set(filter->sc, 0, NR_syscalls);
		bitmap_set(filter->sc_compat, 0, NR_compat_syscalls);
	} else {
		filter = chan->sc_filter;
	}

	if (!name) {
		/* Fail if all syscalls are already disabled. */
		if (bitmap_empty(filter->sc, NR_syscalls)
			&& bitmap_empty(filter->sc_compat,
				NR_compat_syscalls)) {
			ret = -EEXIST;
			goto error;
		}

		/* Disable all system calls */
		bitmap_clear(filter->sc, 0, NR_syscalls);
		bitmap_clear(filter->sc_compat, 0, NR_compat_syscalls);
		goto apply_filter;
	}
	syscall_nr = get_syscall_nr(name);
	compat_syscall_nr = get_compat_syscall_nr(name);
	if (syscall_nr < 0 && compat_syscall_nr < 0) {
		ret = -ENOENT;
		goto error;
	}
	if (syscall_nr >= 0) {
		if (!test_bit(syscall_nr, filter->sc)) {
			ret = -EEXIST;
			goto error;
		}
		bitmap_clear(filter->sc, syscall_nr, 1);
	}
	if (compat_syscall_nr >= 0) {
		if (!test_bit(compat_syscall_nr, filter->sc_compat)) {
			ret = -EEXIST;
			goto error;
		}
		bitmap_clear(filter->sc_compat, compat_syscall_nr, 1);
	}
apply_filter:
	if (!chan->sc_filter)
		rcu_assign_pointer(chan->sc_filter, filter);
	chan->syscall_all = 0;
	return 0;

error:
	if (!chan->sc_filter)
		kfree(filter);
	return ret;
}

int lttng_syscall_filter_disable_trigger(struct lttng_trigger *trigger)
{
	list_del_rcu(&trigger->u.syscall.node);
	return 0;
}

static
const struct trace_syscall_entry *syscall_list_get_entry(loff_t *pos)
{
	const struct trace_syscall_entry *entry;
	int iter = 0;

	for (entry = sc_table;
			entry < sc_table + ARRAY_SIZE(sc_table);
			 entry++) {
		if (iter++ >= *pos)
			return entry;
	}
	for (entry = compat_sc_table;
			entry < compat_sc_table + ARRAY_SIZE(compat_sc_table);
			 entry++) {
		if (iter++ >= *pos)
			return entry;
	}
	/* End of list */
	return NULL;
}

static
void *syscall_list_start(struct seq_file *m, loff_t *pos)
{
	return (void *) syscall_list_get_entry(pos);
}

static
void *syscall_list_next(struct seq_file *m, void *p, loff_t *ppos)
{
	(*ppos)++;
	return (void *) syscall_list_get_entry(ppos);
}

static
void syscall_list_stop(struct seq_file *m, void *p)
{
}

static
int get_sc_table(const struct trace_syscall_entry *entry,
		const struct trace_syscall_entry **table,
		unsigned int *bitness)
{
	if (entry >= sc_table && entry < sc_table + ARRAY_SIZE(sc_table)) {
		if (bitness)
			*bitness = BITS_PER_LONG;
		if (table)
			*table = sc_table;
		return 0;
	}
	if (!(entry >= compat_sc_table
			&& entry < compat_sc_table + ARRAY_SIZE(compat_sc_table))) {
		return -EINVAL;
	}
	if (bitness)
		*bitness = 32;
	if (table)
		*table = compat_sc_table;
	return 0;
}

static
int syscall_list_show(struct seq_file *m, void *p)
{
	const struct trace_syscall_entry *table, *entry = p;
	unsigned int bitness;
	unsigned long index;
	int ret;
	const char *name;

	ret = get_sc_table(entry, &table, &bitness);
	if (ret)
		return ret;
	if (!entry->desc)
		return 0;
	if (table == sc_table) {
		index = entry - table;
		name = &entry->desc->name[strlen(SYSCALL_ENTRY_STR)];
	} else {
		index = (entry - table) + ARRAY_SIZE(sc_table);
		name = &entry->desc->name[strlen(COMPAT_SYSCALL_ENTRY_STR)];
	}
	seq_printf(m,	"syscall { index = %lu; name = %s; bitness = %u; };\n",
		index, name, bitness);
	return 0;
}

static
const struct seq_operations lttng_syscall_list_seq_ops = {
	.start = syscall_list_start,
	.next = syscall_list_next,
	.stop = syscall_list_stop,
	.show = syscall_list_show,
};

static
int lttng_syscall_list_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &lttng_syscall_list_seq_ops);
}

const struct file_operations lttng_syscall_list_fops = {
	.owner = THIS_MODULE,
	.open = lttng_syscall_list_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

long lttng_channel_syscall_mask(struct lttng_channel *channel,
		struct lttng_kernel_syscall_mask __user *usyscall_mask)
{
	uint32_t len, sc_tables_len, bitmask_len;
	int ret = 0, bit;
	char *tmp_mask;
	struct lttng_syscall_filter *filter;

	ret = get_user(len, &usyscall_mask->len);
	if (ret)
		return ret;
	sc_tables_len = get_sc_tables_len();
	bitmask_len = ALIGN(sc_tables_len, 8) >> 3;
	if (len < sc_tables_len) {
		return put_user(sc_tables_len, &usyscall_mask->len);
	}
	/* Array is large enough, we can copy array to user-space. */
	tmp_mask = kzalloc(bitmask_len, GFP_KERNEL);
	if (!tmp_mask)
		return -ENOMEM;
	filter = channel->sc_filter;

	for (bit = 0; bit < ARRAY_SIZE(sc_table); bit++) {
		char state;

		if (channel->sc_table) {
			if (filter)
				state = test_bit(bit, filter->sc);
			else
				state = 1;
		} else {
			state = 0;
		}
		bt_bitfield_write_be(tmp_mask, char, bit, 1, state);
	}
	for (; bit < sc_tables_len; bit++) {
		char state;

		if (channel->compat_sc_table) {
			if (filter)
				state = test_bit(bit - ARRAY_SIZE(sc_table),
						filter->sc_compat);
			else
				state = 1;
		} else {
			state = 0;
		}
		bt_bitfield_write_be(tmp_mask, char, bit, 1, state);
	}
	if (copy_to_user(usyscall_mask->mask, tmp_mask, bitmask_len))
		ret = -EFAULT;
	kfree(tmp_mask);
	return ret;
}

int lttng_abi_syscall_list(void)
{
	struct file *syscall_list_file;
	int file_fd, ret;

	file_fd = lttng_get_unused_fd();
	if (file_fd < 0) {
		ret = file_fd;
		goto fd_error;
	}

	syscall_list_file = anon_inode_getfile("[lttng_syscall_list]",
					  &lttng_syscall_list_fops,
					  NULL, O_RDWR);
	if (IS_ERR(syscall_list_file)) {
		ret = PTR_ERR(syscall_list_file);
		goto file_error;
	}
	ret = lttng_syscall_list_fops.open(NULL, syscall_list_file);
	if (ret < 0)
		goto open_error;
	fd_install(file_fd, syscall_list_file);
	return file_fd;

open_error:
	fput(syscall_list_file);
file_error:
	put_unused_fd(file_fd);
fd_error:
	return ret;
}
