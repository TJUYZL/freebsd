/*-
 * Copyright (c) 2003 Jake Burkholder.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/linker_set.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cache.h>
#include <machine/cpufunc.h>
#include <machine/smp.h>

PMAP_STATS_VAR(spitfire_dcache_npage_inval);
PMAP_STATS_VAR(spitfire_dcache_npage_inval_match);
PMAP_STATS_VAR(spitfire_icache_npage_inval);
PMAP_STATS_VAR(spitfire_icache_npage_inval_match);

/*
 * Flush a physical page from the data cache.
 */
void
spitfire_dcache_page_inval(vm_offset_t pa)
{
	u_long target;
	void *cookie;
	u_long addr;
	u_long tag;

	KASSERT((pa & PAGE_MASK) == 0,
	    ("dcache_page_inval: pa not page aligned"));
	PMAP_STATS_INC(spitfire_dcache_npage_inval);
	target = pa >> (PAGE_SHIFT - DC_TAG_SHIFT);
	cookie = ipi_dcache_page_inval(tl_ipi_spitfire_dcache_page_inval, pa);
	for (addr = 0; addr < cache.dc_size; addr += cache.dc_linesize) {
		tag = ldxa(addr, ASI_DCACHE_TAG);
		if (((tag >> DC_VALID_SHIFT) & DC_VALID_MASK) == 0)
			continue;
		tag &= DC_TAG_MASK << DC_TAG_SHIFT;
		if (tag == target) {
			PMAP_STATS_INC(spitfire_dcache_npage_inval_match);
			stxa_sync(addr, ASI_DCACHE_TAG, tag);
		}
	}
	ipi_wait(cookie);
}

/*
 * Flush a physical page from the instruction cache.
 */
void
spitfire_icache_page_inval(vm_offset_t pa)
{
	register u_long tag __asm("%g1");
	u_long target;
	void *cookie;
	u_long addr;

	KASSERT((pa & PAGE_MASK) == 0,
	    ("icache_page_inval: pa not page aligned"));
	PMAP_STATS_INC(spitfire_icache_npage_inval);
	target = pa >> (PAGE_SHIFT - IC_TAG_SHIFT);
	cookie = ipi_icache_page_inval(tl_ipi_spitfire_icache_page_inval, pa);
	for (addr = 0; addr < cache.ic_size; addr += cache.ic_linesize) {
		__asm __volatile("ldda [%1] %2, %%g0" /*, %g1 */
		    : "=r" (tag) : "r" (addr), "n" (ASI_ICACHE_TAG));
		if (((tag >> IC_VALID_SHIFT) & IC_VALID_MASK) == 0)
			continue;
		tag &= IC_TAG_MASK << IC_TAG_SHIFT;
		if (tag == target) {
			PMAP_STATS_INC(spitfire_icache_npage_inval_match);
			stxa_sync(addr, ASI_ICACHE_TAG, tag);
		}
	}
	ipi_wait(cookie);
}
