/* See COPYING file for copyright and license details. */

#include "nproc.h"

#include <glib.h>

int
nproc(void)
{
	return (int)g_get_num_processors();
}
