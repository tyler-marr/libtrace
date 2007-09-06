#define _GNU_SOURCE

#include "config.h"
#include "common.h"
#include "libtrace.h"
#include "libtrace_int.h"
#include "format_helper.h"

#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



#define DATA(x) ((struct atmhdr_format_data_t *)x->format_data)

#define INPUT DATA(libtrace)->input

struct atmhdr_format_data_t {
        union {
                int fd;
                libtrace_io_t *file;
        } input;
};

static int atmhdr_get_framing_length(const libtrace_packet_t *packet UNUSED)
{
	return sizeof(atmhdr_t);
}

static int atmhdr_init_input(libtrace_t *libtrace) {
	libtrace->format_data = malloc(sizeof(struct atmhdr_format_data_t));
	DATA(libtrace)->input.file = NULL;
	return 0;
}

static int atmhdr_start_input(libtrace_t *libtrace)
{
	if (DATA(libtrace)->input.file)
		return 0;
	DATA(libtrace)->input.file = trace_open_file(libtrace);
	if (DATA(libtrace)->input.file)
		return 0;
	return -1;
}

static int atmhdr_fin_input(libtrace_t *libtrace)
{
	libtrace_io_close(INPUT.file);
	free(libtrace->format_data);
	return 0;
}

static int atmhdr_read_packet(libtrace_t *libtrace, libtrace_packet_t *packet) {
	int numbytes;
	void *buffer;

	if (!packet->buffer || packet->buf_control == TRACE_CTRL_EXTERNAL) {
		packet->buf_control = TRACE_CTRL_PACKET;
		packet->buffer=malloc((size_t)LIBTRACE_PACKET_BUFSIZE);
	}
	buffer = packet->buffer;

	packet->type = TRACE_RT_DATA_ATMHDR;

	if ((numbytes=libtrace_io_read(INPUT.file, buffer, (size_t)12)) != 12)
	{
		if (numbytes != 0) {
			trace_set_err(libtrace,errno,"read(%s)",libtrace->uridata);
		}
		return numbytes;
	}

	packet->header = packet->buffer;
	packet->payload = (void*)((char*)packet->buffer +
			libtrace->format->get_framing_length(packet));

	return 12;
}

static libtrace_linktype_t atmhdr_get_link_type(const libtrace_packet_t *packet UNUSED) {
	return TRACE_TYPE_ATM;
}

static int atmhdr_get_capture_length(const libtrace_packet_t *packet UNUSED) {
	return 4;
}

static int atmhdr_get_wire_length(const libtrace_packet_t *packet UNUSED) {
	return 53;
}

static uint64_t atmhdr_get_erf_timestamp(const libtrace_packet_t *packet) {
	uint64_t ts;
	atmhdr_t *atm = (atmhdr_t *)packet->header;
	ts = (uint64_t)atm->ts_fraction + ((uint64_t)atm->ts_sec << 32);

	return ts;
}

static struct libtrace_format_t atmhdr = {
	"atmhdr",
	"$Id$",
	TRACE_FORMAT_ATMHDR,
        atmhdr_init_input,                 /* init_input */
        NULL,                           /* config_input */
        atmhdr_start_input,                /* start_input */
        NULL,                           /* pause_input */
        NULL,                           /* init_output */
        NULL,                           /* config_output */
        NULL,                           /* start_output */
        atmhdr_fin_input,                  /* fin_input */
        NULL,                           /* fin_output */
        atmhdr_read_packet,             /* read_packet */
        NULL,                           /* fin_packet */
        NULL,                           /* write_packet */
        atmhdr_get_link_type,        /* get_link_type */
        NULL,                           /* get_direction */
        NULL,                           /* set_direction */
        atmhdr_get_erf_timestamp,       /* get_erf_timestamp */
        NULL,                           /* get_timeval */
        NULL,                           /* get_seconds */
        NULL,                           /* seek_erf */
        NULL,                           /* seek_timeval */
        NULL,                           /* seek_seconds */
        atmhdr_get_capture_length,      /* get_capture_length */
        atmhdr_get_wire_length,      /* get_wire_length */
        atmhdr_get_framing_length,   /* get_framing_length */
        NULL,                           /* set_capture_length */
        NULL,                           /* get_fd */
        trace_event_trace,              /* trace_event */
        NULL,                 /* help */
        NULL                            /* next pointer */
};
	

void atmhdr_constructor(void) {
	register_format(&atmhdr);
}