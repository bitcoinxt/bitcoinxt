package=curl
$(package)_version=7.43.0
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_path=http://curl.haxx.se/download
$(package)_sha256_hash=1a084da1edbfc3bd632861358b26af45ba91aaadfb15d6482de55748b8dfc693
$(package)_dependencies=openssl

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --with-ssl --with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
