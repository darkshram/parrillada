/***************************************************************************
 *            audio-disc.h
 *
 *  dim nov 27 15:34:32 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Parrillada is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Parrillada is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef AUDIO_DISC_H
#define AUDIO_DISC_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_AUDIO_DISC         (parrillada_audio_disc_get_type ())
#define PARRILLADA_AUDIO_DISC(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_AUDIO_DISC, ParrilladaAudioDisc))
#define PARRILLADA_AUDIO_DISC_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_AUDIO_DISC, ParrilladaAudioDiscClass))
#define PARRILLADA_IS_AUDIO_DISC(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_AUDIO_DISC))
#define PARRILLADA_IS_AUDIO_DISC_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_AUDIO_DISC))
#define PARRILLADA_AUDIO_DISC_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_AUDIO_DISC, ParrilladaAudioDiscClass))

typedef struct _ParrilladaAudioDiscPrivate ParrilladaAudioDiscPrivate;

typedef struct {
	GtkBox parent;
	ParrilladaAudioDiscPrivate *priv;
} ParrilladaAudioDisc;

typedef struct {
	GtkBoxClass parent_class;
} ParrilladaAudioDiscClass;

GType parrillada_audio_disc_get_type (void);
GtkWidget *parrillada_audio_disc_new (void);

G_END_DECLS

G_END_DECLS

#endif /* AUDIO_DISC_H */
