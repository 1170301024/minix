
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/keymap.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>

#include <errno.h>
#include <assert.h>
#include <env.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "sanitycheck.h"

void free_proc(struct vmproc *vmp)
{
	map_free_proc(vmp);
	if(vmp->vm_flags & VMF_HASPT) {
		vmp->vm_flags &= ~VMF_HASPT;
		pt_free(&vmp->vm_pt);
	}
	region_init(&vmp->vm_regions_avl);
	vmp->vm_region_top = 0;
#if VMSTATS
	vmp->vm_bytecopies = 0;
#endif
}

void clear_proc(struct vmproc *vmp)
{
	region_init(&vmp->vm_regions_avl);
	vmp->vm_region_top = 0;
	vmp->vm_callback = NULL;	/* No pending vfs callback. */
	vmp->vm_flags = 0;		/* Clear INUSE, so slot is free. */
	vmp->vm_heap = NULL;
	vmp->vm_yielded = 0;
#if VMSTATS
	vmp->vm_bytecopies = 0;
#endif
}

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
int do_exit(message *msg)
{
	int proc;
	struct vmproc *vmp;

SANITYCHECK(SCL_FUNCTIONS);

	if(vm_isokendpt(msg->VME_ENDPOINT, &proc) != OK) {
		printf("VM: bogus endpoint VM_EXIT %d\n", msg->VME_ENDPOINT);
		return EINVAL;
	}
	vmp = &vmproc[proc];
	if(!(vmp->vm_flags & VMF_EXITING)) {
		printf("VM: unannounced VM_EXIT %d\n", msg->VME_ENDPOINT);
		return EINVAL;
	}

	if(vmp->vm_flags & VMF_HAS_DMA) {
		release_dma(vmp);
	} else if(vmp->vm_flags & VMF_HASPT) {
		/* Free pagetable and pages allocated by pt code. */
SANITYCHECK(SCL_DETAIL);
		free_proc(vmp);
SANITYCHECK(SCL_DETAIL);
	} else {
                 /* Free the data and stack segments. */ 	 
	         free_mem(vmp->vm_arch.vm_seg[D].mem_phys, 	 
	                 vmp->vm_arch.vm_seg[S].mem_vir + 	 
	                 vmp->vm_arch.vm_seg[S].mem_len - 	 
	                 vmp->vm_arch.vm_seg[D].mem_vir); 	 
	}
SANITYCHECK(SCL_DETAIL);

	/* Reset process slot fields. */
	clear_proc(vmp);

SANITYCHECK(SCL_FUNCTIONS);
	return OK;
}

/*===========================================================================*
 *				do_willexit				     *
 *===========================================================================*/
int do_willexit(message *msg)
{
	int proc;
	struct vmproc *vmp;

	if(vm_isokendpt(msg->VMWE_ENDPOINT, &proc) != OK) {
		printf("VM: bogus endpoint VM_EXITING %d\n",
			msg->VMWE_ENDPOINT);
		return EINVAL;
	}
	vmp = &vmproc[proc];

	vmp->vm_flags |= VMF_EXITING;

	return OK;
}

