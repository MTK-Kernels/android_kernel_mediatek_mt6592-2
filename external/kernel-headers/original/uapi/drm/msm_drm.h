/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MSM_DRM_H__
#define __MSM_DRM_H__

#include <stddef.h>
#include <drm/drm.h>


#define MSM_PIPE_NONE        0x00
#define MSM_PIPE_2D0         0x01
#define MSM_PIPE_2D1         0x02
#define MSM_PIPE_3D0         0x10

struct drm_msm_timespec {
	int64_t tv_sec;          
	int64_t tv_nsec;         
};

#define MSM_PARAM_GPU_ID     0x01
#define MSM_PARAM_GMEM_SIZE  0x02
#define MSM_PARAM_CHIP_ID    0x03

struct drm_msm_param {
	uint32_t pipe;           
	uint32_t param;          
	uint64_t value;          
};


#define MSM_BO_SCANOUT       0x00000001     
#define MSM_BO_GPU_READONLY  0x00000002
#define MSM_BO_CACHE_MASK    0x000f0000
#define MSM_BO_CACHED        0x00010000
#define MSM_BO_WC            0x00020000
#define MSM_BO_UNCACHED      0x00040000

#define MSM_BO_FLAGS         (MSM_BO_SCANOUT | \
                              MSM_BO_GPU_READONLY | \
                              MSM_BO_CACHED | \
                              MSM_BO_WC | \
                              MSM_BO_UNCACHED)

struct drm_msm_gem_new {
	uint64_t size;           
	uint32_t flags;          
	uint32_t handle;         
};

struct drm_msm_gem_info {
	uint32_t handle;         
	uint32_t pad;
	uint64_t offset;         
};

#define MSM_PREP_READ        0x01
#define MSM_PREP_WRITE       0x02
#define MSM_PREP_NOSYNC      0x04

#define MSM_PREP_FLAGS       (MSM_PREP_READ | MSM_PREP_WRITE | MSM_PREP_NOSYNC)

struct drm_msm_gem_cpu_prep {
	uint32_t handle;         
	uint32_t op;             
	struct drm_msm_timespec timeout;   
};

struct drm_msm_gem_cpu_fini {
	uint32_t handle;         
};


/* The value written into the cmdstream is logically:
 *
 *   ((relocbuf->gpuaddr + reloc_offset) << shift) | or
 *
 * When we have GPU's w/ >32bit ptrs, it should be possible to deal
 * with this by emit'ing two reloc entries with appropriate shift
 * values.  Or a new MSM_SUBMIT_CMD_x type would also be an option.
 *
 * NOTE that reloc's must be sorted by order of increasing submit_offset,
 * otherwise EINVAL.
 */
struct drm_msm_gem_submit_reloc {
	uint32_t submit_offset;  
	uint32_t or;             
	int32_t  shift;          
	uint32_t reloc_idx;      
	uint64_t reloc_offset;   
};

#define MSM_SUBMIT_CMD_BUF             0x0001
#define MSM_SUBMIT_CMD_IB_TARGET_BUF   0x0002
#define MSM_SUBMIT_CMD_CTX_RESTORE_BUF 0x0003
struct drm_msm_gem_submit_cmd {
	uint32_t type;           
	uint32_t submit_idx;     
	uint32_t submit_offset;  
	uint32_t size;           
	uint32_t pad;
	uint32_t nr_relocs;      
	uint64_t __user relocs;  
};

#define MSM_SUBMIT_BO_READ             0x0001
#define MSM_SUBMIT_BO_WRITE            0x0002

#define MSM_SUBMIT_BO_FLAGS            (MSM_SUBMIT_BO_READ | MSM_SUBMIT_BO_WRITE)

struct drm_msm_gem_submit_bo {
	uint32_t flags;          
	uint32_t handle;         
	uint64_t presumed;       
};

struct drm_msm_gem_submit {
	uint32_t pipe;           
	uint32_t fence;          
	uint32_t nr_bos;         
	uint32_t nr_cmds;        
	uint64_t __user bos;     
	uint64_t __user cmds;    
};

struct drm_msm_wait_fence {
	uint32_t fence;          
	uint32_t pad;
	struct drm_msm_timespec timeout;   
};

#define DRM_MSM_GET_PARAM              0x00
#define DRM_MSM_GEM_NEW                0x02
#define DRM_MSM_GEM_INFO               0x03
#define DRM_MSM_GEM_CPU_PREP           0x04
#define DRM_MSM_GEM_CPU_FINI           0x05
#define DRM_MSM_GEM_SUBMIT             0x06
#define DRM_MSM_WAIT_FENCE             0x07
#define DRM_MSM_NUM_IOCTLS             0x08

#define DRM_IOCTL_MSM_GET_PARAM        DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GET_PARAM, struct drm_msm_param)
#define DRM_IOCTL_MSM_GEM_NEW          DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_NEW, struct drm_msm_gem_new)
#define DRM_IOCTL_MSM_GEM_INFO         DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_INFO, struct drm_msm_gem_info)
#define DRM_IOCTL_MSM_GEM_CPU_PREP     DRM_IOW (DRM_COMMAND_BASE + DRM_MSM_GEM_CPU_PREP, struct drm_msm_gem_cpu_prep)
#define DRM_IOCTL_MSM_GEM_CPU_FINI     DRM_IOW (DRM_COMMAND_BASE + DRM_MSM_GEM_CPU_FINI, struct drm_msm_gem_cpu_fini)
#define DRM_IOCTL_MSM_GEM_SUBMIT       DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_SUBMIT, struct drm_msm_gem_submit)
#define DRM_IOCTL_MSM_WAIT_FENCE       DRM_IOW (DRM_COMMAND_BASE + DRM_MSM_WAIT_FENCE, struct drm_msm_wait_fence)

#endif 
