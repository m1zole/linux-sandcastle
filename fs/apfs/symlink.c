// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Ernesto A. Fernández <ernesto.mnd.fernandez@gmail.com>
 */

#include <linux/fs.h>
#include <linux/slab.h>
#include "apfs.h"

/**
 * apfs_get_link - Follow a symbolic link
 * @dentry:	dentry for the link
 * @inode:	inode for the link
 * @done:	delayed call to free the returned buffer after use
 *
 * Returns a pointer to a buffer containing the target path, or an appropriate
 * error pointer in case of failure.
 */
static const char *apfs_get_link(struct dentry *dentry, struct inode *inode,
				 struct delayed_call *done)
{
	struct super_block *sb = inode->i_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	char *target = NULL;
	int err;
	int size;

	down_read(&sbi->s_big_sem);

	if (!dentry) {
		err = -ECHILD;
		goto fail;
	}

	size = __apfs_xattr_get(inode, APFS_XATTR_NAME_SYMLINK,
				NULL /* buffer */, 0 /* size */);
	if (size < 0) { /* TODO: return a better error code */
		err = size;
		goto fail;
	}

	target = kmalloc(size, GFP_KERNEL);
	if (!target) {
		err = -ENOMEM;
		goto fail;
	}

	size = __apfs_xattr_get(inode, APFS_XATTR_NAME_SYMLINK, target, size);
	if (size < 0) {
		err = size;
		goto fail;
	}
	if (size == 0 || *(target + size - 1) != 0) {
		/* Target path must be NULL-terminated */
		apfs_alert(sb, "bad link target in inode 0x%llx",
			   apfs_ino(inode));
		err = -EFSCORRUPTED;
		goto fail;
	}

	up_read(&sbi->s_big_sem);
	set_delayed_call(done, kfree_link, target);
	return target;

fail:
	kfree(target);
	up_read(&sbi->s_big_sem);
	return ERR_PTR(err);
}

const struct inode_operations apfs_symlink_inode_operations = {
	.get_link	= apfs_get_link,
	.getattr	= apfs_getattr,
	.listxattr	= apfs_listxattr,
};
