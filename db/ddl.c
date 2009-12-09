/*
 * Copyright (c) 2006, 2007 Christoph Schwering <schwering@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ddl.h"
#include "constants.h"
#include "err.h"
#include "fgnkey.h"
#include "io.h"
#include "ixmngt.h"
#include "mem.h"
#include "rlmngt.h"
#include "str.h"
#include "verif.h"
#include "view.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool ddl_exec(struct ddl_stmt *stmt)
{
	assert(stmt != NULL);

	if (!ddl_stmt_verify(stmt)) {
		printf("DDL semantic verification failed\n");
		errprint();
	}
	
	switch (stmt->type) {
		case CREATE_TABLE:
			return ddl_create_table(stmt->ptr.crt_tbl);
		case DROP_TABLE:
			return ddl_drop_table(stmt->ptr.drp_tbl);
		case CREATE_VIEW:
			return ddl_create_view(stmt->ptr.crt_view);
		case DROP_VIEW:
			return ddl_drop_view(stmt->ptr.drp_view);
		case CREATE_INDEX:
			return ddl_create_index(stmt->ptr.crt_ix);
		case DROP_INDEX:
			return ddl_drop_index(stmt->ptr.drp_ix);
		default:
			return false;
	}
}

bool ddl_create_table(struct crt_tbl *crt_tbl)
{
	struct srel *rl;
	struct sattr sattrs[ATTR_MAX];
	int i;
	size_t atsize_sum;
	bool retval;

	assert(crt_tbl != NULL);
	assert(crt_tbl->tbl_name != NULL);
	assert(crt_tbl->cnt > 0);
	assert(crt_tbl->cnt <= ATTR_MAX);
	assert(crt_tbl->attr_dcls != NULL);

	for (i = 0, atsize_sum = 0; i < crt_tbl->cnt; i++) {
		struct attr_dcl *dcl;

		dcl = crt_tbl->attr_dcls[i];
		sattrs[i].at_domain = dcl->type->domain;
		strntermcpy(sattrs[i].at_name, dcl->attr_name, AT_NAME_MAX+1);
		sattrs[i].at_size = dcl->type->size;
		sattrs[i].at_indexed = dcl->primary_index ? PRIMARY
			: NOT_INDEXED;
		sattrs[i].at_offset = atsize_sum;
		atsize_sum += sattrs[i].at_size;
	}

	rl = create_relation(crt_tbl->tbl_name, sattrs, crt_tbl->cnt);

	if (rl == NULL)
		return false;

	retval = true;
	for (i = 0, atsize_sum = 0; i < crt_tbl->cnt; i++) {
		struct attr_dcl *dcl;
		struct srel *fgn_rl;
		struct sattr *attr, *fgn_attr;
		int j;

		dcl = crt_tbl->attr_dcls[i];
		attr = &rl->rl_header.hd_attrs[i];
		if (dcl->fk_tbl_name != NULL && dcl->fk_attr_name != NULL) {
			fgn_rl = open_relation(dcl->fk_tbl_name);
			for (j = 0; j < fgn_rl->rl_header.hd_atcnt; j++) {
				fgn_attr = &fgn_rl->rl_header.hd_attrs[j];
				if (!strncmp(fgn_attr->at_name,
							dcl->fk_attr_name,
							AT_NAME_MAX)) {
					retval &= create_foreign_key(fgn_rl,
							fgn_attr,
							rl, attr);
					break;
				}
			}
		}
	}
	return retval;
}

bool ddl_drop_table(struct drp_tbl *drp_tbl)
{
	assert(drp_tbl != NULL);
	assert(drp_tbl->tbl_name != NULL);

	if (!drop_relation(drp_tbl->tbl_name)) {
		ERR(E_UNLINK_RELATION_FAILED);
		return false;
	} else
		return true;
}

bool ddl_create_view(struct crt_view *crt_view)
{
	if (!create_view(crt_view->view_name, crt_view->query)) {
		ERR(E_COULD_NOT_CREATE_VIEW);
		return false;
	} else
		return true;
}

bool ddl_drop_view(struct drp_view *drp_view)
{
	if (!drop_view(drp_view->view_name)) {
		ERR(E_COULD_NOT_DROP_VIEW);
		return false;
	} else
		return true;
}

bool ddl_create_index(struct crt_ix *crt_ix)
{
	struct srel *rl;
	struct sattr *sattr;
	int i;

	assert(crt_ix != NULL);
	assert(crt_ix->tbl_name != NULL);
	assert(crt_ix->attr_name != NULL);

	rl = open_relation(crt_ix->tbl_name);
	if (rl == NULL)
		return false;

	sattr = NULL;
	for (i = 0; i < rl->rl_header.hd_atcnt; i++)
		if (!strcmp(rl->rl_header.hd_attrs[i].at_name,
					crt_ix->attr_name))
			sattr = &rl->rl_header.hd_attrs[i];
	if (sattr == NULL)
		return false;

	return create_index(rl, sattr, SECONDARY) != NULL;
}

bool ddl_drop_index(struct drp_ix *drp_ix)
{
	struct srel *rl;
	struct sattr *sattr;
	int i;

	assert(drp_ix != NULL);
	assert(drp_ix->tbl_name != NULL);
	assert(drp_ix->attr_name != NULL);

	rl = open_relation(drp_ix->tbl_name);
	if (rl == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		return false;
	}

	sattr = NULL;
	for (i = 0; i < rl->rl_header.hd_atcnt; i++)
		if (!strcmp(rl->rl_header.hd_attrs[i].at_name,
					drp_ix->attr_name))
			sattr = &rl->rl_header.hd_attrs[i];
	if (sattr == NULL) {
		ERR(E_ATTRIBUTE_NOT_FOUND);
		return false;
	}

	if (!drop_index(rl, sattr)) {
		ERR(E_UNLINK_INDEX_FAILED);
		return false;
	} else
		return true;
}

