/*
   op_services.h - Defines objects that are available to message handlers

   Copyright (C) 2012 Paul Betts

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef _OP_SERVICES_H
#define _OP_SERVICES_H

#include "pubsub.h"

struct op_services {
	struct pubsub_ctx* pub_sub;
	gboolean* should_quit;
};

#endif
