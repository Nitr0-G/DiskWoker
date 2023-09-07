#pragma once
#include <ntddk.h>
#include <ntdddisk.h>
#include <ntifs.h>

#include <vector>

#define SECTOR_IO_DEVICE       0x8000

#define IOCTL_SECTOR_READ		CTL_CODE(SECTOR_IO_DEVICE, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SECTOR_WRITE		CTL_CODE(SECTOR_IO_DEVICE, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_GET_SECTOR_SIZE	CTL_CODE(SECTOR_IO_DEVICE, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


extern "C" __declspec(dllimport) POBJECT_TYPE* IoDriverObjectType;

extern "C" __declspec(dllimport) NTSTATUS ObReferenceObjectByName(
    _In_ PUNICODE_STRING ObjectPath,
    _In_ ULONG Attributes,
    _In_ PACCESS_STATE PassedAccessState,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_TYPE ObjectType,
    _In_ KPROCESSOR_MODE AccessMode,
    _Inout_ PVOID ParseContext,
    _Out_ PVOID* ObjectPtr);

class Driver;

class Disk {
private:
    friend class Driver;
    NTSTATUS GetGeometry(_In_ PDEVICE_OBJECT pDiskDeviceObject, _Inout_ PDISK_GEOMETRY pDiskGeometry);
public:
    NTSTATUS GetAllDiskObjects();
};

#pragma pack (push, 1)

extern "C" typedef struct _DISK_OBJ {
    LIST_ENTRY					list;
    BOOLEAN						bIsRawDiskObj;
    BOOLEAN						bGeometryFound;
    UINT32						dwDiskOrdinal;	// If bIsRawDiskObj = TRUE Disk Number is Raw Disk Number else it is Partition Number
    ULONG						ulSectorSize;	// Sector Size on disk
    PDEVICE_OBJECT				pDiskDevObj;	// Pointer to Device Object
} DISK_OBJ, * PDISK_OBJ;

extern "C" typedef struct _DISK_LOCATION {
    BOOLEAN						bIsRawDiskObj;
    UINT32						dwDiskOrdinal;
    ULONGLONG					ullSectorNum;
} DISK_LOCATION, * PDISK_LOCATION;

extern "C" typedef struct _DEVICE_EXTENSION {
    LIST_ENTRY                  list_head;
    KSPIN_LOCK                  list_lock;
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

#pragma pack (pop)