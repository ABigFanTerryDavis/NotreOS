NOTRE_SHELL_VERSION = 0.0.1
NOTRE_SHELL_SITE = $(BR2_EXTERNAL_NOTREOS_PATH)/package/notre-shell/src
NOTRE_SHELL_SITE_METHOD = local
NOTRE_SHELL_LICENSE = NOASSERTION

NOTRE_SHELL_CFLAGS = $(TARGET_CFLAGS) -std=c11 -Wall -Wextra -Werror

define NOTRE_SHELL_BUILD_CMDS
	$(TARGET_CC) $(NOTRE_SHELL_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(@D)/notre-shell $(@D)/main.c
endef

define NOTRE_SHELL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/notre-shell $(TARGET_DIR)/usr/bin/notre-shell
endef

$(eval $(generic-package))
