/*
 * Copyright(c) 2011-2016 Intel Corporation. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _GVT_REG_H
#define _GVT_REG_H

#define INTEL_GVT_PCI_CLASS_VGA_OTHER   0x80

#define INTEL_GVT_PCI_GMCH_CONTROL	0x50
#define   BDW_GMCH_GMS_SHIFT		8
#define   BDW_GMCH_GMS_MASK		0xff

#define INTEL_GVT_PCI_SWSCI		0xe8
#define   SWSCI_SCI_SELECT		(1 << 15)
#define   SWSCI_SCI_TRIGGER		1

#define INTEL_GVT_PCI_OPREGION		0xfc

#define INTEL_GVT_OPREGION_CLID		0x1AC
#define INTEL_GVT_OPREGION_SCIC		0x200
#define   OPREGION_SCIC_FUNC_MASK	0x1E
#define   OPREGION_SCIC_FUNC_SHIFT	1
#define   OPREGION_SCIC_SUBFUNC_MASK	0xFF00
#define   OPREGION_SCIC_SUBFUNC_SHIFT	8
#define   OPREGION_SCIC_EXIT_MASK	0xE0
#define INTEL_GVT_OPREGION_SCIC_F_GETBIOSDATA         4
#define INTEL_GVT_OPREGION_SCIC_F_GETBIOSCALLBACKS    6
#define INTEL_GVT_OPREGION_SCIC_SF_SUPPRTEDCALLS      0
#define INTEL_GVT_OPREGION_SCIC_SF_REQEUSTEDCALLBACKS 1
#define INTEL_GVT_OPREGION_PARM                   0x204

#define INTEL_GVT_OPREGION_PAGES	2
#define INTEL_GVT_OPREGION_PORDER	1
#define INTEL_GVT_OPREGION_SIZE		(2 * 4096)

#endif
