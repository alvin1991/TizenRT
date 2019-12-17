/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * fs/vfs/fs_pwrite.c
 *
 *   Copyright (C) 2014 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <tinyara/cancelpt.h>
#include <tinyara/fs/fs.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: file_pwrite
 *
 * Description:
 *   Equivalent to the standard pwrite function except that is accepts a
 *   struct file instance instead of a file descriptor.  Currently used
 *   only by aio_write();
 *
 ****************************************************************************/

ssize_t file_pwrite(FAR struct file *filep, FAR const void *buf, size_t nbytes, off_t offset)
{
	off_t savepos;
	off_t pos;
	ssize_t ret;
	int errcode;

	/* Perform the seek to the current position.  This will not move the
	 * file pointer, but will return its current setting
	 */

	savepos = file_seek(filep, 0, SEEK_CUR);
	if (savepos == (off_t)-1) {
		/* file_seek might fail if this if the media is not seekable */

		return ERROR;
	}

	/* Then seek to the correct position in the file */

	pos = file_seek(filep, offset, SEEK_SET);
	if (pos == (off_t)-1) {
		/* This might fail is the offset is beyond the end of file */

		return ERROR;
	}

	/* Then perform the write operation */

	ret = file_write(filep, buf, nbytes);
	errcode = get_errno();

	/* Restore the file position */

	pos = file_seek(filep, savepos, SEEK_SET);
	if (pos == (off_t)-1 && ret >= 0) {
		/* This really should not fail */

		return ERROR;
	}

	set_errno(errcode);
	return ret;
}

/****************************************************************************
 * Name: pwrite
 *
 * Description:
 *   The pwrite() function performs the same action as write(), except that
 *   it writes into a given position without changing the file pointer. The
 *   first three arguments to pwrite() are the same as write() with the
 *   addition of a fourth argument offset for the desired position inside
 *   the file.
 *
 *   NOTE: This function could have been wholly implemented within libc but
 *   it is not.  Why?  Because if pwrite were implemented in libc, it would
 *   require four system calls.  If it is implemented within the kernel,
 *   only three.
 *
 * Parameters:
 *   fd       file descriptor (or socket descriptor) to write to
 *   buf      Data to write
 *   nbytes   Length of data to write
 *
 * Return:
 *   The positive non-zero number of bytes read on success, 0 on if an
 *   end-of-file condition, or -1 on failure with errno set appropriately.
 *   See write() return values
 *
 ****************************************************************************/

ssize_t pwrite(int fd, FAR const void *buf, size_t nbytes, off_t offset)
{
	FAR struct file *filep;
	ssize_t ret;

	/* pread() is a cancellation point */

	(void)enter_cancellation_point();

	/* Get the file structure corresponding to the file descriptor. */

	ret = (ssize_t)fs_getfilep(fd, &filep);
	if (ret < 0) {
		goto errout;
	}

	/* Let file_pwrite do the real work */

	ret = file_pwrite(filep, buf, nbytes, offset);
	if (ret < 0) {
		goto errout;
	}

	leave_cancellation_point();
	return ret;

errout:
	set_errno((int)-ret);
	leave_cancellation_point();
	return (ssize_t)ERROR;
}
