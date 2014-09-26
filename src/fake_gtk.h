#ifndef _FAKE_GTK_H
#define _FAKE_GTK_H_

#if !defined(HAVE_GTK_2) && !defined(HAVE_GTK_3)

#define GtkWidget int
#define GdkEvent int
#define gpointer int *
#define gboolean void
#define gint int
#define GdkInputCondition int

#else

#include <gtk/gtk.h>

#endif /* HAVE_GTK_2/3 */

#endif /* _FAKE_GTK_H_ */
