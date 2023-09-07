#include "DiskWriter.hpp"
extern PDEVICE_OBJECT GDeviceObject;

std::vector<PDISK_GEOMETRY> pDiskGeometries;
std::vector<PDEVICE_OBJECT> pDevicesObjects;

NTSTATUS Disk::GetAllDiskObjects()
{
	PDEVICE_EXTENSION	pDevExtn = (PDEVICE_EXTENSION)GDeviceObject->DeviceExtension;
	UNICODE_STRING	DisksByDiskSys;
	PDRIVER_OBJECT	pDiskObject;

	RtlInitUnicodeString(&DisksByDiskSys, L"\\Driver\\disk");

	if (ObReferenceObjectByName(&DisksByDiskSys, OBJ_CASE_INSENSITIVE, 0, 0,
		*IoDriverObjectType, KernelMode, 0,
		(PVOID*)&pDiskObject) >= 0)
	{
		PDEVICE_OBJECT pDeviceObjectTemp = pDiskObject->DeviceObject;
		pDevicesObjects.push_back(pDeviceObjectTemp);
		if (!pDeviceObjectTemp) { return STATUS_INSUFFICIENT_RESOURCES; }
		PDISK_GEOMETRY pDiskGeometry = (PDISK_GEOMETRY)ExAllocatePool(NonPagedPool, sizeof(DISK_GEOMETRY));
		if (!pDiskGeometry) { return STATUS_INSUFFICIENT_RESOURCES; }

		do
		{
			RtlZeroMemory(pDiskGeometry, sizeof(DISK_GEOMETRY));

			if (pDeviceObjectTemp->DeviceType == FILE_DEVICE_DISK && (pDeviceObjectTemp->Flags & DO_DEVICE_HAS_NAME))
			{
				ULONG dwRetLength; 
				POBJECT_NAME_INFORMATION pNameBuffer;

				ObQueryNameString(pDeviceObjectTemp, NULL, 0, &dwRetLength);
				if (!dwRetLength) { return STATUS_INFO_LENGTH_MISMATCH; }

				pNameBuffer = (POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(PagedPool, dwRetLength, ' sFI');
				if(!pNameBuffer) { ExFreePool(pDiskGeometry); return STATUS_INSUFFICIENT_RESOURCES; }

				if (ObQueryNameString(pDeviceObjectTemp, pNameBuffer,
					1024, &dwRetLength) == STATUS_SUCCESS && pNameBuffer->Name.Buffer)
				{
					BOOLEAN IsFound = FALSE;
					PDISK_OBJ pDisk = (PDISK_OBJ)ExAllocatePool(PagedPool, sizeof(DISK_OBJ));

					if (!pDisk) { ExFreePool(pDiskGeometry); ExFreePool(pNameBuffer); return STATUS_INSUFFICIENT_RESOURCES; }

					for (const WCHAR* pNameTemp = pNameBuffer->Name.Buffer + wcslen(pNameBuffer->Name.Buffer); pNameTemp > pNameBuffer->Name.Buffer; --pNameTemp)
					{
						if (!_wcsnicmp(pNameTemp, L"\\DR", 3))
						{
							pDisk->bIsRawDiskObj = TRUE; IsFound = TRUE;
							break;
						}
						#if _WIN32_WINNT < 0x0A00
						else if (!_wcsnicmp(pNameTemp, L"\\DP(", 4))
						{
							pDisk->bIsRawDiskObj = FALSE; IsFound = TRUE;
							break;
						}
						#else
						else if (!_wcsnicmp(pNameTemp, L"\\P", 2))
						{
							pDisk->bIsRawDiskObj = FALSE; IsFound = TRUE;
							break;
						}
						#endif
					}
					if (IsFound)
					{
						pDisk->dwDiskOrdinal = (USHORT)pNameBuffer->
							Name.Buffer[wcslen(pNameBuffer->Name.Buffer) - 1]
							- (USHORT)L'0';
						pDisk->pDiskDevObj = pDeviceObjectTemp;

						//ExInterlockedInsertTailList(&pDevExtn->list_head, &pDisk->list, &pDevExtn->list_lock);

						NTSTATUS Status = GetGeometry(pDisk->pDiskDevObj, pDiskGeometry);
						//_MEDIA_TYPE::FixedMedia
						KdPrint(("Disk: %wS\n", pNameBuffer->Name.Buffer));
						KdPrint(("SectorSize: %X\n", pDiskGeometry->BytesPerSector));
						KdPrint(("Cylinders count: %X\n", pDiskGeometry->Cylinders.QuadPart));
						KdPrint(("MediaType: %X\n", pDiskGeometry->MediaType));
						KdPrint(("SectorPerTrace: %X\n", pDiskGeometry->SectorsPerTrack));
						KdPrint(("TracksPerCylinder: %X\n", pDiskGeometry->TracksPerCylinder));

						if (!NT_SUCCESS(Status)) { pDisk->bGeometryFound = FALSE; }
						else { pDisk->bGeometryFound = TRUE; pDisk->ulSectorSize = pDiskGeometry->BytesPerSector; }		
						pDiskGeometries.push_back(pDiskGeometry);
					}
					
				}
				ExFreePoolWithTag(pNameBuffer, 0);
			}

			pDeviceObjectTemp = pDeviceObjectTemp->NextDevice;
		} while (pDeviceObjectTemp);
		
		//ExFreePool(pDiskGeometry);
	}
	
	return STATUS_SUCCESS;
}

NTSTATUS Disk::GetGeometry(_In_ PDEVICE_OBJECT pDiskDeviceObject, _Inout_ PDISK_GEOMETRY pDiskGeometry)
{
	KEVENT Event;
	
	KeInitializeEvent(&Event, NotificationEvent, FALSE);

	IO_STATUS_BLOCK IoStatusBlock;
	PIRP pIrp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_GEOMETRY,
		pDiskDeviceObject, NULL, 0, pDiskGeometry,
		sizeof(DISK_GEOMETRY), FALSE, &Event,
		&IoStatusBlock);

	if (!pIrp) { return STATUS_INSUFFICIENT_RESOURCES; }

	NTSTATUS status = IoCallDriver(pDiskDeviceObject, pIrp);

	if (status == STATUS_PENDING) { KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL); status = IoStatusBlock.Status; }
	return status;
}