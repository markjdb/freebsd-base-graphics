#include <linux/acpi.h>
#include <acpi/button.h>
#include <linux/pci.h>



char empty_zero_page[PAGE_SIZE] __aligned(PAGE_SIZE);


#define INVALID_ACPI_HANDLE	((acpi_handle)empty_zero_page)

extern acpi_handle acpi_lid_handle;

#define acpi_handle_warn(handle, fmt, ...)

acpi_status
AcpiGetData(acpi_handle obj_handle, acpi_object_handler handler, void **data);


extern acpi_status
AcpiEvaluateObjectTyped(acpi_handle handle,
			acpi_string pathname,
			struct acpi_object_list *external_params,
			struct acpi_buffer *return_buffer,
			acpi_object_type return_type);

extern acpi_status
AcpiEvaluateObject(acpi_handle handle,
		   acpi_string pathname,
		   struct acpi_object_list *external_params,
		   struct acpi_buffer *return_buffer);
extern acpi_status
AcpiWalkNamespace(acpi_object_type type,
		    acpi_handle start_object,
		    u32 max_depth,
		    acpi_walk_callback
		    descending_callback,
		    acpi_walk_callback
		    ascending_callback,
		    void *context,
		    void **return_value);


acpi_status
AcpiGetName(acpi_handle handle, u32 name_type, struct acpi_buffer * buffer);

acpi_status
AcpiGetHandle(acpi_handle parent,
    acpi_string pathname, acpi_handle * ret_handle);

acpi_status
AcpiGetTable(acpi_string signature, u32 instance, struct acpi_table_header **out_table);




extern const char *
AcpiFormatException(acpi_status status);

static void
acpi_util_eval_error(acpi_handle h, acpi_string p, acpi_status s)
{
#ifdef ACPI_DEBUG_OUTPUT
	char prefix[80] = {'\0'};
	struct acpi_buffer buffer = {sizeof(prefix), prefix};
	acpi_get_name(h, ACPI_FULL_PATHNAME, &buffer);
	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Evaluate [%s.%s]: %s\n",
		(char *) prefix, p, acpi_format_exception(s)));
#else
	return;
#endif
}

acpi_status
acpi_evaluate_integer(acpi_handle handle,
		      acpi_string pathname,
		      struct acpi_object_list *arguments, unsigned long long *data)
{
	acpi_status status = AE_OK;
	union acpi_object element;
	struct acpi_buffer buffer = { 0, NULL };

	if (!data)
		return AE_BAD_PARAMETER;

	buffer.length = sizeof(union acpi_object);
	buffer.pointer = &element;
	status = AcpiEvaluateObject(handle, pathname, arguments, &buffer);
	if (ACPI_FAILURE(status)) {
		acpi_util_eval_error(handle, pathname, status);
		return status;
	}

	if (element.type != ACPI_TYPE_INTEGER) {
		acpi_util_eval_error(handle, pathname, AE_BAD_DATA);
		return AE_BAD_DATA;
	}

	*data = element.integer.value;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Return value [%llu]\n", *data));

	return AE_OK;
}

acpi_status
acpi_evaluate_object(acpi_handle handle,
		     acpi_string pathname,
		     struct acpi_object_list *external_params,
		     struct acpi_buffer *return_buffer)
{

	return (AcpiEvaluateObject(handle, pathname, external_params, return_buffer));
}

acpi_status
acpi_evaluate_object_typed(acpi_handle handle,
			   acpi_string pathname,
			   struct acpi_object_list *external_params,
			   struct acpi_buffer *return_buffer,
			   acpi_object_type return_type)
{
	return (AcpiEvaluateObjectTyped(handle, pathname, external_params, return_buffer, return_type));
}

acpi_status
acpi_get_data(acpi_handle obj_handle, acpi_object_handler handler, void **data)
{

	return (AcpiGetData(obj_handle, handler, data));
}


acpi_status
acpi_walk_namespace(acpi_object_type type, acpi_handle start_object, u32 max_depth,
    acpi_walk_callback descending_callback, acpi_walk_callback ascending_callback,
    void *context, void **return_value)
{

	return (AcpiWalkNamespace(type, start_object, max_depth, descending_callback,
             ascending_callback, context, return_value));
}

acpi_status
acpi_get_name(acpi_handle handle, u32 name_type, struct acpi_buffer * buffer)
{

	return (AcpiGetName(handle, name_type, buffer));
}

acpi_status
acpi_get_handle(acpi_handle parent,
		acpi_string pathname, acpi_handle * ret_handle)
{

	return (AcpiGetHandle(parent, pathname, ret_handle));
}

acpi_status
acpi_get_table(acpi_string signature, u32 instance, struct acpi_table_header **out_table)
{

	return (AcpiGetTable(signature, instance, out_table));
}

bool
acpi_has_method(acpi_handle handle, char *name)
{
	acpi_handle tmp;

	return ACPI_SUCCESS(acpi_get_handle(handle, name, &tmp));
}

const char *
acpi_format_exception(acpi_status status)
{
	return (AcpiFormatException(status));
}

union acpi_object *
acpi_evaluate_dsm(acpi_handle handle, const u8 *uuid, int rev, int func,
		  union acpi_object *argv4)
{
	acpi_status ret;
	struct acpi_buffer buf = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object params[4];
	struct acpi_object_list input = {
		.count = 4,
		.pointer = params,
	};

	params[0].type = ACPI_TYPE_BUFFER;
	params[0].buffer.length = 16;
	params[0].buffer.pointer = (char *)uuid;
	params[1].type = ACPI_TYPE_INTEGER;
	params[1].integer.value = rev;
	params[2].type = ACPI_TYPE_INTEGER;
	params[2].integer.value = func;
	if (argv4) {
		params[3] = *argv4;
	} else {
		params[3].type = ACPI_TYPE_PACKAGE;
		params[3].package.count = 0;
		params[3].package.elements = NULL;
	}

	ret = acpi_evaluate_object(handle, "_DSM", &input, &buf);
	if (ACPI_SUCCESS(ret))
		return (union acpi_object *)buf.pointer;

	if (ret != AE_NOT_FOUND)
		acpi_handle_warn(handle,
				"failed to evaluate _DSM (0x%x)\n", ret);

	return NULL;
}

/**
 * acpi_check_dsm - check if _DSM method supports requested functions.
 * @handle: ACPI device handle
 * @uuid: UUID of requested functions, should be 16 bytes at least
 * @rev: revision number of requested functions
 * @funcs: bitmap of requested functions
 *
 * Evaluate device's _DSM method to check whether it supports requested
 * functions. Currently only support 64 functions at maximum, should be
 * enough for now.
 */
bool
acpi_check_dsm(acpi_handle handle, const u8 *uuid, int rev, u64 funcs)
{
	int i;
	u64 mask = 0;
	union acpi_object *obj;

	if (funcs == 0)
		return false;

	obj = acpi_evaluate_dsm(handle, uuid, rev, 0, NULL);
	if (!obj)
		return false;

	/* For compatibility, old BIOSes may return an integer */
	if (obj->type == ACPI_TYPE_INTEGER)
		mask = obj->integer.value;
	else if (obj->type == ACPI_TYPE_BUFFER)
		for (i = 0; i < obj->buffer.length && i < 8; i++)
			mask |= (((u64)obj->buffer.pointer[i]) << (i * 8));
	ACPI_FREE(obj);

	/*
	 * Bit 0 indicates whether there's support for any functions other than
	 * function 0 for the specified UUID and revision.
	 */
	if ((mask & 0x1) && (mask & funcs) == funcs)
		return true;

	return false;
}

int
acpi_lid_open(void)
{
	acpi_status status;
	unsigned long long state;

	if (acpi_lid_handle == NULL)
		return (-ENODEV);

	status = acpi_evaluate_integer(acpi_lid_handle, "_LID", NULL,
				       &state);
	if (ACPI_FAILURE(status))
		return (-ENODEV);

	return (state != 0);
}

void acpi_fake_objhandler(acpi_handle h, void *data);

static device_t
acpi_get_device(acpi_handle handle)
{
	void *dev = NULL;
	acpi_get_data(handle, acpi_fake_objhandler, &dev);

	return ((device_t)dev);
}

DEFINE_MUTEX(acpi_device_lock);
extern struct list_head acpi_bus_id_list;

struct acpi_device_bus_id {
	char bus_id[15];
	unsigned int instance_no;
	struct list_head node;
};

#if 0
static void
acpi_device_del(struct acpi_device *device)
{
	struct acpi_device_bus_id *acpi_device_bus_id;

	mutex_lock(&acpi_device_lock);
	if (device->parent)
		list_del(&device->node);

	list_for_each_entry(acpi_device_bus_id, &acpi_bus_id_list, node)
		if (!strcmp(acpi_device_bus_id->bus_id,
			    acpi_device_hid(device))) {
			if (acpi_device_bus_id->instance_no > 0)
				acpi_device_bus_id->instance_no--;
			else {
				list_del(&acpi_device_bus_id->node);
				kfree(acpi_device_bus_id);
			}
			break;
		}

	list_del(&device->wakeup_list);
	mutex_unlock(&acpi_device_lock);
#ifdef __notyet__
	acpi_power_add_remove_device(device, false);
#endif
	if (device->remove)
		device->remove(device);

	device_del(&device->dev);
}

acpi_status acpi_pwr_switch_consumer(acpi_handle consumer, int state);

static void
acpi_device_del_work_fn(struct work_struct *work_not_used)
{
	for (;;) {
		struct acpi_device *adev;

		mutex_lock(&acpi_device_del_lock);
		if (list_empty(&acpi_device_del_list)) {
			mutex_unlock(&acpi_device_del_lock);
			break;
		}
		adev = list_first_entry(&acpi_device_del_list,
					struct acpi_device, del_list);
		list_del(&adev->del_list);
		mutex_unlock(&acpi_device_del_lock);
		acpi_pwr_switch_consumer(adev->handle, ACPI_STATE_D3_COLD);
		acpi_device_del(adev);
		put_device(&adev->dev);
	}
}
#endif

static LINUX_LIST_HEAD(acpi_device_del_list);
static DEFINE_MUTEX(acpi_device_del_lock);

void
acpi_scan_drop_device(acpi_handle handle, void *context)
{
#if 0
	static DECLARE_WORK(work, acpi_device_del_work_fn);
#endif
	struct acpi_device *adev = context;

	mutex_lock(&acpi_device_del_lock);
#ifdef notyet
	if (list_empty(&acpi_device_del_list))
		acpi_queue_hotplug_work(&work);
#endif
	list_add_tail(&adev->del_list, &acpi_device_del_list);
	adev->handle = INVALID_ACPI_HANDLE;
	mutex_unlock(&acpi_device_del_lock);
}

int
acpi_bus_get_device(acpi_handle handle, struct acpi_device **device)
{
	acpi_status status;

	if (!device)
		return -EINVAL;

	status = acpi_get_data(handle, acpi_scan_drop_device, (void **)device);
	if (ACPI_FAILURE(status) || !*device) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "No context for object [%p]\n",
				  handle));
		return -ENODEV;
	}
	return 0;
}

struct pci_dev *
acpi_get_pci_dev(acpi_handle handle)
{
	device_t dev;
	struct pci_dev *pdev;

	if ((dev = acpi_get_device(handle)) == NULL)
		return (NULL);
	pdev = linux_bsddev_to_pci_dev(dev);

	return (pdev);
}

static acpi_status
acpi_backlight_cap_match(acpi_handle handle, u32 level, void *context,
			  void **return_value)
{
	long *cap = context;

	if (acpi_has_method(handle, "_BCM") &&
	    acpi_has_method(handle, "_BCL")) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found generic backlight "
				  "support\n"));
		*cap |= ACPI_VIDEO_BACKLIGHT;
		if (!acpi_has_method(handle, "_BQC"))
			log(LOG_WARNING, "%s: No _BQC method, "
			    "cannot determine initial brightness\n", __FUNCTION__);
		/* We have backlight support, no need to scan further */
		return AE_CTRL_TERMINATE;
	}
	return (0);
}

long
acpi_is_video_device(acpi_handle handle)
{
	long video_caps = 0;

	/* Is this device able to support video switching ? */
	if (acpi_has_method(handle, "_DOD") || acpi_has_method(handle, "_DOS"))
		video_caps |= ACPI_VIDEO_OUTPUT_SWITCHING;

	/* Is this device able to retrieve a video ROM ? */
	if (acpi_has_method(handle, "_ROM"))
		video_caps |= ACPI_VIDEO_ROM_AVAILABLE;

	/* Is this device able to configure which video head to be POSTed ? */
	if (acpi_has_method(handle, "_VPO") &&
	    acpi_has_method(handle, "_GPD") &&
	    acpi_has_method(handle, "_SPD"))
		video_caps |= ACPI_VIDEO_DEVICE_POSTING;

	/* Only check for backlight functionality if one of the above hit. */
	if (video_caps)
		AcpiWalkNamespace(ACPI_TYPE_DEVICE, handle,
				    ACPI_UINT32_MAX, acpi_backlight_cap_match, NULL,
				    &video_caps, NULL);

	return (video_caps);
}

static bool
linux_acpi_match_device_cls(const struct acpi_device_id *id, struct acpi_hardware_id *hwid)
{
	int i, msk, byte_shift;
	char buf[3];

	if (!id->cls)
		return false;

	/* Apply class-code bitmask, before checking each class-code byte */
	for (i = 1; i <= 3; i++) {
		byte_shift = 8 * (3 - i);
		msk = (id->cls_msk >> byte_shift) & 0xFF;
		if (!msk)
			continue;

		sprintf(buf, "%02x", (id->cls >> byte_shift) & msk);
		if (strncmp(buf, &hwid->id[(i - 1) * 2], 2))
			return (false);
	}
	return (true);
}

const struct acpi_device_id *
linux_acpi_match_device(struct acpi_device *device, const struct acpi_device_id *ids,
    const struct of_device_id *of_ids)
{
	const struct acpi_device_id *id;
	struct acpi_hardware_id *hwid;

	if (device == NULL || !device->status.present)
		return NULL;

	list_for_each_entry(hwid, &device->pnp.ids, list) {
		/* First, check the ACPI/PNP IDs provided by the caller. */
		for (id = ids; id->id[0] || id->cls; id++) {
			if (id->id[0] && !strcmp((char *) id->id, hwid->id))
				return id;
			else if (id->cls && linux_acpi_match_device_cls(id, hwid))
				return id;
		}

#ifdef __notyet__
		if (!strcmp(ACPI_DT_NAMESPACE_HID, hwid->id)
		    && acpi_of_match_device(device, of_ids))
			return id;
#endif		
	}
	return NULL;
}

#ifdef CONFIG_ACPI_SLEEP
static u32 acpi_target_sleep_state = ACPI_STATE_S0;

u32 acpi_target_system_state(void)
{
        return acpi_target_sleep_state;
}
#endif
