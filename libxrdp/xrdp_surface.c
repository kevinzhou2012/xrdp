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
   Copyright (C) Jay Sorg 2004-2009
   Copyright (C) Kevin Zhou 2012

*/

#include "libxrdp.h"
#include "freerdp/codec/rfx.h"

/*****************************************************************************/
struct xrdp_surface* APP_CC
xrdp_surface_create(struct xrdp_session* session, struct xrdp_fastpath* fastpath)
{
	struct xrdp_surface *self;

	self = (struct xrdp_surface*)g_malloc(sizeof(struct xrdp_surface), 1);
	self->session = session;
 	self->fastpath = fastpath;
	self->rfx_context = rfx_context_new();
	self->s = stream_new(16384);
	return self;
}

/*****************************************************************************/
void APP_CC
xrdp_surface_delete(struct xrdp_surface* self)
{
	STREAM* s = (STREAM*)self->s;
	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*)self->rfx_context;

	if (self == 0)
	{
		return;
	}
	free_stream(self->out_s);
	stream_free(s);
	rfx_context_free(rfx_context);
	g_free(self);
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_surface_reset(struct xrdp_surface* self)
{
	return 0;
}

int APP_CC
xrdp_surface_init(struct xrdp_surface* self)
{
	int width;
	int height;
	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*)self->rfx_context;
	
	width = self->session->client_info->width;
	height= self->session->client_info->height;

	rfx_context->mode = self->session->client_info->rfx_entropy;
	rfx_context->width = width;
	rfx_context->height= height;
	
	make_stream(self->out_s);
	init_stream(self->out_s, 2*3*width*height+22);

	return 0;
}



/*****************************************************************************/
int APP_CC
xrdp_surface_send_surface_bits(struct xrdp_surface* self,int bpp, char* data,
              int x, int y, int cx, int cy)
{
	RFX_RECT rect;
	int Bpp;
	char* buf;
	int i,j;
	uint32 bitmapDataLength;
	STREAM* s = (STREAM*)self->s;
	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*)self->rfx_context;

	if ( (bpp==24)||(bpp==32) ) 
 		Bpp = 4;
	else
	{
		g_writeln("bpp = %d is not supported\n",bpp);
		return 1;
	}

	rect.x  = 0;
	rect.y  = 0;
	rect.width  = cx;
	rect.height = cy;

	init_stream(self->out_s,0);

	stream_set_pos(s, 0);
	rfx_compose_message(rfx_context, s, &rect, 1, data, cx, cy, Bpp * cx);

	/* surface_bits_command */
	out_uint16_le	(self->out_s, CMDTYPE_STREAM_SURFACE_BITS);	/* cmdType */
	out_uint16_le	(self->out_s, x);				/* destLeft */
	out_uint16_le	(self->out_s, y);				/* destTop */
	out_uint16_le	(self->out_s, x + cx);				/* destRight */
	out_uint16_le	(self->out_s, y + cy);				/* destBottom */
	out_uint8	(self->out_s, 32);				/* bpp */
	out_uint8	(self->out_s, 0);				/* reserved1 */	
	out_uint8	(self->out_s, 0);				/* reserved2 */	
	out_uint8	(self->out_s, self->session->client_info->rfx_codecId);	/* codecId */	
	out_uint16_le	(self->out_s, cx);				/* width */
	out_uint16_le	(self->out_s, cy);				/* height */
	bitmapDataLength = stream_get_length(s);
	out_uint32_le	(self->out_s, bitmapDataLength);		/* bitmapDataLength */
	//g_writeln("xrdp_surface_send_surface_bits %d %d %d %d %d len=%d ",x,y,cx,cy,bpp,bitmapDataLength);

	/* rfx bit stream */
	out_uint8p(self->out_s, s->data, bitmapDataLength);
  
	s_mark_end(self->out_s);
	return xrdp_fastpath_send_update_pdu(self->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, self->out_s);
 
}

/*****************************************************************************/
int APP_CC
xrdp_surface_send_frame_marker(struct xrdp_surface* self,
                               uint16 frameAction,uint32 frameId)
{
	//g_writeln("xrdp_surface_send_frame_maker: Action=%d Id=%d ",frameAction,frameId);
	init_stream(self->out_s,0);
	out_uint16_le(self->out_s, CMDTYPE_FRAME_MARKER);
	out_uint16_le(self->out_s, frameAction);
	out_uint32_le(self->out_s, frameId);
	s_mark_end(self->out_s);
	return xrdp_fastpath_send_update_pdu(self->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, self->out_s);
}

