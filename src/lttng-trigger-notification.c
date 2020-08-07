/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-trigger-notification.c
 *
 * Copyright (C) 2020 Francis Deslauriers <francis.deslauriers@efficios.com>
 */

#include <linux/bug.h>

#include <lttng/lttng-bytecode.h>
#include <lttng/events.h>
#include <lttng/msgpack.h>
#include <lttng/trigger-notification.h>

/*
 * FIXME: this probably too low but it needs to be below 1024 bytes to avoid
 * the frame to be larger than the 1024 limit enforced by the kernel.
 */
#define CAPTURE_BUFFER_SIZE 512

struct lttng_trigger_notification {
	int notification_fd;
	uint64_t trigger_id;
	uint8_t capture_buf[CAPTURE_BUFFER_SIZE];
	struct lttng_msgpack_writer writer;
	bool has_captures;
};

static
int capture_enum(struct lttng_msgpack_writer *writer,
		struct lttng_interpreter_output *output)
{
	int ret;

	/*
	 * Enums are captured as a map containing 2 key-value pairs. Such as:
	 * - type: enum
	 *   value: 177
	 */
	ret = lttng_msgpack_begin_map(writer, 2);
	if (ret) {
		WARN_ON_ONCE(1);
		goto end;
	}

	ret = lttng_msgpack_write_str(writer, "type");
	if (ret) {
		WARN_ON_ONCE(1);
		goto end;
	}

	ret = lttng_msgpack_write_str(writer, "enum");
	if (ret) {
		WARN_ON_ONCE(1);
		goto end;
	}

	ret = lttng_msgpack_write_str(writer, "value");
	if (ret) {
		WARN_ON_ONCE(1);
		goto end;
	}

	switch (output->type) {
	case LTTNG_INTERPRETER_TYPE_SIGNED_ENUM:
		ret = lttng_msgpack_write_signed_integer(writer, output->u.s);
		if (ret) {
			WARN_ON_ONCE(1);
			goto end;
		}
		break;
	case LTTNG_INTERPRETER_TYPE_UNSIGNED_ENUM:
		ret = lttng_msgpack_write_signed_integer(writer, output->u.u);
		if (ret) {
			WARN_ON_ONCE(1);
			goto end;
		}
		break;
	default:
		WARN_ON(1);
	}

	ret = lttng_msgpack_end_map(writer);
	if (ret)
		WARN_ON_ONCE(1);

end:
	return ret;
}

static
int64_t capture_sequence_element_signed(uint8_t *ptr,
		const struct lttng_integer_type *type)
{
	int64_t value = 0;
	unsigned int size = type->size;
	bool byte_order_reversed = type->reverse_byte_order;

	switch (size) {
	case 8:
		value = *ptr;
		break;
	case 16:
	{
		int16_t tmp;
		tmp = *(int16_t *) ptr;
		if (byte_order_reversed)
			__swab16s(&tmp);

		value = tmp;
		break;
	}
	case 32:
	{
		int32_t tmp;
		tmp = *(int32_t *) ptr;
		if (byte_order_reversed)
			__swab32s(&tmp);

		value = tmp;
		break;
	}
	case 64:
	{
		int64_t tmp;
		tmp = *(int64_t *) ptr;
		if (byte_order_reversed)
			__swab64s(&tmp);

		value = tmp;
		break;
	}
	default:
		WARN_ON(1);
	}

	return value;
}

static
uint64_t capture_sequence_element_unsigned(uint8_t *ptr,
		const struct lttng_integer_type *type)
{
	uint64_t value = 0;
	unsigned int size = type->size;
	bool byte_order_reversed = type->reverse_byte_order;

	switch (size) {
	case 8:
		value = *ptr;
		break;
	case 16:
	{
		uint16_t tmp;
		tmp = *(uint16_t *) ptr;
		if (byte_order_reversed)
			__swab16s(&tmp);

		value = tmp;
		break;
	}
	case 32:
	{
		uint32_t tmp;
		tmp = *(uint32_t *) ptr;
		if (byte_order_reversed)
			__swab32s(&tmp);

		value = tmp;
		break;
	}
	case 64:
	{
		uint64_t tmp;
		tmp = *(uint64_t *) ptr;
		if (byte_order_reversed)
			__swab64s(&tmp);

		value = tmp;
		break;
	}
	default:
		WARN_ON(1);
	}

	return value;
}

int capture_sequence(struct lttng_msgpack_writer *writer,
		struct lttng_interpreter_output *output)
{
	const struct lttng_integer_type *integer_type = NULL;
	const struct lttng_type *nested_type;
	uint8_t *ptr;
	bool signedness;
	int ret, i;

	ret = lttng_msgpack_begin_array(writer, output->u.sequence.nr_elem);
	if (ret) {
		WARN_ON_ONCE(1);
		goto end;
	}

	ptr = (uint8_t *) output->u.sequence.ptr;
	nested_type = output->u.sequence.nested_type;
	switch (nested_type->atype) {
	case atype_integer:
		integer_type = &nested_type->u.integer;
		break;
	case atype_enum_nestable:
		/* Treat enumeration as an integer. */
		integer_type = &nested_type->u.enum_nestable.container_type->u.integer;
		break;
	default:
		/* Capture of array of non-integer are not supported. */
		WARN_ON(1);
	}
	signedness = integer_type->signedness;
	for (i = 0; i < output->u.sequence.nr_elem; i++) {
		if (signedness) {
			ret =lttng_msgpack_write_signed_integer(writer,
				capture_sequence_element_signed(ptr, integer_type));
			if (ret) {
				WARN_ON_ONCE(1);
				goto end;
			}
		} else {
			ret = lttng_msgpack_write_unsigned_integer(writer,
				capture_sequence_element_unsigned(ptr, integer_type));
			if (ret) {
				WARN_ON_ONCE(1);
				goto end;
			}
		}

		/*
		 * We assume that alignment is smaller or equal to the size.
		 * This currently holds true but if it changes in the future,
		 * we will want to change the pointer arithmetics below to
		 * take into account that the next element might be further
		 * away.
		 */
		WARN_ON(integer_type->alignment > integer_type->size);

		/* Size is in number of bits. */
		ptr += (integer_type->size / CHAR_BIT) ;
	}

	ret = lttng_msgpack_end_array(writer);
	if (ret)
		WARN_ON_ONCE(1);
end:
	return ret;
}

static
int notification_append_capture(
		struct lttng_trigger_notification *notif,
		struct lttng_interpreter_output *output)
{
	struct lttng_msgpack_writer *writer = &notif->writer;
	int ret = 0;

	switch (output->type) {
	case LTTNG_INTERPRETER_TYPE_S64:
		ret = lttng_msgpack_write_signed_integer(writer, output->u.s);
		if (ret) {
			WARN_ON_ONCE(1);
			goto end;
		}
		break;
	case LTTNG_INTERPRETER_TYPE_U64:
		ret = lttng_msgpack_write_unsigned_integer(writer, output->u.u);
		if (ret) {
			WARN_ON_ONCE(1);
			goto end;
		}
		break;
	case LTTNG_INTERPRETER_TYPE_STRING:
		ret = lttng_msgpack_write_str(writer, output->u.str.str);
		if (ret) {
			WARN_ON_ONCE(1);
			goto end;
		}
		break;
	case LTTNG_INTERPRETER_TYPE_SEQUENCE:
		ret = capture_sequence(writer, output);
		if (ret) {
			WARN_ON_ONCE(1);
			goto end;
		}
		break;
	case LTTNG_INTERPRETER_TYPE_SIGNED_ENUM:
	case LTTNG_INTERPRETER_TYPE_UNSIGNED_ENUM:
		ret = capture_enum(writer, output);
		if (ret) {
			WARN_ON_ONCE(1);
			goto end;
		}
		break;
	default:
		ret = -1;
		WARN_ON(1);
	}
end:
	return ret;
}

static
int notification_append_empty_capture(
		struct lttng_trigger_notification *notif)
{
	int ret = lttng_msgpack_write_nil(&notif->writer);
	if (ret)
		WARN_ON_ONCE(1);

	return ret;
}

static
int notification_init(struct lttng_trigger_notification *notif,
		struct lttng_trigger *trigger)
{
	struct lttng_msgpack_writer *writer = &notif->writer;
	int ret = 0;

	notif->has_captures = false;

	if (trigger->num_captures > 0) {
		lttng_msgpack_writer_init(writer, notif->capture_buf,
				CAPTURE_BUFFER_SIZE);

		ret = lttng_msgpack_begin_array(writer, trigger->num_captures);
		if (ret) {
			WARN_ON_ONCE(1);
			goto end;
		}

		notif->has_captures = true;
	}

end:
	return ret;
}

static
void notification_send(struct lttng_trigger_notification *notif,
		struct lttng_trigger *trigger)
{
	struct lttng_trigger_group *trigger_group = trigger->group;
	struct lib_ring_buffer_ctx ctx;
	struct lttng_kernel_trigger_notification kernel_notif;
	size_t capture_buffer_content_len, reserve_size;
	int ret;
	int64_t dimension_index[1];

	reserve_size = sizeof(kernel_notif);
	kernel_notif.id = trigger->id;

	if (notif->has_captures) {
		capture_buffer_content_len = notif->writer.write_pos - notif->writer.buffer;
	} else {
		capture_buffer_content_len = 0;
	}

	WARN_ON_ONCE(capture_buffer_content_len > CAPTURE_BUFFER_SIZE);

	reserve_size += capture_buffer_content_len;
	kernel_notif.capture_buf_size = capture_buffer_content_len;

	lib_ring_buffer_ctx_init(&ctx, trigger_group->chan, NULL, reserve_size,
			lttng_alignof(kernel_notif), -1);
	ret = trigger_group->ops->event_reserve(&ctx, 0);
	if (ret < 0) {
		//TODO: error handling with counter maps
		//silently drop for now.
		WARN_ON_ONCE(1);
		return;
	}

	dimension_index[0] = trigger->error_counter_index;

	trigger_group->error_counter->ops->counter_add(trigger_group->error_counter->counter,
			dimension_index, 1);

	lib_ring_buffer_align_ctx(&ctx, lttng_alignof(kernel_notif));

	/* Write the notif structure. */
	trigger_group->ops->event_write(&ctx, &kernel_notif,
			sizeof(kernel_notif));

	/*
	 * Write the capture buffer. No need to realigned as the below is a raw
	 * char* buffer.
	 */
	trigger_group->ops->event_write(&ctx, &notif->capture_buf,
			capture_buffer_content_len);

	trigger_group->ops->event_commit(&ctx);
	irq_work_queue(&trigger_group->wakeup_pending);
}

void lttng_trigger_notification_send(struct lttng_trigger *trigger,
		struct lttng_probe_ctx *lttng_probe_ctx,
		const char *stack_data)
{
	struct lttng_trigger_notification notif = {0};
	int ret;

	if (unlikely(!READ_ONCE(trigger->enabled)))
		return;

	ret = notification_init(&notif, trigger);
	if (ret) {
		WARN_ON_ONCE(1);
		goto end;
	}

	if (unlikely(!list_empty(&trigger->capture_bytecode_runtime_head))) {
		struct lttng_bytecode_runtime *capture_bc_runtime;

		/*
		 * Iterate over all the capture bytecodes. If the interpreter
		 * functions returns successfully, append the value of the
		 * `output` parameter to the capture buffer. If the interpreter
		 * fails, append an empty capture to the buffer.
		 */
		list_for_each_entry(capture_bc_runtime,
				&trigger->capture_bytecode_runtime_head, node) {
			struct lttng_interpreter_output output;

			if (capture_bc_runtime->interpreter_funcs.capture(capture_bc_runtime,
					lttng_probe_ctx, stack_data, &output) & LTTNG_INTERPRETER_RECORD_FLAG)
				ret = notification_append_capture(&notif, &output);
			else
				ret = notification_append_empty_capture(&notif);

			if (ret)
				printk(KERN_WARNING "Error appending capture to notification");
		}
	}

	/*
	 * Send the notification (including the capture buffer) to the
	 * sessiond.
	 */
	notification_send(&notif, trigger);
end:
	return;
}
