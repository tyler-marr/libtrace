/*
 * This file is part of libtrace
 *
 * Copyright (c) 2007,2008 The University of Waikato, Hamilton, New Zealand.
 * Authors: Daniel Lawson 
 *          Perry Lorier 
 *          
 * All rights reserved.
 *
 * This code has been developed by the University of Waikato WAND 
 * research group. For further information please see http://www.wand.net.nz/
 *
 * libtrace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libtrace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libtrace; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 *
 */

#include "common.h"
#include "config.h"
#include "libtrace.h"
#include "libtrace_int.h"
#include "format_helper.h"

#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define DATA(x) ((struct pcapfile_format_data_t*)((x)->format_data))
#define DATAOUT(x) ((struct pcapfile_format_data_out_t*)((x)->format_data))
#define IN_OPTIONS DATA(libtrace)->options

typedef struct pcapfile_header_t {
		uint32_t magic_number;   /* magic number */
		uint16_t version_major;  /* major version number */
		uint16_t version_minor;  /* minor version number */
		int32_t  thiszone;       /* GMT to local correction */
		uint32_t sigfigs;        /* timestamp accuracy */
		uint32_t snaplen;        /* aka "wirelen" */
		uint32_t network;        /* data link type */
} pcapfile_header_t; 

struct pcapfile_format_data_t {
	libtrace_io_t *file;
	struct {
		int real_time;
	} options;
	pcapfile_header_t header;
};

struct pcapfile_format_data_out_t {
	libtrace_io_t *file;
	int level;
	int flag;

};

static int pcapfile_init_input(libtrace_t *libtrace) {
	libtrace->format_data = malloc(sizeof(struct pcapfile_format_data_t));

	if (libtrace->format_data == NULL) {
		trace_set_err(libtrace,ENOMEM,"Out of memory");
		return -1;
	}

	DATA(libtrace)->file=NULL;
	IN_OPTIONS.real_time = 0;
	return 0;
}

static int pcapfile_init_output(libtrace_out_t *libtrace) {
	libtrace->format_data = 
		malloc(sizeof(struct pcapfile_format_data_out_t));

	DATAOUT(libtrace)->file=NULL;
	DATAOUT(libtrace)->level=0;
	DATAOUT(libtrace)->flag=O_CREAT|O_WRONLY;

	return 0;
}

static uint16_t swaps(libtrace_t *libtrace, uint16_t num)
{
	/* to deal with open_dead traces that might try and use this
	 * if we don't have any per trace data, assume host byte order
	 */
	if (!DATA(libtrace))
		return num;
	if (DATA(libtrace)->header.magic_number == 0xd4c3b2a1)
		return byteswap16(num);

	return num;
}

static uint32_t swapl(libtrace_t *libtrace, uint32_t num)
{
	/* to deal with open_dead traces that might try and use this
	 * if we don't have any per trace data, assume host byte order
	 */
	if (!DATA(libtrace))
		return num;
	if (DATA(libtrace)->header.magic_number == 0xd4c3b2a1)
		return byteswap32(num);

	return num;
}


static int pcapfile_start_input(libtrace_t *libtrace) 
{
	int err;

	if (!DATA(libtrace)->file) {
		DATA(libtrace)->file=trace_open_file(libtrace);

		if (!DATA(libtrace)->file)
			return -1;

		err=libtrace_io_read(DATA(libtrace)->file,
				&DATA(libtrace)->header,
				sizeof(DATA(libtrace)->header));

		if (err<1)
			return -1;
		
		if (swapl(libtrace,DATA(libtrace)->header.magic_number) != 0xa1b2c3d4) {
			trace_set_err(libtrace,TRACE_ERR_INIT_FAILED,"Not a pcap tracefile\n");
			return -1; /* Not a pcap file */
		}

		if (swaps(libtrace,DATA(libtrace)->header.version_major)!=2
			&& swaps(libtrace,DATA(libtrace)->header.version_minor)!=4) {
			trace_set_err(libtrace,TRACE_ERR_INIT_FAILED,"Unknown pcap tracefile version %d.%d\n",
					swaps(libtrace,
						DATA(libtrace)->header.version_major),
					swaps(libtrace,
						DATA(libtrace)->header.version_minor));
			return -1;
		}

	}

	return 0;
}

static int pcapfile_start_output(libtrace_out_t *libtrace UNUSED)
{
	return 0;
}

static int pcapfile_config_input(libtrace_t *libtrace,
		trace_option_t option,
		void *data)
{
	switch(option) {
		case TRACE_OPTION_EVENT_REALTIME:
			IN_OPTIONS.real_time = *(int *)data;
			return 0;
		case TRACE_OPTION_META_FREQ:
		case TRACE_OPTION_SNAPLEN:
		case TRACE_OPTION_PROMISC:
		case TRACE_OPTION_FILTER:
			/* all these are either unsupported or handled
			 * by trace_config */
			break;
	}
	
	trace_set_err(libtrace,TRACE_ERR_UNKNOWN_OPTION,
			"Unknown option %i", option);
	return -1;
}

static int pcapfile_fin_input(libtrace_t *libtrace) 
{
	if (DATA(libtrace)->file)
		libtrace_io_close(DATA(libtrace)->file);
	free(libtrace->format_data);
	return 0; /* success */
}

static int pcapfile_fin_output(libtrace_out_t *libtrace)
{
	if (DATA(libtrace)->file)
		libtrace_io_close(DATA(libtrace)->file);
	free(libtrace->format_data);
	libtrace->format_data=NULL;
	return 0; /* success */
}

static int pcapfile_config_output(libtrace_out_t *libtrace,
		trace_option_output_t option,
		void *value)
{
	switch (option) {
		case TRACE_OPTION_OUTPUT_COMPRESS:
			DATAOUT(libtrace)->level = *(int*)value;
			return 0;
		case TRACE_OPTION_OUTPUT_FILEFLAGS:
			DATAOUT(libtrace)->flag = *(int*)value;
			return 0;
		default:
			/* Unknown option */
			trace_set_err_out(libtrace,TRACE_ERR_UNKNOWN_OPTION,
					"Unknown option");
			return -1;
	}
	return -1;
}

static int pcapfile_prepare_packet(libtrace_t *libtrace, 
		libtrace_packet_t *packet, void *buffer, 
		libtrace_rt_types_t rt_type, uint32_t flags) {

	if (packet->buffer != buffer && 
			packet->buf_control == TRACE_CTRL_PACKET) {
		free(packet->buffer);
	}

	if ((flags & TRACE_PREP_OWN_BUFFER) == TRACE_PREP_OWN_BUFFER) {
		packet->buf_control = TRACE_CTRL_PACKET;
	} else
                packet->buf_control = TRACE_CTRL_EXTERNAL;
	
	
	packet->buffer = buffer;
	packet->header = buffer;
	packet->payload = (char*)packet->buffer 
		+ sizeof(libtrace_pcapfile_pkt_hdr_t);
	packet->type = rt_type;	

	if (libtrace->format_data == NULL) {
		if (pcapfile_init_input(libtrace))
			return -1;
	}
	
	return 0;
}

static int pcapfile_read_packet(libtrace_t *libtrace, libtrace_packet_t *packet)
{
	int err;
	uint32_t flags = 0;
	
	assert(libtrace->format_data);

	packet->type = pcap_linktype_to_rt(swapl(libtrace,
				DATA(libtrace)->header.network));

	if (!packet->buffer || packet->buf_control == TRACE_CTRL_EXTERNAL) {
		packet->buffer = malloc((size_t)LIBTRACE_PACKET_BUFSIZE);
	}

	flags |= TRACE_PREP_OWN_BUFFER;
	
	err=libtrace_io_read(DATA(libtrace)->file,
			packet->buffer,
			sizeof(libtrace_pcapfile_pkt_hdr_t));

	assert(swapl(libtrace,((libtrace_pcapfile_pkt_hdr_t*)packet->buffer)->caplen)<LIBTRACE_PACKET_BUFSIZE);

	if (err<0) {
		trace_set_err(libtrace,errno,"reading packet");
		return -1;
	}
	if (err==0) {
		/* EOF */
		return 0;
	}

	err=libtrace_io_read(DATA(libtrace)->file,
			(char*)packet->buffer+sizeof(libtrace_pcapfile_pkt_hdr_t),
			(size_t)swapl(libtrace,((libtrace_pcapfile_pkt_hdr_t*)packet->buffer)->caplen)
			);

	
	if (err<0) {
		trace_set_err(libtrace,errno,"reading packet");
		return -1;
	}
	if (err==0) {
		return 0;
	}

	if (pcapfile_prepare_packet(libtrace, packet, packet->buffer,
				packet->type, flags)) {
		return -1;
	}
	
	return sizeof(libtrace_pcapfile_pkt_hdr_t)
		+swapl(libtrace,((libtrace_pcapfile_pkt_hdr_t*)packet->buffer)->caplen);
}

static int pcapfile_write_packet(libtrace_out_t *out,
		libtrace_packet_t *packet)
{
	struct libtrace_pcapfile_pkt_hdr_t hdr;
	struct timeval tv = trace_get_timeval(packet);
	int numbytes;
	int ret;
	void *ptr;
	uint32_t remaining;
	libtrace_linktype_t linktype;

	ptr = trace_get_packet_buffer(packet,&linktype,&remaining);
	
	/* Silently discard RT metadata packets and packets with an
	 * unknown linktype. */
	if (linktype == TRACE_TYPE_METADATA || linktype == -1) {
		return 0;
	}

	/* If this packet cannot be converted to a pcap linktype then
	 * pop off the top header until it can be converted
	 */
	while (libtrace_to_pcap_linktype(linktype)==~0U) {
		if (!demote_packet(packet)) {
			trace_set_err_out(out, 
				TRACE_ERR_NO_CONVERSION,
				"pcap does not support this format");
			return -1;
		}

		ptr = trace_get_packet_buffer(packet,&linktype,&remaining);
	}


	/* Now we know the link type write out a header if we've not done
	 * so already
	 */
	if (!DATAOUT(out)->file) {
		struct pcapfile_header_t pcaphdr;

		DATAOUT(out)->file=trace_open_file_out(out,
				DATAOUT(out)->level,
				DATAOUT(out)->flag);
		if (!DATAOUT(out)->file)
			return -1;

		pcaphdr.magic_number = 0xa1b2c3d4;
		pcaphdr.version_major = 2;
		pcaphdr.version_minor = 4;
		pcaphdr.thiszone = 0;
		pcaphdr.sigfigs = 0;
		pcaphdr.snaplen = 65536;
		pcaphdr.network = 
			libtrace_to_pcap_linktype(linktype);

		libtrace_io_write(DATAOUT(out)->file, 
				&pcaphdr, sizeof(pcaphdr));
	}

	hdr.ts_sec = tv.tv_sec;
	hdr.ts_usec = tv.tv_usec;
	hdr.caplen = trace_get_capture_length(packet);
	assert(hdr.caplen < LIBTRACE_PACKET_BUFSIZE);
	/* PCAP doesn't include the FCS, we do */
	if (linktype==TRACE_TYPE_ETH)
		if (trace_get_wire_length(packet) >= 4) {
			hdr.wirelen = trace_get_wire_length(packet)-4;
		}
		else {
			hdr.wirelen = 0;
		}
	else
		hdr.wirelen = trace_get_wire_length(packet);

	assert(hdr.wirelen < LIBTRACE_PACKET_BUFSIZE);


	numbytes=libtrace_io_write(DATAOUT(out)->file,
			&hdr, sizeof(hdr));

	if (numbytes!=sizeof(hdr)) 
		return -1;

	ret=libtrace_io_write(DATAOUT(out)->file,
			ptr,
			remaining);

	if (ret!=(int)remaining)
		return -1;

	return numbytes+ret;
}

static libtrace_linktype_t pcapfile_get_link_type(
		const libtrace_packet_t *packet) 
{
#if 0
	return pcap_linktype_to_libtrace(
			swapl(packet->trace,
				DATA(packet->trace)->header.network
			     )
			);
#endif
	return pcap_linktype_to_libtrace(rt_to_pcap_linktype(packet->type));
}

static libtrace_direction_t pcapfile_get_direction(const libtrace_packet_t *packet) 
{
	libtrace_direction_t direction  = -1;
	switch(pcapfile_get_link_type(packet)) {
		case TRACE_TYPE_LINUX_SLL:
		{
			libtrace_sll_header_t *sll;
			libtrace_linktype_t linktype;

			sll = (libtrace_sll_header_t*)trace_get_packet_buffer(
					packet,
					&linktype,
					NULL);
			if (!sll) {
				trace_set_err(packet->trace,
					TRACE_ERR_BAD_PACKET,
						"Bad or missing packet");
				return -1;
			}
			/* 0 == LINUX_SLL_HOST */
			/* the Waikato Capture point defines "packets
			 * originating locally" (ie, outbound), with a
			 * direction of 0, and "packets destined locally"
			 * (ie, inbound), with a direction of 1.
			 * This is kind-of-opposite to LINUX_SLL.
			 * We return consistent values here, however
			 *
			 * Note that in recent versions of pcap, you can
			 * use "inbound" and "outbound" on ppp in linux
			 */
			if (ntohs(sll->pkttype == 0)) {
				direction = TRACE_DIR_INCOMING;
			} else {
				direction = TRACE_DIR_OUTGOING;
			}
			break;

		}
		case TRACE_TYPE_PFLOG:
		{
			libtrace_pflog_header_t *pflog;
			libtrace_linktype_t linktype;

			pflog=(libtrace_pflog_header_t*)trace_get_packet_buffer(
					packet,&linktype,NULL);
			if (!pflog) {
				trace_set_err(packet->trace,
						TRACE_ERR_BAD_PACKET,
						"Bad or missing packet");
				return -1;
			}
			/* enum    { PF_IN=0, PF_OUT=1 }; */
			if (ntohs(pflog->dir==0)) {

				direction = TRACE_DIR_INCOMING;
			}
			else {
				direction = TRACE_DIR_OUTGOING;
			}
			break;
		}
		default:
			break;
	}	
	return direction;
}


static struct timeval pcapfile_get_timeval(
		const libtrace_packet_t *packet) 
{
	libtrace_pcapfile_pkt_hdr_t *hdr =
		(libtrace_pcapfile_pkt_hdr_t*)packet->header;
	struct timeval ts;
	ts.tv_sec = swapl(packet->trace,hdr->ts_sec);
	ts.tv_usec = swapl(packet->trace,hdr->ts_usec);
	return ts;
}


static int pcapfile_get_capture_length(const libtrace_packet_t *packet) {
	libtrace_pcapfile_pkt_hdr_t *pcapptr 
		= (libtrace_pcapfile_pkt_hdr_t *)packet->header;

	return swapl(packet->trace,pcapptr->caplen);
}

static int pcapfile_get_wire_length(const libtrace_packet_t *packet) {
	libtrace_pcapfile_pkt_hdr_t *pcapptr 
		= (libtrace_pcapfile_pkt_hdr_t *)packet->header;
	if (packet->type==pcap_linktype_to_rt(TRACE_DLT_EN10MB))
		/* Include the missing FCS */
		return swapl(packet->trace,pcapptr->wirelen)+4; 
	else if (packet->type==pcap_linktype_to_rt(TRACE_DLT_IEEE802_11_RADIO)){
		/* If the packet is Radiotap and the flags field indicates
		 * that the FCS is not included in the 802.11 frame, then
		 * we need to add 4 to the wire-length to account for it.
		 */
		uint8_t flags;
		void *link;
		libtrace_linktype_t linktype;
		link = trace_get_packet_buffer(packet, &linktype, NULL);
		trace_get_wireless_flags(link, linktype, &flags);
		if ((flags & TRACE_RADIOTAP_F_FCS) == 0)
			return swapl(packet->trace,pcapptr->wirelen)+4;
	}
	return swapl(packet->trace,pcapptr->wirelen);
}

static int pcapfile_get_framing_length(const libtrace_packet_t *packet UNUSED) {
	return sizeof(libtrace_pcapfile_pkt_hdr_t);
}

static size_t pcapfile_set_capture_length(libtrace_packet_t *packet,size_t size) {
	libtrace_pcapfile_pkt_hdr_t *pcapptr = 0;
	assert(packet);
	if (size > trace_get_capture_length(packet)) {
		/* can't make a packet larger */
		return trace_get_capture_length(packet);
	}
	/* Reset the cached capture length */
	packet->capture_length = -1;
	pcapptr = (libtrace_pcapfile_pkt_hdr_t *)packet->header;
	pcapptr->caplen = swapl(packet->trace,(uint32_t)size);
	return trace_get_capture_length(packet);
}

static struct libtrace_eventobj_t pcapfile_event(libtrace_t *libtrace, libtrace_packet_t *packet) {
	
	libtrace_eventobj_t event = {0,0,0.0,0};
	
	if (IN_OPTIONS.real_time) {
		event.size = pcapfile_read_packet(libtrace, packet);
		if (event.size < 1)
			event.type = TRACE_EVENT_TERMINATE;
		else
			event.type = TRACE_EVENT_PACKET;
		return event;
	} else {
		return trace_event_trace(libtrace, packet);
	}
}

static void pcapfile_help(void) {
	printf("pcapfile format module: $Revision$\n");
	printf("Supported input URIs:\n");
	printf("\tpcapfile:/path/to/file\n");
	printf("\tpcapfile:/path/to/file.gz\n");
	printf("\n");
	printf("\te.g.: pcapfile:/tmp/trace.pcap\n");
	printf("\n");
}

static struct libtrace_format_t pcapfile = {
	"pcapfile",
	"$Id$",
	TRACE_FORMAT_PCAPFILE,
	pcapfile_init_input,		/* init_input */
	pcapfile_config_input,		/* config_input */
	pcapfile_start_input,		/* start_input */
	NULL,				/* pause_input */
	pcapfile_init_output,		/* init_output */
	pcapfile_config_output,		/* config_output */
	pcapfile_start_output,		/* start_output */
	pcapfile_fin_input,		/* fin_input */
	pcapfile_fin_output,		/* fin_output */
	pcapfile_read_packet,		/* read_packet */
	pcapfile_prepare_packet,	/* prepare_packet */
	NULL,				/* fin_packet */
	pcapfile_write_packet,		/* write_packet */
	pcapfile_get_link_type,		/* get_link_type */
	pcapfile_get_direction,		/* get_direction */
	NULL,				/* set_direction */
	NULL,				/* get_erf_timestamp */
	pcapfile_get_timeval,		/* get_timeval */
	NULL,				/* get_seconds */
	NULL,				/* seek_erf */
	NULL,				/* seek_timeval */
	NULL,				/* seek_seconds */
	pcapfile_get_capture_length,	/* get_capture_length */
	pcapfile_get_wire_length,	/* get_wire_length */
	pcapfile_get_framing_length,	/* get_framing_length */
	pcapfile_set_capture_length,	/* set_capture_length */
	NULL,				/* get_received_packets */
	NULL,				/* get_filtered_packets */
	NULL,				/* get_dropped_packets */
	NULL,				/* get_captured_packets */
	NULL,				/* get_fd */
	trace_event_trace,		/* trace_event */
	pcapfile_help,			/* help */
	NULL				/* next pointer */
};


void pcapfile_constructor(void) {
	register_format(&pcapfile);
}


