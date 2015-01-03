
#ifndef __parrillada_marshal_MARSHAL_H__
#define __parrillada_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* INT:VOID (./libparrillada-marshal.list:1) */
extern void parrillada_marshal_INT__VOID (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);

/* INT:INT (./libparrillada-marshal.list:2) */
extern void parrillada_marshal_INT__INT (GClosure     *closure,
                                      GValue       *return_value,
                                      guint         n_param_values,
                                      const GValue *param_values,
                                      gpointer      invocation_hint,
                                      gpointer      marshal_data);

/* INT:STRING (./libparrillada-marshal.list:3) */
extern void parrillada_marshal_INT__STRING (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* INT:OBJECT (./libparrillada-marshal.list:4) */
extern void parrillada_marshal_INT__OBJECT (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* INT:INT,STRING (./libparrillada-marshal.list:5) */
extern void parrillada_marshal_INT__INT_STRING (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* INT:OBJECT,INT,INT (./libparrillada-marshal.list:6) */
extern void parrillada_marshal_INT__OBJECT_INT_INT (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);

/* INT:POINTER,BOOLEAN (./libparrillada-marshal.list:7) */
extern void parrillada_marshal_INT__POINTER_BOOLEAN (GClosure     *closure,
                                                  GValue       *return_value,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint,
                                                  gpointer      marshal_data);

/* BOOLEAN:VOID (./libparrillada-marshal.list:8) */
extern void parrillada_marshal_BOOLEAN__VOID (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);

/* BOOLEAN:STRING (./libparrillada-marshal.list:9) */
extern void parrillada_marshal_BOOLEAN__STRING (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* BOOLEAN:POINTER (./libparrillada-marshal.list:10) */
extern void parrillada_marshal_BOOLEAN__POINTER (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:INT,STRING (./libparrillada-marshal.list:11) */
extern void parrillada_marshal_VOID__INT_STRING (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:POINTER,STRING (./libparrillada-marshal.list:12) */
extern void parrillada_marshal_VOID__POINTER_STRING (GClosure     *closure,
                                                  GValue       *return_value,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint,
                                                  gpointer      marshal_data);

/* VOID:POINTER,POINTER (./libparrillada-marshal.list:13) */
extern void parrillada_marshal_VOID__POINTER_POINTER (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);

/* VOID:OBJECT,BOOLEAN (./libparrillada-marshal.list:14) */
extern void parrillada_marshal_VOID__OBJECT_BOOLEAN (GClosure     *closure,
                                                  GValue       *return_value,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint,
                                                  gpointer      marshal_data);

/* VOID:OBJECT,UINT (./libparrillada-marshal.list:15) */
extern void parrillada_marshal_VOID__OBJECT_UINT (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

/* VOID:BOOLEAN,BOOLEAN (./libparrillada-marshal.list:16) */
extern void parrillada_marshal_VOID__BOOLEAN_BOOLEAN (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);

/* VOID:DOUBLE,DOUBLE,LONG (./libparrillada-marshal.list:17) */
extern void parrillada_marshal_VOID__DOUBLE_DOUBLE_LONG (GClosure     *closure,
                                                      GValue       *return_value,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data);

/* VOID:POINTER,UINT,POINTER (./libparrillada-marshal.list:18) */
extern void parrillada_marshal_VOID__POINTER_UINT_POINTER (GClosure     *closure,
                                                        GValue       *return_value,
                                                        guint         n_param_values,
                                                        const GValue *param_values,
                                                        gpointer      invocation_hint,
                                                        gpointer      marshal_data);

G_END_DECLS

#endif /* __parrillada_marshal_MARSHAL_H__ */

