NOTRE_BOOT_VERSION = 0.0.1
NOTRE_BOOT_SITE = $(BR2_EXTERNAL_NOTREOS_PATH)/package/notre-boot/src
NOTRE_BOOT_SITE_METHOD = local
NOTRE_BOOT_LICENSE = NOASSERTION
NOTRE_BOOT_DEPENDENCIES = gnu-efi
NOTRE_BOOT_INSTALL_TARGET = NO
NOTRE_BOOT_INSTALL_IMAGES = YES

NOTRE_BOOT_EFI_CFLAGS = -fPIC -fshort-wchar -mno-red-zone \
	-fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-builtin \
	-DGNU_EFI_USE_MS_ABI -maccumulate-outgoing-args
NOTRE_BOOT_EFI_LDFLAGS = -nostdlib --warn-common --no-undefined \
	--fatal-warnings --build-id=sha1 -z norelro -z nocombreloc \
	--no-dynamic-linker -pie -Bsymbolic

define NOTRE_BOOT_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(NOTRE_BOOT_EFI_CFLAGS) \
		-I$(STAGING_DIR)/usr/include/efi \
		-I$(STAGING_DIR)/usr/include/efi/x86_64 \
		-I$(STAGING_DIR)/usr/include/efi/protocol \
		-c $(@D)/main.c -o $(@D)/main.o
	$(TARGET_LD) $(NOTRE_BOOT_EFI_LDFLAGS) \
		-T $(STAGING_DIR)/usr/lib/elf_x86_64_efi.lds \
		-L$(STAGING_DIR)/usr/lib \
		$(STAGING_DIR)/usr/lib/crt0-efi-x86_64.o $(@D)/main.o \
		-lefi -lgnuefi \
		-o $(@D)/notreboot.so
	$(TARGET_OBJCOPY) -j .text -j .sdata -j .data -j .dynamic \
		-j .rodata -j .rel -j .rela -j .rel.* -j .rela.* \
		-j .rel* -j .rela* -j .areloc -j .reloc \
		-O efi-app-x86_64 $(@D)/notreboot.so $(@D)/NotreBootX64.efi
endef

define NOTRE_BOOT_INSTALL_IMAGES_CMDS
	$(INSTALL) -D -m 0644 $(@D)/NotreBootX64.efi \
		$(BINARIES_DIR)/NotreBootX64.efi
endef

$(eval $(generic-package))
