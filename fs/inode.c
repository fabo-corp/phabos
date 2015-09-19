/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <phabos/assert.h>
#include <phabos/fs.h>
#include <phabos/utils.h>

#include <errno.h>

int inode_init(struct inode *inode)
{
    RET_IF_FAIL(inode, -EINVAL);

    memset(inode, 0, sizeof(*inode));
    atomic_init(&inode->refcount, 1);
    mutex_init(&inode->dlock);
    return 0;
}

struct inode *inode_alloc(void)
{
    struct inode *inode;
    int retval;

    inode = kmalloc(sizeof(*inode), MM_KERNEL);
    RET_IF_FAIL(inode, NULL);

    retval = inode_init(inode);
    if (retval)
        goto error;

    return inode;

error:
    kfree(inode);
    return NULL;
}

void inode_ref(struct inode *inode)
{
    RET_IF_FAIL(inode,);
    atomic_inc(&inode->refcount);
}

void inode_unref(struct inode *inode)
{
    if (!inode)
        return;

    if (atomic_dec(&inode->refcount))
        return;

    kfree(inode);
}
