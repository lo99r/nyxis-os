#include <efi.h>
#include <efilib.h>
#include <efiprot.h>

#include "include/boot_info.h"

/* ELF32 Definitions */
#define ELF_MAGIC       0x464C457F
#define ET_EXEC         2
#define EM_386          3
#define PT_LOAD         1

typedef struct {
    UINT32 e_magic;
    UINT8 e_class;
    UINT8 e_data;
    UINT8 e_version;
    UINT8 e_osabi;
    UINT8 e_abiversion;
    UINT8 pad[7];
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

/* Read file into allocated buffer */
EFI_STATUS read_file(EFI_FILE_HANDLE file, VOID** buffer, UINTN* size) {
    EFI_STATUS Status;
    EFI_FILE_INFO* FileInfo;
    UINTN InfoSize = 0;

    /* Get file info */
    Status = uefi_call_wrapper(file->GetInfo, 4, file, &GenericFileInfo, &InfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return Status;
    }

    FileInfo = AllocatePool(InfoSize);
    if (!FileInfo) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = uefi_call_wrapper(file->GetInfo, 4, file, &GenericFileInfo, &InfoSize, FileInfo);
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

    Status = uefi_call_wrapper(file->Read, 3, file, size, *buffer);
    FreePool(FileInfo);
    return Status;
}

/* Load ELF kernel and return entry point */
EFI_STATUS load_elf_kernel(VOID* elf_buffer, UINTN elf_size, kernel_entry_t* entry_point) {
    ELF_HEADER* hdr = (ELF_HEADER*)elf_buffer;
    ELF_PROGRAM_HEADER* phdr;
    UINTN i;

    /* Validate ELF header */
    if (elf_size < sizeof(ELF_HEADER)) {
        return EFI_INVALID_PARAMETER;
    }

    if (hdr->e_magic != ELF_MAGIC) {
        Print(L"Invalid ELF magic: 0x%x\n", hdr->e_magic);
        return EFI_INVALID_PARAMETER;
    }

    if (hdr->e_machine != EM_386) {
        Print(L"Invalid architecture: 0x%x\n", hdr->e_machine);
        return EFI_INVALID_PARAMETER;
    }

    if (hdr->e_type != ET_EXEC) {
        Print(L"Not executable: 0x%x\n", hdr->e_type);
        return EFI_INVALID_PARAMETER;
    }

    /* Load PT_LOAD segments */
    for (i = 0; i < hdr->e_phnum; i++) {
        phdr = (ELF_PROGRAM_HEADER*)((UINTN)elf_buffer + hdr->e_phoff + i * hdr->e_phentsize);

        if (phdr->p_type == PT_LOAD) {
            VOID* segment_addr = (VOID*)(UINTN)phdr->p_paddr;
            VOID* segment_data = (VOID*)((UINTN)elf_buffer + phdr->p_offset);

            Print(L"Loading segment %u: 0x%x (size: 0x%x)\n", 
                  i, phdr->p_paddr, phdr->p_memsz);

            /* Zero and copy segment data */
            SetMem(segment_addr, phdr->p_memsz, 0);
            CopyMem(segment_addr, segment_data, phdr->p_filesz);
        }
    }

    *entry_point = (kernel_entry_t)(UINTN)hdr->e_entry;
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
efi_main(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE* SystemTable
) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE* LoadedImage;
    EFI_FILE_IO_INTERFACE* FileSystem;
    EFI_FILE_HANDLE RootDir, KernelFile;
    VOID* KernelBuffer = NULL;
    UINTN KernelSize = 0;
    kernel_entry_t KernelEntry = NULL;

    EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop;
    NTBLI* BootInfo;

    InitializeLib(ImageHandle, SystemTable);

    Print(L"Nyxis Bootloader v1.0\n");
    Print(L"=====================\n");

    /* Get loaded image info */
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, 
        ImageHandle, &LoadedImageProtocol, (VOID**)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get loaded image: %r\n", Status);
        return Status;
    }

    /* Get file system protocol */
    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
        LoadedImage->DeviceHandle, &FileSystemProtocol, (VOID**)&FileSystem);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get file system: %r\n", Status);
        return Status;
    }

    /* Open root directory */
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &RootDir);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open volume: %r\n", Status);
        return Status;
    }

    /* Open kernel.elf */
    Print(L"Loading kernel.elf...\n");
    Status = uefi_call_wrapper(RootDir->Open, 5, RootDir, &KernelFile, 
        L"kernel.elf", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open kernel.elf: %r\n", Status);
        uefi_call_wrapper(RootDir->Close, 1, RootDir);
        return Status;
    }

    /* Read kernel file */
    Status = read_file(KernelFile, &KernelBuffer, &KernelSize);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read kernel.elf: %r\n", Status);
        uefi_call_wrapper(KernelFile->Close, 1, KernelFile);
        uefi_call_wrapper(RootDir->Close, 1, RootDir);
        return Status;
    }

    uefi_call_wrapper(KernelFile->Close, 1, KernelFile);
    uefi_call_wrapper(RootDir->Close, 1, RootDir);

    Print(L"Kernel loaded: 0x%x bytes\n", KernelSize);

    /* Parse and load ELF */
    Status = load_elf_kernel(KernelBuffer, KernelSize, &KernelEntry);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to load kernel: %r\n", Status);
        FreePool(KernelBuffer);
        return Status;
    }

    Print(L"Kernel entry: 0x%x\n", (UINTN)KernelEntry);

    /* Initialize GOP */
    Status = uefi_call_wrapper(BS->LocateProtocol, 3,
        &GraphicsOutputProtocol, NULL, (VOID**)&Gop);
    if (EFI_ERROR(Status)) {
        Print(L"GOP not available: %r\n", Status);
        FreePool(KernelBuffer);
        return Status;
    }

    /* Prepare boot info */
    BootInfo = AllocatePages(1);
    if (!BootInfo) {
        Print(L"Failed to allocate boot info\n");
        FreePool(KernelBuffer);
        return EFI_OUT_OF_RESOURCES;
    }

    BootInfo->version = 1;
    BootInfo->framebuffer_base = (VOID*)(UINTN)Gop->Mode->FrameBufferBase;
    BootInfo->framebuffer_size = Gop->Mode->FrameBufferSize;
    BootInfo->width = Gop->Mode->Info->HorizontalResolution;
    BootInfo->height = Gop->Mode->Info->VerticalResolution;
    BootInfo->pixels_per_scan_line = Gop->Mode->Info->PixelsPerScanLine;

    Print(L"Boot info prepared\n");
    Print(L"Framebuffer: 0x%x (%u x %u)\n", 
          (UINTN)BootInfo->framebuffer_base,
          BootInfo->width,
          BootInfo->height);

    /* Exit boot services */
    Print(L"Exiting UEFI boot services...\n");
    
    Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to exit boot services: %r\n", Status);
        FreePool(KernelBuffer);
        return Status;
    }

    /* Free kernel buffer since we've loaded it into memory */
    FreePool(KernelBuffer);

    /* Jump to kernel */
    KernelEntry(BootInfo);

    /* Should not return */
    return EFI_SUCCESS;
}