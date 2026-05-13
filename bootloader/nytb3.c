#include <efi.h>
#include <efilib.h>
#include <efiprot.h>

#include "boot_info.h"

/* =========================================================
 * ELF32 DEFINITIONS
 * ========================================================= */

#define ELF_MAGIC 0x464C457F

#define ET_EXEC 2
#define EM_386  3

#define PT_LOAD 1

#define PAGE_SIZE 4096

typedef struct {
    UINT32 e_magic;

    UINT8  e_class;
    UINT8  e_data;
    UINT8  e_version;
    UINT8  e_osabi;
    UINT8  e_abiversion;
    UINT8  pad[7];

    UINT16 e_type;
    UINT16 e_machine;

    UINT32 e_version2;
    UINT32 e_entry;

    UINT32 e_phoff;
    UINT32 e_shoff;

    UINT32 e_flags;

    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;

    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;

} ELF_HEADER;

typedef struct {
    UINT32 p_type;

    UINT32 p_offset;

    UINT32 p_vaddr;
    UINT32 p_paddr;

    UINT32 p_filesz;
    UINT32 p_memsz;

    UINT32 p_flags;
    UINT32 p_align;

} ELF_PROGRAM_HEADER;

typedef void (*kernel_entry_t)(NTBLI*);

/* =========================================================
 * READ FILE
 * ========================================================= */

EFI_STATUS
read_file(
    EFI_FILE_HANDLE file,
    VOID** buffer,
    UINTN* size
) {
    EFI_STATUS Status;

    EFI_FILE_INFO* FileInfo;

    UINTN InfoSize = 0;

    Status = uefi_call_wrapper(
        file->GetInfo,
        4,
        file,
        &GenericFileInfo,
        &InfoSize,
        NULL
    );

    if (Status != EFI_BUFFER_TOO_SMALL) {
        return Status;
    }

    FileInfo = AllocatePool(InfoSize);

    if (!FileInfo) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = uefi_call_wrapper(
        file->GetInfo,
        4,
        file,
        &GenericFileInfo,
        &InfoSize,
        FileInfo
    );

    if (EFI_ERROR(Status)) {
        FreePool(FileInfo);
        return Status;
    }

    *size = FileInfo->FileSize;

    *buffer = AllocatePool(*size);

    if (!*buffer) {
        FreePool(FileInfo);
        return EFI_OUT_OF_RESOURCES;
    }

    Status = uefi_call_wrapper(
        file->Read,
        3,
        file,
        size,
        *buffer
    );

    FreePool(FileInfo);

    return Status;
}

/* =========================================================
 * LOAD ELF
 * ========================================================= */

EFI_STATUS
load_elf_kernel(
    VOID* elf_buffer,
    UINTN elf_size,
    kernel_entry_t* entry_point
) {
    ELF_HEADER* hdr;
    ELF_PROGRAM_HEADER* phdr;

    UINTN i;

    hdr = (ELF_HEADER*)elf_buffer;

    /* -----------------------------
     * Validate ELF
     * ----------------------------- */

    if (elf_size < sizeof(ELF_HEADER)) {
        return EFI_INVALID_PARAMETER;
    }

    if (hdr->e_magic != ELF_MAGIC) {
        Print(L"Invalid ELF magic\n");
        return EFI_INVALID_PARAMETER;
    }

    if (hdr->e_machine != EM_386) {
        Print(L"Invalid architecture\n");
        return EFI_INVALID_PARAMETER;
    }

    if (hdr->e_type != ET_EXEC) {
        Print(L"ELF is not executable\n");
        return EFI_INVALID_PARAMETER;
    }

    /* -----------------------------
     * Load Segments
     * ----------------------------- */

    for (i = 0; i < hdr->e_phnum; i++) {

        phdr = (ELF_PROGRAM_HEADER*)(
            (UINTN)elf_buffer +
            hdr->e_phoff +
            (i * hdr->e_phentsize)
        );

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        EFI_PHYSICAL_ADDRESS SegmentAddr;
        UINTN Pages;

        VOID* SegmentData;

        EFI_STATUS Status;

        SegmentAddr = phdr->p_paddr;

        Pages = (phdr->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;

        Print(
            L"Loading segment %u -> 0x%x (%u bytes)\n",
            i,
            SegmentAddr,
            phdr->p_memsz
        );

        /* Allocate pages at exact physical address */

        Status = uefi_call_wrapper(
            BS->AllocatePages,
            4,
            AllocateAddress,
            EfiLoaderData,
            Pages,
            &SegmentAddr
        );

        if (EFI_ERROR(Status)) {
            Print(L"AllocatePages failed: %r\n", Status);
            return Status;
        }

        SegmentData = (VOID*)((UINTN)elf_buffer + phdr->p_offset);

        /* Zero segment */

        SetMem(
            (VOID*)(UINTN)SegmentAddr,
            phdr->p_memsz,
            0
        );

        /* Copy ELF data */

        CopyMem(
            (VOID*)(UINTN)SegmentAddr,
            SegmentData,
            phdr->p_filesz
        );
    }

    *entry_point = (kernel_entry_t)(UINTN)hdr->e_entry;

    return EFI_SUCCESS;
}

/* =========================================================
 * EFI MAIN
 * ========================================================= */

EFI_STATUS
EFIAPI
efi_main(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE* SystemTable
) {
    EFI_STATUS Status;

    EFI_LOADED_IMAGE* LoadedImage;

    EFI_FILE_IO_INTERFACE* FileSystem;

    EFI_FILE_HANDLE RootDir;
    EFI_FILE_HANDLE KernelFile;

    VOID* KernelBuffer = NULL;

    UINTN KernelSize = 0;

    kernel_entry_t KernelEntry = NULL;

    EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop;

    NTBLI* BootInfo;

    /* Memory map */

    UINTN MemoryMapSize = 0;

    EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;

    UINTN MapKey;

    UINTN DescriptorSize;

    UINT32 DescriptorVersion;

    InitializeLib(ImageHandle, SystemTable);

    Print(L"Nyxis Bootloader v1.0\n");
    Print(L"=====================\n");

    /* =====================================================
     * Loaded image
     * ===================================================== */

    Status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        ImageHandle,
        &LoadedImageProtocol,
        (VOID**)&LoadedImage
    );

    if (EFI_ERROR(Status)) {
        Print(L"HandleProtocol failed: %r\n", Status);
        return Status;
    }

    /* =====================================================
     * File system
     * ===================================================== */

    Status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        LoadedImage->DeviceHandle,
        &FileSystemProtocol,
        (VOID**)&FileSystem
    );

    if (EFI_ERROR(Status)) {
        Print(L"FileSystemProtocol failed: %r\n", Status);
        return Status;
    }

    /* =====================================================
     * Open volume
     * ===================================================== */

    Status = uefi_call_wrapper(
        FileSystem->OpenVolume,
        2,
        FileSystem,
        &RootDir
    );

    if (EFI_ERROR(Status)) {
        Print(L"OpenVolume failed: %r\n", Status);
        return Status;
    }

    /* =====================================================
     * Open kernel
     * ===================================================== */

    Print(L"Opening kernel.elf...\n");

    Status = uefi_call_wrapper(
        RootDir->Open,
        5,
        RootDir,
        &KernelFile,
        L"kernel.elf",
        EFI_FILE_MODE_READ,
        0
    );

    if (EFI_ERROR(Status)) {
        Print(L"Failed to open kernel.elf: %r\n", Status);
        return Status;
    }

    /* =====================================================
     * Read kernel
     * ===================================================== */

    Status = read_file(
        KernelFile,
        &KernelBuffer,
        &KernelSize
    );

    if (EFI_ERROR(Status)) {
        Print(L"Failed to read kernel: %r\n", Status);
        return Status;
    }

    uefi_call_wrapper(KernelFile->Close, 1, KernelFile);

    uefi_call_wrapper(RootDir->Close, 1, RootDir);

    Print(L"Kernel size: %u bytes\n", KernelSize);

    /* =====================================================
     * Load ELF
     * ===================================================== */

    Status = load_elf_kernel(
        KernelBuffer,
        KernelSize,
        &KernelEntry
    );

    if (EFI_ERROR(Status)) {
        Print(L"ELF loading failed: %r\n", Status);
        FreePool(KernelBuffer);
        return Status;
    }

    Print(L"Kernel entry: 0x%x\n", (UINTN)KernelEntry);

    /* =====================================================
     * GOP
     * ===================================================== */

    Status = uefi_call_wrapper(
        BS->LocateProtocol,
        3,
        &GraphicsOutputProtocol,
        NULL,
        (VOID**)&Gop
    );

    if (EFI_ERROR(Status)) {
        Print(L"GOP failed: %r\n", Status);
        FreePool(KernelBuffer);
        return Status;
    }

    /* =====================================================
     * Boot info
     * ===================================================== */

    BootInfo = AllocatePool(sizeof(NTBLI));

    if (!BootInfo) {
        Print(L"BootInfo allocation failed\n");
        FreePool(KernelBuffer);
        return EFI_OUT_OF_RESOURCES;
    }

    BootInfo->version = 1;

    BootInfo->framebuffer_base =
        (VOID*)(UINTN)Gop->Mode->FrameBufferBase;

    BootInfo->framebuffer_size =
        Gop->Mode->FrameBufferSize;

    BootInfo->width =
        Gop->Mode->Info->HorizontalResolution;

    BootInfo->height =
        Gop->Mode->Info->VerticalResolution;

    BootInfo->pixels_per_scan_line =
        Gop->Mode->Info->PixelsPerScanLine;

    Print(L"Framebuffer: %ux%u\n",
        BootInfo->width,
        BootInfo->height
    );

    /* =====================================================
     * Free temp kernel buffer BEFORE ExitBootServices
     * ===================================================== */

    FreePool(KernelBuffer);

    /* =====================================================
     * Get Memory Map
     * ===================================================== */

    Status = uefi_call_wrapper(
        BS->GetMemoryMap,
        5,
        &MemoryMapSize,
        NULL,
        &MapKey,
        &DescriptorSize,
        &DescriptorVersion
    );

    if (Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"GetMemoryMap size failed: %r\n", Status);
        return Status;
    }

    MemoryMapSize += DescriptorSize * 8;

    Status = uefi_call_wrapper(
        BS->AllocatePool,
        3,
        EfiLoaderData,
        MemoryMapSize,
        (VOID**)&MemoryMap
    );

    if (EFI_ERROR(Status)) {
        Print(L"MemoryMap allocation failed\n");
        return Status;
    }

    Status = uefi_call_wrapper(
        BS->GetMemoryMap,
        5,
        &MemoryMapSize,
        MemoryMap,
        &MapKey,
        &DescriptorSize,
        &DescriptorVersion
    );

    if (EFI_ERROR(Status)) {
        Print(L"GetMemoryMap failed: %r\n", Status);
        return Status;
    }

    /* =====================================================
     * Exit Boot Services
     * ===================================================== */

    Status = uefi_call_wrapper(
        BS->ExitBootServices,
        2,
        ImageHandle,
        MapKey
    );

    if (EFI_ERROR(Status)) {
        return Status;
    }

    /* =====================================================
     * JUMP TO KERNEL
     * ===================================================== */

    KernelEntry(BootInfo);

    /* Should never return */

    for (;;) {
        __asm__ __volatile__("cli");
        __asm__ __volatile__("hlt");
    }

    return EFI_SUCCESS;
}
