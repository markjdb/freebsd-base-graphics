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
 *
 * Authors:
 *    Zhi Wang <zhi.a.wang@intel.com>
 *
 * Contributors:
 *    Changbin Du <changbin.du@intel.com>
 *
 */

#include <linux/firmware.h>
#include <linux/crc32.h>

#include "i915_drv.h"
#include "gvt.h"
#include "i915_pvinfo.h"

#define FIRMWARE_VERSION (0x0)

struct gvt_firmware_header {
	u64 magic;
	u32 crc32;		/* protect the data after this field */
	u32 version;
	u64 cfg_space_size;
	u64 cfg_space_offset;	/* offset in the file */
	u64 mmio_size;
	u64 mmio_offset;	/* offset in the file */
	unsigned char data[1];
};

#define RD(offset) (readl(mmio + offset.reg))
#define WR(v, offset) (writel(v, mmio + offset.reg))

static void bdw_forcewake_get(void *mmio)
{
	WR(_MASKED_BIT_DISABLE(0xffff), FORCEWAKE_MT);

	RD(ECOBUS);

	if (wait_for((RD(FORCEWAKE_ACK_HSW) & FORCEWAKE_KERNEL) == 0, 50))
		gvt_err("fail to wait forcewake idle\n");

	WR(_MASKED_BIT_ENABLE(FORCEWAKE_KERNEL), FORCEWAKE_MT);

	if (wait_for((RD(FORCEWAKE_ACK_HSW) & FORCEWAKE_KERNEL), 50))
		gvt_err("fail to wait forcewake ack\n");

	if (wait_for((RD(GEN6_GT_THREAD_STATUS_REG) &
		      GEN6_GT_THREAD_STATUS_CORE_MASK) == 0, 50))
		gvt_err("fail to wait c0 wake up\n");
}

#undef RD
#undef WR

#define dev_to_drm_minor(d) dev_get_drvdata((d))

static ssize_t
gvt_firmware_read(struct file *filp, struct kobject *kobj,
	     struct bin_attribute *attr, char *buf,
	     loff_t offset, size_t count)
{
	memcpy(buf, attr->private + offset, count);
	return count;
}

static struct bin_attribute firmware_attr = {
	.attr = {.name = "gvt_firmware", .mode = (S_IRUSR)},
	.read = gvt_firmware_read,
	.write = NULL,
	.mmap = NULL,
};

static int expose_firmware_sysfs(struct intel_gvt *gvt, void *mmio)
{
	struct intel_gvt_device_info *info = &gvt->device_info;
	struct pci_dev *pdev = gvt->dev_priv->drm.pdev;
	struct intel_gvt_mmio_info *e;
	struct gvt_firmware_header *h;
	void *firmware;
	void *p;
	unsigned long size;
	int i;
	int ret;

	size = sizeof(*h) + info->mmio_size + info->cfg_space_size - 1;
	firmware = vmalloc(size);
	if (!firmware)
		return -ENOMEM;

	h = firmware;

	h->magic = VGT_MAGIC;
	h->version = FIRMWARE_VERSION;
	h->cfg_space_size = info->cfg_space_size;
	h->cfg_space_offset = offsetof(struct gvt_firmware_header, data);
	h->mmio_size = info->mmio_size;
	h->mmio_offset = h->cfg_space_offset + h->cfg_space_size;

	p = firmware + h->cfg_space_offset;

	for (i = 0; i < h->cfg_space_size; i += 4)
		pci_read_config_dword(pdev, i, p + i);

	memcpy(gvt->firmware.cfg_space, p, info->cfg_space_size);

	p = firmware + h->mmio_offset;

	hash_for_each(gvt->mmio.mmio_info_table, i, e, node) {
		int j;

		for (j = 0; j < e->length; j += 4)
			*(u32 *)(p + e->offset + j) =
				readl(mmio + e->offset + j);
	}

	memcpy(gvt->firmware.mmio, p, info->mmio_size);

	firmware_attr.size = size;
	firmware_attr.private = firmware;

	ret = device_create_bin_file(&pdev->dev, &firmware_attr);
	if (ret) {
		vfree(firmware);
		return ret;
	}
	return 0;
}

static void clean_firmware_sysfs(struct intel_gvt *gvt)
{
	struct pci_dev *pdev = gvt->dev_priv->drm.pdev;

	device_remove_bin_file(&pdev->dev, &firmware_attr);
	vfree(firmware_attr.private);
}

/**
 * intel_gvt_free_firmware - free GVT firmware
 * @gvt: intel gvt device
 *
 */
void intel_gvt_free_firmware(struct intel_gvt *gvt)
{
	if (!gvt->firmware.firmware_loaded)
		clean_firmware_sysfs(gvt);

	kfree(gvt->firmware.cfg_space);
	kfree(gvt->firmware.mmio);
}

static int verify_firmware(struct intel_gvt *gvt,
			   const struct firmware *fw)
{
	struct intel_gvt_device_info *info = &gvt->device_info;
	struct drm_i915_private *dev_priv = gvt->dev_priv;
	struct pci_dev *pdev = dev_priv->drm.pdev;
	struct gvt_firmware_header *h;
	unsigned long id, crc32_start;
	const void *mem;
	const char *item;
	u64 file, request;

	h = (struct gvt_firmware_header *)fw->data;

	crc32_start = offsetof(struct gvt_firmware_header, crc32) + 4;
	mem = fw->data + crc32_start;

#define VERIFY(s, a, b) do { \
	item = (s); file = (u64)(a); request = (u64)(b); \
	if ((a) != (b)) \
		goto invalid_firmware; \
} while (0)

	VERIFY("magic number", h->magic, VGT_MAGIC);
	VERIFY("version", h->version, FIRMWARE_VERSION);
	VERIFY("crc32", h->crc32, crc32_le(0, mem, fw->size - crc32_start));
	VERIFY("cfg space size", h->cfg_space_size, info->cfg_space_size);
	VERIFY("mmio size", h->mmio_size, info->mmio_size);

	mem = (fw->data + h->cfg_space_offset);

	id = *(u16 *)(mem + PCI_VENDOR_ID);
	VERIFY("vender id", id, pdev->vendor);

	id = *(u16 *)(mem + PCI_DEVICE_ID);
	VERIFY("device id", id, pdev->device);

	id = *(u8 *)(mem + PCI_REVISION_ID);
	VERIFY("revision id", id, pdev->revision);

#undef VERIFY
	return 0;

invalid_firmware:
	gvt_dbg_core("Invalid firmware: %s [file] 0x%llx [request] 0x%llx\n",
		     item, file, request);
	return -EINVAL;
}

#define GVT_FIRMWARE_PATH "i915/gvt"

/**
 * intel_gvt_load_firmware - load GVT firmware
 * @gvt: intel gvt device
 *
 */
int intel_gvt_load_firmware(struct intel_gvt *gvt)
{
	struct intel_gvt_device_info *info = &gvt->device_info;
	struct drm_i915_private *dev_priv = gvt->dev_priv;
	struct pci_dev *pdev = dev_priv->drm.pdev;
	struct intel_gvt_firmware *firmware = &gvt->firmware;
	struct gvt_firmware_header *h;
	const struct firmware *fw;
	char *path;
	void *mmio, *mem;
	int ret;

	path = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!path)
		return -ENOMEM;

	mem = kmalloc(info->cfg_space_size, GFP_KERNEL);
	if (!mem) {
		kfree(path);
		return -ENOMEM;
	}

	firmware->cfg_space = mem;

	mem = kmalloc(info->mmio_size, GFP_KERNEL);
	if (!mem) {
		kfree(path);
		kfree(firmware->cfg_space);
		return -ENOMEM;
	}

	firmware->mmio = mem;

	mmio = pci_iomap(pdev, info->mmio_bar, info->mmio_size);
	if (!mmio) {
		kfree(path);
		kfree(firmware->cfg_space);
		kfree(firmware->mmio);
		return -EINVAL;
	}

	if (IS_BROADWELL(gvt->dev_priv) || IS_SKYLAKE(gvt->dev_priv))
		bdw_forcewake_get(mmio);

	sprintf(path, "%s/vid_0x%04x_did_0x%04x_rid_0x%04x.golden_hw_state",
		 GVT_FIRMWARE_PATH, pdev->vendor, pdev->device,
		 pdev->revision);

	gvt_dbg_core("request hw state firmware %s...\n", path);

	ret = request_firmware(&fw, path, &dev_priv->drm.pdev->dev);
	kfree(path);

	if (ret)
		goto expose_firmware;

	gvt_dbg_core("success.\n");

	ret = verify_firmware(gvt, fw);
	if (ret)
		goto out_free_fw;

	gvt_dbg_core("verified.\n");

	h = (struct gvt_firmware_header *)fw->data;

	memcpy(firmware->cfg_space, fw->data + h->cfg_space_offset,
	       h->cfg_space_size);
	memcpy(firmware->mmio, fw->data + h->mmio_offset,
	       h->mmio_size);

	release_firmware(fw);
	firmware->firmware_loaded = true;
	pci_iounmap(pdev, mmio);
	return 0;

out_free_fw:
	release_firmware(fw);
expose_firmware:
	expose_firmware_sysfs(gvt, mmio);
	pci_iounmap(pdev, mmio);
	return 0;
}
