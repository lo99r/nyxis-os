#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <IndustryStandard/Acpi.h>

typedef struct {
    UINTN       Version;
    CHAR16*     ID;

    // Memory
    UINTN       Memory_Size;

    // Framebuffer
    void*       Framebuffer_Base;
    UINTN       Framebuffer_Size;
    UINT32      Width;
    UINT32      Height;
    UINT32      PixelsPerScanLine;

    // ACPI
    void*       Rsdp;

} NTBLI;

EFI_STATUS HandleReboot(void) {
    Print(L"[ REBOOTING ] :: System will reboot in 3 seconds...\n");

    gBS->Stall(3000000);

    gRT->ResetSystem(
        EfiResetCold,
        EFI_SUCCESS,
        0,
        NULL
    );

    return EFI_SUCCESS;
}

EFI_STATUS HandleError(
    EFI_STATUS Status,
    CHAR16* Message
) {
    if (EFI_ERROR(Status)) {
        Print(
            L"[ BOOTFAIL ] :: %s (%r)\n",
            Message,
            Status
        );

        gBS->Stall(3000000);

        HandleReboot();

        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
) {
    EFI_STATUS Status;

    (void)SystemTable;

    //
    // GOP
    //

    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;

    Status = gBS->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID**)&Gop
    );

    if (EFI_ERROR(Status)) {
        return HandleError(
            Status,
            L"GOP Locate Failed"
        );
    }

    //
    // NTBLI Allocate
    //

    NTBLI *Info;

    Status = gBS->AllocatePool(
        EfiLoaderData,
        sizeof(NTBLI),
        (void**)&Info
    );

    if (EFI_ERROR(Status)) {
        return HandleError(
            Status,
            L"NTBLI Allocation Failed"
        );
    }

    //
    // Fill Boot Info
    //

    Info->Version           = 4;
    Info->ID                = L"Nyxis Bootloader Tamyo";

    Info->Framebuffer_Base  =
        (void*)Gop->Mode->FrameBufferBase;

    Info->Framebuffer_Size  =
        Gop->Mode->FrameBufferSize;

    Info->Width             =
        Gop->Mode->Info->HorizontalResolution;

    Info->Height            =
        Gop->Mode->Info->VerticalResolution;

    Info->PixelsPerScanLine =
        Gop->Mode->Info->PixelsPerScanLine;

    //
    // Find RSDP
    //

    Info->Rsdp = NULL;

    for (
        UINTN i = 0;
        i < gST->NumberOfTableEntries;
        i++
    ) {
        EFI_CONFIGURATION_TABLE *Table =
            &gST->ConfigurationTable[i];

        if (
            CompareGuid(
                &Table->VendorGuid,
                &gEfiAcpi20TableGuid
            ) ||

            CompareGuid(
                &Table->VendorGuid,
                &gEfiAcpi10TableGuid
            )
        ) {
            Info->Rsdp = Table->VendorTable;
            break;
        }
    }

    if (Info->Rsdp == NULL) {
        return HandleError(
            EFI_NOT_FOUND,
            L"RSDP Not Found"
        );
    }

    //
    // Memory Map
    //

    EFI_MEMORY_DESCRIPTOR *MemMap = NULL;

    UINTN MemMapSize = 0;
    UINTN MapKey;
    UINTN DescriptorSize;

    UINT32 DescriptorVersion;

    Status = gBS->GetMemoryMap(
        &MemMapSize,
        NULL,
        &MapKey,
        &DescriptorSize,
        &DescriptorVersion
    );

    if (Status != EFI_BUFFER_TOO_SMALL) {
        return HandleError(
            Status,
            L"GetMemoryMap Size Query Failed"
        );
    }

    MemMapSize += DescriptorSize * 2;

    Status = gBS->AllocatePool(
        EfiLoaderData,
        MemMapSize,
        (void**)&MemMap
    );

    if (EFI_ERROR(Status)) {
        return HandleError(
            Status,
            L"Memory Map Allocation Failed"
        );
    }

    Status = gBS->GetMemoryMap(
        &MemMapSize,
        MemMap,
        &MapKey,
        &DescriptorSize,
        &DescriptorVersion
    );

    if (EFI_ERROR(Status)) {
        gBS->FreePool(MemMap);

        return HandleError(
            Status,
            L"GetMemoryMap Failed"
        );
    }

    UINTN TotalMem = 0;

    for (
        UINTN i = 0;
        i < (MemMapSize / DescriptorSize);
        i++
    ) {
        EFI_MEMORY_DESCRIPTOR *Desc =
            (EFI_MEMORY_DESCRIPTOR*)
            ((UINT8*)MemMap + (i * DescriptorSize));

        TotalMem +=
            Desc->NumberOfPages *
            EFI_PAGE_SIZE;
    }

    Info->Memory_Size = TotalMem;

    gBS->FreePool(MemMap);

    //
    // Loaded Image
    //

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

    Status = gBS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (void**)&LoadedImage
    );

    if (EFI_ERROR(Status)) {
        return HandleError(
            Status,
            L"LIP Handle Failed"
        );
    }

    //
    // Kernel Path
    //

    EFI_DEVICE_PATH_PROTOCOL *KernelPath;

    KernelPath = FileDevicePath(
        LoadedImage->DeviceHandle,
        L"\\nyxskrnl.efi"
    );

    if (KernelPath == NULL) {
        return HandleError(
            EFI_OUT_OF_RESOURCES,
            L"Device Path Creation Failed"
        );
    }

    //
    // Load Kernel
    //

    EFI_HANDLE KernelImage;

    Status = gBS->LoadImage(
        FALSE,
        ImageHandle,
        KernelPath,
        NULL,
        0,
        &KernelImage
    );

    if (EFI_ERROR(Status)) {
        gBS->FreePool(KernelPath);

        return HandleError(
            Status,
            L"Kernel LoadImage Failed"
        );
    }

    gBS->FreePool(KernelPath);

    //
    // Pass NTBLI
    //

    EFI_LOADED_IMAGE_PROTOCOL *KernelLoadedImage;

    Status = gBS->HandleProtocol(
        KernelImage,
        &gEfiLoadedImageProtocolGuid,
        (void**)&KernelLoadedImage
    );

    if (EFI_ERROR(Status)) {
        return HandleError(
            Status,
            L"Kernel LIP Handle Failed"
        );
    }

    KernelLoadedImage->LoadOptions     = Info;
    KernelLoadedImage->LoadOptionsSize = sizeof(NTBLI);

    //
    // Start Kernel
    //

    Status = gBS->StartImage(
        KernelImage,
        NULL,
        NULL
    );

    if (EFI_ERROR(Status)) {
        return HandleError(
            Status,
            L"Kernel StartImage Failed"
        );
    }

    return EFI_SUCCESS;
}