/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics Co., LTD. All Rights Reserved.
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
 * arch/arm/src/s5j/s5j_sflash.c
 *
 *   Copyright (C) 2015 Gregory Nutt. All rights reserved.
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
 * 3. Neither the name tinyara nor the names of its contributors may be
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

#include <errno.h>
#include <tinyara/irq.h>
#include <tinyara/progmem.h>

#include "up_arch.h"
#include "soc/s5jt200_memorymap.h"
#include "cache.h"
#include "s5j_qspi.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define S5J_SFLASH_MAPPED_ADDR   (CONFIG_S5J_FLASH_BASE)

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#ifdef CONFIG_MTD_PROGMEM
static uint8_t s5j_sflash_read_status(void)
{
	return getreg8(rRDSR);
}

size_t up_progmem_getaddress(size_t page)
{
	return S5J_SFLASH_MAPPED_ADDR + up_progmem_pagesize(page) * page;
}

ssize_t up_progmem_getpage(size_t addr)
{
	return (addr - S5J_SFLASH_MAPPED_ADDR) / up_progmem_pagesize(0);
}

ssize_t up_progmem_erasepage(size_t page)
{
	size_t addr;
	irqstate_t irqs;

	if (page >= up_progmem_npages())
		return -EFAULT;

	addr = up_progmem_getaddress(page);

	/* Disable IRQs while erasing sector */
	irqs = irqsave();

	s5j_qspi_disable_wp();

	/* Set sector address and then send erase command */
	putreg32(addr - S5J_SFLASH_MAPPED_ADDR, rERASE_ADDRESS);
	putreg8(0xff, rSE);

	/* Wait for the completion */
	while (s5j_sflash_read_status() & 0x1);

	s5j_qspi_enable_wp();

	/* Invalidate cache */
	arch_invalidate_dcache(addr, addr + up_progmem_pagesize(page));

	/* Restore IRQs */
	irqrestore(irqs);

	return up_progmem_pagesize(page);
}

ssize_t up_progmem_ispageerased(size_t page)
{
	size_t addr;
	size_t count;
	size_t bwritten;

	if (page >= up_progmem_npages())
		return -EFAULT;

	bwritten = 0;
	addr = up_progmem_getaddress(page);
	for (count = up_progmem_pagesize(page); count; count--) {
		if (getreg32(addr) != 0xffffff)
			bwritten++;
		addr += sizeof(int);
	}

	return bwritten * sizeof(int);
}

ssize_t up_progmem_write(size_t addr, const void *buf, size_t count)
{
	int page;
	int pagesize;
	size_t remain = count;

	page = up_progmem_getpage(addr + count);
	if (page < 0)
		return -EINVAL;

	pagesize = up_progmem_pagesize(page);

	while (remain) {
		int tmp = remain;
		irqstate_t irqs;

		if (tmp > pagesize)
			tmp = pagesize;

		/* Disable IRQs */
		irqs = irqsave();

		s5j_qspi_disable_wp();

		/* Load and write data */
		memcpy((void *)addr, buf, tmp);

		/* Flush cache */
		arch_flush_dcache(addr, addr + tmp);

		s5j_qspi_enable_wp();

		/* Restore IRQs */
		irqrestore(irqs);

		buf    += tmp;
		addr   += tmp;
		remain -= tmp;
	}

	return count;
}
#endif /* CONFIG_MTD_PROGMEM */
