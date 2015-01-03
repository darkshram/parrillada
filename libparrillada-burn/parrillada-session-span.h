/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _PARRILLADA_SESSION_SPAN_H_
#define _PARRILLADA_SESSION_SPAN_H_

#include <glib-object.h>

#include <parrillada-session.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_SESSION_SPAN             (parrillada_session_span_get_type ())
#define PARRILLADA_SESSION_SPAN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_SESSION_SPAN, ParrilladaSessionSpan))
#define PARRILLADA_SESSION_SPAN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_SESSION_SPAN, ParrilladaSessionSpanClass))
#define PARRILLADA_IS_SESSION_SPAN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_SESSION_SPAN))
#define PARRILLADA_IS_SESSION_SPAN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_SESSION_SPAN))
#define PARRILLADA_SESSION_SPAN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_SESSION_SPAN, ParrilladaSessionSpanClass))

typedef struct _ParrilladaSessionSpanClass ParrilladaSessionSpanClass;
typedef struct _ParrilladaSessionSpan ParrilladaSessionSpan;

struct _ParrilladaSessionSpanClass
{
	ParrilladaBurnSessionClass parent_class;
};

struct _ParrilladaSessionSpan
{
	ParrilladaBurnSession parent_instance;
};

GType parrillada_session_span_get_type (void) G_GNUC_CONST;

ParrilladaSessionSpan *
parrillada_session_span_new (void);

ParrilladaBurnResult
parrillada_session_span_again (ParrilladaSessionSpan *session);

ParrilladaBurnResult
parrillada_session_span_possible (ParrilladaSessionSpan *session);

ParrilladaBurnResult
parrillada_session_span_start (ParrilladaSessionSpan *session);

ParrilladaBurnResult
parrillada_session_span_next (ParrilladaSessionSpan *session);

goffset
parrillada_session_span_get_max_space (ParrilladaSessionSpan *session);

void
parrillada_session_span_stop (ParrilladaSessionSpan *session);

G_END_DECLS

#endif /* _PARRILLADA_SESSION_SPAN_H_ */
