#include "DiskWriter.hpp"

extern std::vector<PDISK_GEOMETRY> pDiskGeometries;
extern std::vector<PDEVICE_OBJECT> pDevicesObjects;

PDEVICE_OBJECT GDeviceObject = NULL;
UNICODE_STRING DriverName, DosDeviceName;

void DrvUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	KdPrint(("DriverWriter Unload called\n"));

	IoDeleteSymbolicLink(&DosDeviceName);
	IoDeleteDevice(GDeviceObject);
	return;
}

NTSTATUS DrvClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DrvUnsupported(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DrvCreate(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	KdPrint(("DrvCreate\n"));
	
	PDEVICE_EXTENSION pDevExtn = NULL;
	Disk objDisk;
	UNREFERENCED_PARAMETER(DeviceObject);

	pDevExtn = (PDEVICE_EXTENSION)GDeviceObject->DeviceExtension;

	//InitializeListHead(&pDevExtn->list_head);
	//KeInitializeSpinLock(&pDevExtn->list_lock);

	NTSTATUS Status = objDisk.GetAllDiskObjects();
	KdPrint(("After GetAllDiskObjects %X\n", Status));
	if (!NT_SUCCESS(Status)) { IoDeleteSymbolicLink(&DosDeviceName); IoDeleteDevice(GDeviceObject); return Status; }
	
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DrvDispatchIoControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	KdPrint(("DrvDispatchIoControl Entry\n"));
	NTSTATUS Status = STATUS_SUCCESS;
	
	UNREFERENCED_PARAMETER(DeviceObject);

	PDISK_LOCATION pDiskLoc = (PDISK_LOCATION)Irp->AssociatedIrp.SystemBuffer;

	KdPrint(("DrvDispatchIoControl 142 %X\n", Status));
	//if (pList == &pDevExtn->list_head) { Status = STATUS_DEVICE_NOT_CONNECTED; Irp->IoStatus.Status = Status; return Status; }
	KdPrint(("DrvDispatchIoControl 144 %X\n", Status));
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);

	ULONG BuffSize = IrpStack->Parameters.DeviceIoControl.OutputBufferLength; //For obtaining the sector size
	ULONG OutputBuffLen = (pDiskGeometries[0]->BytesPerSector); // By default output size is sector size
	ULONG InputBuffLen = sizeof(DISK_LOCATION);
	PVOID pBuff = Irp->AssociatedIrp.SystemBuffer;

	ULONG IoCtl = IrpStack->Parameters.DeviceIoControl.IoControlCode;
	KdPrint(("Size: %X\n", pDiskGeometries.size()));
	
	switch (IoCtl)
	{
	case IOCTL_GET_SECTOR_SIZE:
		if (BuffSize >= sizeof(ULONG)) { *(PULONG)pBuff = pDiskGeometries[0]->BytesPerSector; Irp->IoStatus.Information = sizeof(ULONG); }
		else { Status = STATUS_INFO_LENGTH_MISMATCH; }
		break;
	case IOCTL_SECTOR_WRITE:
		pBuff = reinterpret_cast<void*>(reinterpret_cast<unsigned long>(pBuff) + sizeof(DISK_LOCATION));
		InputBuffLen += pDiskGeometries[0]->BytesPerSector;
		OutputBuffLen = 0;
		//Break;
	case IOCTL_SECTOR_READ:
		if (InputBuffLen > IrpStack->Parameters.DeviceIoControl.InputBufferLength)
		{
			Status = STATUS_INFO_LENGTH_MISMATCH; break;
		}
		if (OutputBuffLen > IrpStack->Parameters.DeviceIoControl.OutputBufferLength)
		{
			Status = STATUS_INFO_LENGTH_MISMATCH; break;
		}

		ULONG MajorFunc = (IoCtl == IOCTL_SECTOR_READ) ? IRP_MJ_READ : IRP_MJ_WRITE;

		LARGE_INTEGER lDiskOffset; KEVENT Event; IO_STATUS_BLOCK IoStatusBlock;

		lDiskOffset.QuadPart = (pDiskGeometries[0]->BytesPerSector) * (pDiskLoc->ullSectorNum);
		KeInitializeEvent(&Event, NotificationEvent, FALSE);

		PIRP pIrp = IoBuildSynchronousFsdRequest(MajorFunc, pDevicesObjects[0], pBuff,
			pDiskGeometries[0]->BytesPerSector, &lDiskOffset,
			&Event, &IoStatusBlock);

		if (!pIrp) { Status = STATUS_INSUFFICIENT_RESOURCES; break; }

		Status = IoCallDriver(pDevicesObjects[0], pIrp);

		if (Status == STATUS_PENDING)
		{
			KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
			Status = IoStatusBlock.Status;

			if (NT_SUCCESS(Status)) { Irp->IoStatus.Information = pDiskGeometries[0]->BytesPerSector; }
		}
		break;
	}
	
	//EXIT:
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Status;
}

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegisterPath)
{
	KdPrint(("DriverEntry\n"));

	UNREFERENCED_PARAMETER(RegisterPath);
	NTSTATUS Ntstatus = STATUS_SUCCESS;

	RtlInitUnicodeString(&DriverName, L"\\Device\\DiskWriter");
	RtlInitUnicodeString(&DosDeviceName, L"\\DosDevices\\DiskWriter");

	Ntstatus = IoCreateDevice(DriverObject, 0, &DriverName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &GDeviceObject);

	if (Ntstatus == STATUS_SUCCESS)
	{
		for (UINT64 Index = 0; Index < IRP_MJ_MAXIMUM_FUNCTION; Index++) { DriverObject->MajorFunction[Index] = DrvUnsupported; }

		DriverObject->MajorFunction[IRP_MJ_CLOSE] = DrvClose;
		DriverObject->MajorFunction[IRP_MJ_CREATE] = DrvCreate;
		DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DrvDispatchIoControl;
		DriverObject->DriverUnload = DrvUnload;
		IoCreateSymbolicLink(&DosDeviceName, &DriverName);
	}

	return STATUS_SUCCESS;
}