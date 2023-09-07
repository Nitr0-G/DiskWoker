#include "DriverLoader/DriverLoader.hpp"

#include <Windows.h>

#include <iostream>
#include <string>

typedef struct _DISK_LOCATION {
    BOOLEAN						bIsRawDiskObj;
    DWORD						dwDiskOrdinal;
    ULONGLONG					ullSectorNum;
} DISK_LOCATION, * PDISK_LOCATION;

#define SECTOR_IO_DEVICE       0x8000

#define IOCTL_SECTOR_READ		CTL_CODE(SECTOR_IO_DEVICE, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SECTOR_WRITE		CTL_CODE(SECTOR_IO_DEVICE, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_GET_SECTOR_SIZE	CTL_CODE(SECTOR_IO_DEVICE, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

DWORD DoDeviceIoCtl(DWORD dwIoCtl, DWORD dwDiskObjOrdinal,
    BOOLEAN bIsRawDisk, ULONGLONG ullSectorNumber, PVOID* pBuf)
{
    DWORD dwStatus, Size = 512, Bytes;
    PVOID pMem = NULL;

    HANDLE hDriver = CreateFileA("\\\\.\\DiskWriter",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ |
        FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL |
        FILE_FLAG_OVERLAPPED,
        NULL);

    if (hDriver == INVALID_HANDLE_VALUE) { DWORD ErrorNum = GetLastError(); printf("[*] CreateFile failed : %d\n", ErrorNum); return false; }

    DISK_LOCATION dlInfo;
    dlInfo.bIsRawDiskObj = bIsRawDisk;
    dlInfo.dwDiskOrdinal = dwDiskObjOrdinal;
    dlInfo.ullSectorNum = ullSectorNumber;

    do
    {
        if (dwIoCtl == IOCTL_SECTOR_WRITE) { Size += sizeof(DISK_LOCATION); }

        pMem = malloc(Size);

        // A very ugly hack to transfer disk location data and input buffer both for write operations
        // Came to know about bug of write operations very late, So instead of handling mapping user address into kernel
        // address space, I did this hack, Will fix it in future
        if (!pMem) { return ERROR_NOT_ENOUGH_MEMORY; }
        RtlZeroMemory(pMem, Size);

        if (dwIoCtl == IOCTL_SECTOR_WRITE) {
            PBYTE pByte = (PBYTE)((ULONG)pMem + sizeof(DISK_LOCATION));
            int i = 0;
            printf("Please type the data you want to write to the sector (input redirection is a better option)\nInput Data: \n");
            char TempChar = 0;
            do {
                TempChar = getc(stdin);
                pByte[i] = TempChar;
                i++;
            } while ((i < Size) && (TempChar != EOF));
        }
        if (dwIoCtl == IOCTL_SECTOR_WRITE) {
            memcpy(pMem, &dlInfo, sizeof(dlInfo));
            if (DeviceIoControl(hDriver, dwIoCtl, pMem, Size, NULL, 0, &Bytes, NULL))
                break;
        }
        else {
            if (DeviceIoControl(hDriver, dwIoCtl, &dlInfo, sizeof(dlInfo), pMem, Size, &Bytes, NULL))
                break;
        }

        dwStatus = GetLastError();
        printf("DeviceIoControl Failed and error code is %d\n", dwStatus);

        if (dwIoCtl == IOCTL_SECTOR_WRITE) {
            Size -= sizeof(DISK_LOCATION);
        }
        free(pMem);
        pMem = NULL;
        Size = Size * 2;
    } while (dwStatus == ERROR_INSUFFICIENT_BUFFER);

    if (dwIoCtl == IOCTL_SECTOR_READ && pMem) 
    {
        PBYTE pByte = (PBYTE)pMem;
        int i = 0;
        char TempChar = 0;
        printf("Displaying the data read from the sector (in hexadecimal, output redirection can also work)\nOutput Data: \n");
        do {
            TempChar = pByte[i];
            //putc(c, stdout);
            printf("0x%-02X ", TempChar & 0x00FFUL);

            if (!(i + 1) % 0x10) {
                printf("\n");
            }
            i++;
        } while (i < Size);
    }

    *pBuf = pMem;
    return Size;
}

void PrintUsage()
{
    printf("Usage is:\n"
        "DiskSector {/disk | /partition} <rawdisk number | partition number> "
        "{/read | /write} <sectornumber> {/unload}\n"
        "\n{/disk | /partition} <rawdisk number | partition number>\n"
        "Disk and Parition options are mutually exclusive\n"
        "Disk numbering starts from 0 while partition starts from 1\n"
        "\n{/read | /write} <sectornumber>\n"
        "Read and Write options are mutually exclusive\n"
        "Sector numbering starts from 0\n"
        "\n{/unload} \nThis option simply unloads the support driver\n"
        "\ne.g \"DiskSector /disk 0 /read 0\" will read raw sector 0 of harddisk 0\n");
    return;
}

int main(int argc, char* argv[])
{
    std::unique_ptr<DRIVER> Driver; Driver = std::make_unique<DRIVER>();
    std::string Wait;

    Driver->LoadDriver((LPTSTR)L"C:\\DiskWriter.sys", (LPTSTR)L"DiskWriter", (LPTSTR)L"DiskWriter", SERVICE_DEMAND_START);
    std::cout << "Driver Started!" << std::endl;

    
    BOOLEAN bIsRawDisk, bReadWrite, bLoadDriver = TRUE;
    UINT32 dwDiskObjOrdinal = -1;
    ULONGLONG ullSectorNumber = -1;
    UINT32 dwSize = 512;
    PVOID pBuf = NULL;

    if (argc < 2 || strcmp(argv[1], "/?") == 0) {
        PrintUsage();
        return (0);
    }

    int argIndex = 1;

    for (argIndex = 1; argIndex < argc; argIndex++)
    {
        if (!strcmp(argv[argIndex], "/disk") || !strcmp(argv[argIndex], "/partition")) {
            if (dwDiskObjOrdinal == -1) {
                bIsRawDisk = strcmp(argv[argIndex], "/disk") ? FALSE : TRUE;
                argIndex++;
                if (argIndex < argc) {
                    char* endptr;
                    dwDiskObjOrdinal = strtoul(argv[argIndex], &endptr, 10);
                }
                else {
                    PrintUsage();
                    return -1;
                }
            }
            else {
                PrintUsage();
                return -1;
            }
        }
        else if (!strcmp(argv[argIndex], "/read") || !strcmp(argv[argIndex], "/write")) {
            if (ullSectorNumber == -1) {
                bReadWrite = strcmp(argv[argIndex], "/read") ? FALSE : TRUE;
                argIndex++;
                if (argIndex < argc) {
                    char* endptr;
                    ullSectorNumber = _strtoui64(argv[argIndex], &endptr, 10);
                }
            }
            else {
                PrintUsage();
                return -1;
            }

        }
        else if (!strcmp(argv[argIndex], "/unload")) {
            bLoadDriver = FALSE;
        }
        else {
            PrintUsage();
            return -1;
        }
    }
    //Sleep(1200);
    DoDeviceIoCtl(bReadWrite ? IOCTL_SECTOR_READ : IOCTL_SECTOR_WRITE,
        dwDiskObjOrdinal, bIsRawDisk, ullSectorNumber, &pBuf);

    std::cout << "Press any key to unload driver...";
    getline(std::cin, Wait, '\n');

    Driver->UnloadDriver();
    std::cout << "Driver unloaded!" << std::endl;

    return 0;
}