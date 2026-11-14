#include <efi.h>
#include <efilib.h>

#define NOTRE_BOOT_VERSION L"0.0.1"
#define KERNEL_PATH L"\\vmlinuz.efi"
#define CONFIG_PATH L"\\notreos.cfg"
#define OPTIONS_CAPACITY 512

static VOID append_option(CHAR16 *out, UINTN *used, CHAR16 c)
{
    if (*used + 1 < OPTIONS_CAPACITY) {
        out[*used] = c;
        ++(*used);
        out[*used] = L'\0';
    }
}

static VOID default_options(CHAR16 *out, UINTN *used)
{
    static const CHAR16 defaults[] =
        L"console=tty0 console=ttyS0,115200n8 notreos.live=1";
    UINTN i;
    *used = 0;
    out[0] = L'\0';
    for (i = 0; defaults[i] != L'\0'; ++i)
        append_option(out, used, defaults[i]);
}

static VOID read_config(EFI_HANDLE device, CHAR16 *out, UINTN *used)
{
    EFI_STATUS status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_HANDLE root = NULL;
    EFI_FILE_HANDLE file = NULL;
    CHAR8 raw[OPTIONS_CAPACITY];
    UINTN raw_size = sizeof(raw);
    UINTN i;
    BOOLEAN comment = FALSE;
    UINTN content_start;

    default_options(out, used);
    status = uefi_call_wrapper(BS->HandleProtocol, 3, device,
                               &FileSystemProtocol, (VOID **)&fs);
    if (EFI_ERROR(status) || fs == NULL)
        return;
    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status) || root == NULL)
        return;
    status = uefi_call_wrapper(root->Open, 5, root, &file, CONFIG_PATH,
                               EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status) || file == NULL) {
        uefi_call_wrapper(root->Close, 1, root);
        return;
    }
    status = uefi_call_wrapper(file->Read, 3, file, &raw_size, raw);
    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(root->Close, 1, root);
    if (EFI_ERROR(status) || raw_size == 0)
        return;

    out[0] = L'\0';
    *used = 0;
    content_start = 0;
    for (i = 0; i < raw_size; ++i) {
        CHAR8 c = raw[i];
        if (c == '#') {
            comment = TRUE;
            continue;
        }
        if (c == '\n' || c == '\r') {
            comment = FALSE;
            if (*used != 0 && out[*used - 1] != L' ')
                append_option(out, used, L' ');
            continue;
        }
        if (comment)
            continue;
        if ((UINT8)c < 0x20)
            continue;
        append_option(out, used, (CHAR16)(UINT8)c);
    }
    while (*used > 0 && out[*used - 1] == L' ')
        out[--(*used)] = L'\0';
    for (content_start = 0; content_start < *used && out[content_start] == L' '; ++content_start)
        ;
    if (content_start != 0) {
        UINTN remaining = *used - content_start;
        for (i = 0; i < remaining; ++i)
            out[i] = out[i + content_start];
        out[remaining] = L'\0';
        *used = remaining;
    }
    if (*used == 0)
        default_options(out, used);
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE *self = NULL;
    EFI_LOADED_IMAGE *kernel_image = NULL;
    EFI_DEVICE_PATH *kernel_path;
    EFI_HANDLE kernel = NULL;
    CHAR16 options[OPTIONS_CAPACITY];
    UINTN options_chars = 0;

    InitializeLib(image, system_table);
    Print(L"NotreBoot " NOTRE_BOOT_VERSION L"\r\n");
    Print(L"Loading the NotreOS Linux kernel...\r\n");

    status = uefi_call_wrapper(BS->HandleProtocol, 3, image,
                               &LoadedImageProtocol, (VOID **)&self);
    if (EFI_ERROR(status)) {
        Print(L"NotreBoot: LoadedImage protocol lookup failed: %r\r\n", status);
        return status;
    }
    if (self == NULL) {
        Print(L"NotreBoot: LoadedImage protocol returned NULL.\r\n");
        return EFI_LOAD_ERROR;
    }

    kernel_path = FileDevicePath(self->DeviceHandle, KERNEL_PATH);
    if (kernel_path == NULL) {
        Print(L"NotreBoot: cannot create the kernel device path.\r\n");
        return EFI_OUT_OF_RESOURCES;
    }
    status = uefi_call_wrapper(BS->LoadImage, 6, FALSE, image, kernel_path,
                               NULL, 0, &kernel);
    FreePool(kernel_path);
    if (EFI_ERROR(status) || kernel == NULL) {
        Print(L"NotreBoot: failed to load \\vmlinuz.efi.\r\n");
        return EFI_LOAD_ERROR;
    }

    status = uefi_call_wrapper(BS->HandleProtocol, 3, kernel,
                               &LoadedImageProtocol, (VOID **)&kernel_image);
    if (EFI_ERROR(status) || kernel_image == NULL) {
        Print(L"NotreBoot: cannot set Linux kernel options.\r\n");
        uefi_call_wrapper(BS->UnloadImage, 1, kernel);
        return EFI_LOAD_ERROR;
    }
    read_config(self->DeviceHandle, options, &options_chars);
    kernel_image->LoadOptions = options;
    kernel_image->LoadOptionsSize = (UINT32)((options_chars + 1) * sizeof(CHAR16));

    Print(L"Starting Linux with NotreOS initramfs...\r\n");
    status = uefi_call_wrapper(BS->StartImage, 3, kernel, NULL, NULL);
    Print(L"NotreBoot: Linux returned to the loader.\r\n");
    return EFI_ERROR(status) ? status : EFI_SUCCESS;
}
