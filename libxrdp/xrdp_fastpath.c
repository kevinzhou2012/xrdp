/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004-2010
   Copyright (C) Kevin Zhou 2012

*/

#include "libxrdp.h"

/*****************************************************************************/
struct xrdp_fastpath* APP_CC
xrdp_fastpath_create(struct xrdp_session* session)
{
	struct xrdp_fastpath *self;

	self = (struct xrdp_fastpath*)g_malloc(sizeof(struct xrdp_fastpath), 1);
	self->tcp_layer = ((struct xrdp_rdp*)session->rdp)->sec_layer->mcs_layer->iso_layer->tcp_layer;
	make_stream(self->out_s);
	init_stream(self->out_s,FASTPATH_MAX_PACKET_SIZE);
	return self;
}

/*****************************************************************************/
void APP_CC
xrdp_fastpath_delete(struct xrdp_fastpath* self)
{
	if (self == 0)
	{
		return;
	}
	free_stream(self->out_s);
	g_free(self);
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_fastpath_reset(struct xrdp_fastpath* self)
{
	return 0;
}

int APP_CC
xrdp_fastpath_init(struct xrdp_fastpath* self)
{
	return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_fastpath_send_update_pdu(struct xrdp_fastpath* self, tui8 updateCode, struct stream* s)
{
	tui16  len;
	tui16  maxLen;
	tui32  payloadLeft;
	tui8   fragment;
	struct stream* s_send;
	tui8   compression = 0;
	int    i;

	s_send = self->out_s;

	maxLen = FASTPATH_MAX_PACKET_SIZE - 6 ; /* 6 bytes for header */
	payloadLeft = (s->end - s->data);

	for (i=0;payloadLeft>0;i++)
	{
		if (payloadLeft > maxLen)
			len = maxLen;
		else
			len = payloadLeft;
		payloadLeft -= len;
		
		if (payloadLeft==0)
			fragment = i ? FASTPATH_FRAGMENT_LAST : FASTPATH_FRAGMENT_SINGLE;
		else
			fragment = i ? FASTPATH_FRAGMENT_NEXT : FASTPATH_FRAGMENT_FIRST;

		init_stream(s_send,0);
		
		out_uint8(s_send, 0);	/* fOutputHeader */
		out_uint8(s_send, ((len+6) >> 8) | 0x80);	/* use 2 bytes for length even length < 128 ??? */
		out_uint8(s_send, (len+6) & 0xff);
		out_uint8(s_send, (updateCode&0x0f)|((fragment&0x03)<<4)|((compression&0x03)<<6));
		out_uint16_le(s_send, len);

		s_copy(s_send, s, len);
		s_mark_end(s_send);
		//g_writeln("xrdp_fastpath_send_update_pdu: %d %d %d",len,i,payloadLeft);
		if (xrdp_tcp_send(self->tcp_layer, s_send) != 0)
		{
			return 1;
		}
	}
		 
	return 0;
}

int
xrdp_fastpath_process_update(struct xrdp_fastpath* self, tui8 updateCode,
				tui32 size, struct stream* s)
{
	g_writeln("xrdp_fastpath_process_update: size= %8d updateCode= %d\n",size,updateCode);
	switch (updateCode)
	{
		case FASTPATH_UPDATETYPE_ORDERS:
		case FASTPATH_UPDATETYPE_BITMAP:
		case FASTPATH_UPDATETYPE_PALETTE:
		case FASTPATH_UPDATETYPE_SYNCHRONIZE:
		case FASTPATH_UPDATETYPE_SURFCMDS:
		case FASTPATH_UPDATETYPE_PTR_NULL:
		case FASTPATH_UPDATETYPE_PTR_DEFAULT:
		case FASTPATH_UPDATETYPE_PTR_POSITION:
		case FASTPATH_UPDATETYPE_COLOR:
		case FASTPATH_UPDATETYPE_CACHED:
		case FASTPATH_UPDATETYPE_POINTER:
			break;
		default:
			g_writeln("unknown updateCode 0x%X", updateCode);
			break;
	}

	return 0;
} 


int APP_CC
xrdp_fastpath_process_data(struct xrdp_fastpath* self, struct stream* s, tui8 header)
{
	tui8  encryptionFlags;
	tui8  numberEvents;
	tui16 length;
	tui8  length2;
	tui8  updateHeader;
	tui8  updateCode;
	tui8  updateFrag;
	tui8  updateComp;
	tui32 size;

	encryptionFlags = (header & 0xc0) >> 6;
	numberEvents = (header & 0x3c) >> 2;

	g_writeln("xrdp_fastpath_process_data: encryptionFlags=%d,numberEvents=%d\n",
			encryptionFlags,numberEvents);

	xrdp_tcp_recv(self->tcp_layer, s, 1);
	in_uint8(s, length);
	if (length & 0x80)
	{
		xrdp_tcp_recv(self->tcp_layer, s, 1);
		in_uint8(s, length2);
		length = (length & 0x7f) << 8 + length2 - 3;
	}
	else
	{
		length -= 2;
	}
	xrdp_tcp_recv(self->tcp_layer, s, length);
	if (encryptionFlags!= 0)
	{
		/* TODO decryption ...*/
	}

	/* parse updateHeader */
	in_uint8(s, updateHeader);
	updateCode = (updateHeader & 0x0f);
	updateFrag = (updateHeader & 0x30) >> 4;
	updateComp = (updateHeader & 0xc0) >> 6;

	if (updateFrag && updateComp)
	{
		/* TODO */
		g_writeln("xrdp_fastpath_process_data: updateFrag=%d,updateComp=%d\n",
			updateFrag,updateComp);
		return 1;
	}
	in_uint16_le(s, size);
	
	return xrdp_fastpath_process_update(self, updateCode, size, s); 
}

