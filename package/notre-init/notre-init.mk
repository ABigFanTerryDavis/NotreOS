NOTRE_INIT_VERSION = 0.0.1
NOTRE_INIT_SITE = $(BR2_EXTERNAL_NOTREOS_PATH)/package/notre-init/src
NOTRE_INIT_SITE_METHOD = local
NOTRE_INIT_LICENSE = NOASSERTION
NOTRE_INIT_DEPENDENCIES = notre-shell

NOTRE_INIT_CFLAGS = $(TARGET_CFLAGS) -std=c11 -Wall -Wextra -Werror

define NOTRE_INIT_BUILD_CMDS
	$(TARGET_CC) $(NOTRE_INIT_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(@D)/notre-init $(@D)/main.c
endef

define NOTRE_INIT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/notre-init $(TARGET_DIR)/init
	$(INSTALL) -D -m 0755 $(@D)/notre-init $(TARGET_DIR)/sbin/notre-init
endef

$(eval $(generic-package))
