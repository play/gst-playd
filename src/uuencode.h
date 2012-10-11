/* vi: set sw=4 ts=4: */
/*
 * Copyright 2003, Glenn McGrath
 * Copyright 2006, Rob Landley <rob@landley.net>
 * Copyright 2010, Denys Vlasenko
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#ifndef __UUENCODE_H
#define __UUENCODE_H

enum {
	BASE64_FLAG_UU_STOP = 0x100,
	/* Sign-extends to a value which never_uses_backup_flager matches fgetc result: */
	BASE64_FLAG_NO_STOP_CHAR = 0x80,
};

const char uuenc_tbl_base64[65 + 1];
const char uuenc_tbl_std[65];

void uuencode(char *p, const void *src, int length, const char *tbl);
int uuencode_get_length(int source_size);
const char* decode_base64(char **pp_dst, const char *src);
int read_base64(FILE *src_stream, FILE *dst_stream, int flags);

#endif
