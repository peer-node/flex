include Makefile.include

AM_CPPFLAGS += -O3 -I$(builddir) \
				   -I$(builddir)/crypto \
				   -I$(builddir)/crypto/25519 \
				   -I$(builddir)/rpc \
				   -I$(builddir)/net \
				   -I$(builddir)/base \
				   -I$(builddir)/database \
				   -I$(builddir)/flexnode \
				   -I$(builddir)/credits \
				   -I$(builddir)/currencies \
                   -I$(builddir)/deposit \
				   -I$(builddir)/mining \
				   -I/usr/include/c++/4.9 \
				   -DSCRYPT_SALSA64 \
				   -DSCRYPT_BLAKE512 \
				   -Wno-unused-function \
				   $(debugflags)

noinst_LIBRARIES = \
  libflex_server.a \
  libflex_common.a \
  libflex_cli.a
if ENABLE_WALLET
noinst_LIBRARIES += libflex_wallet.a
endif

bin_PROGRAMS =

if BUILD_BITCOIND
  bin_PROGRAMS += flexd
endif

if BUILD_BITCOIN_CLI
  bin_PROGRAMS += flex-cli
endif

SUBDIRS = . $(BUILD_QT) $(BUILD_TEST)
DIST_SUBDIRS = .
.PHONY: FORCE
# bitcoin core #
BITCOIN_CORE_H = \
  base/allocators.h \
  base/base58.h \
  base/bloom.h \
  base/chainparams.h \
  base/clientversion.h \
  base/core.h \
  base/limitedmap.h \
  base/mruset.h \
  base/noui.h \
  base/protocol.h \
  base/script.h \
  base/serialize.h \
  base/sync.h \
  base/threadsafety.h \
  base/tinyformat.h \
  base/ui_interface.h \
  base/util.h \
  base/version.h \
  compat.h \
  credits/credits.h \
  credits/creditsign.h \
  credits/creditutil.h \
  crypto/25519/curve25519-donna-32bit.h \
  crypto/25519/curve25519-donna-64bit.h \
  crypto/25519/curve25519-donna-helpers.h \
  crypto/25519/curve25519-donna-sse2.h \
  crypto/25519/ed25519-donna-32bit-sse2.h \
  crypto/25519/ed25519-donna-32bit-tables.h \
  crypto/25519/ed25519-donna-64bit-sse2.h \
  crypto/25519/ed25519-donna-64bit-tables.h \
  crypto/25519/ed25519-donna-64bit-x86-32bit.h \
  crypto/25519/ed25519-donna-64bit-x86.h \
  crypto/25519/ed25519-donna-basepoint-table.h \
  crypto/25519/ed25519-donna-batchverify.h \
  crypto/25519/ed25519-donna.h \
  crypto/25519/ed25519-donna-impl-base.h \
  crypto/25519/ed25519-donna-impl-sse2.h \
  crypto/25519/ed25519-donna-portable.h \
  crypto/25519/ed25519-donna-portable-identify.h \
  crypto/25519/ed25519.h \
  crypto/25519/ed25519-hash-custom.h \
  crypto/25519/ed25519-hash.h \
  crypto/25519/ed25519-randombytes-custom.h \
  crypto/25519/ed25519-randombytes.h \
  crypto/25519/modm-donna-32bit.h \
  crypto/25519/modm-donna-64bit.h \
  crypto/25519points.h \
  crypto/25519/regression.h \
  crypto/25519/test-ticks.h \
  crypto/bignum.h \
  crypto/crypter.h \
  crypto/flexcrypto.h \
  crypto/hash.h \
  crypto/hashtrees.h \
  crypto/key.h \
  crypto/point.h \
  crypto/polynomial.h \
  crypto/secp256k1point.h \
  crypto/shorthash.h \
  crypto/signature.h \
  crypto/uint256.h \
  currencies/currencies.h \
  database/data.h \
  database/datastore.h \
  database/keystore.h \
  database/memdb.h \
  define.h \
  deposit/deposit_messages.h \
  deposit/deposit_secret.h \
  deposit/deposits.h \
  deposit/disclosure_messages.h \
  deposit/message_handler.h \
  deposit/transfer_messages.h \
  deposit/withdrawal_messages.h \
  flexnode/batchchainer.h \
  flexnode/calendar.h \
  flexnode/eventnotifier.h \
  flexnode/flexnode.h \
  flexnode/init.h \
  flexnode/main.h \
  flexnode/message_handler.h \
  flexnode/orphanage.h \
  flexnode/schedule.h \
  flexnode/wallet.h \
  initialize/datamessages.h \
  initialize/dataserver.h \
  initialize/downloader.h \
  leveldbwrapper.h \
  log.h \
  mining/pit.h \
  mining/work.h \
  net/addrman.h \
  net/alert.h \
  net/netbase.h \
  net/net.h \
  net/resourcemonitor.h \
  relays/relay_admission.h \
  relays/relay_inheritance.h \
  relays/relay_messages.h \
  relays/relays.h \
  relays/relaystate.h \
  relays/successionsecret.h \
  rpc/rpcclient.h \
  rpc/rpcobjects.h \
  rpc/rpcprotocol.h \
  rpc/rpcserver.h \
  trade/market.h \
  trade/relay.h \
  trade/thirdparty.h \
  trade/trade.h \
  trade/trade.h \
  trade/trade_messages.h \
  trade/trader.h \
  trade/tradesecret.h \
  twist/twist.h \
  twist/twistcpp.h \
  twist/scrypt-jane-twist.h \
  twist/code/scrypt-jane-pbkdf2.h \
  twist/code/scrypt-jane-mix_salsa64-sse2.h \
  twist/code/scrypt-jane-hash_sha512.h \
  twist/code/scrypt-jane-hash_blake512.h \
  twist/code/scrypt-jane-test-vectors.h \
  twist/code/scrypt-jane-chacha.h \
  twist/code/scrypt-jane-mix_chacha-sse2.h \
  twist/code/twist-salsa64.h \
  twist/code/scrypt-jane-hash_sha256.h \
  twist/code/scrypt-jane-portable.h \
  twist/code/scrypt-jane-salsa64.h \
  twist/code/scrypt-jane-mix_chacha.h \
  twist/code/scrypt-jane-mix_salsa-avx.h \
  twist/code/scrypt-jane-mix_salsa64-avx2.h \
  twist/code/scrypt-jane-mix_salsa-xop.h \
  twist/code/scrypt-jane-mix_salsa64-ssse3.h \
  twist/code/scrypt-conf.h \
  twist/code/scrypt-jane-hash.h \
  twist/code/scrypt-jane-romix.h \
  twist/code/scrypt-jane-hash_keccak.h \
  twist/code/scrypt-jane-hash_blake256.h \
  twist/code/scrypt-jane-mix_salsa64.h \
  twist/code/scrypt-jane-mix_salsa.h \
  twist/code/scrypt-jane-portable-x86.h \
  twist/code/scrypt-jane-salsa.h \
  twist/code/twist-template.h \
  twist/code/scrypt-jane-romix-basic.h \
  twist/code/scrypt-jane-mix_chacha-avx.h \
  twist/code/scrypt-jane-mix_salsa-sse2.h \
  twist/code/scrypt-jane-mix_salsa64-avx.h \
  twist/code/scrypt-jane-mix_salsa64-xop.h \
  twist/code/twist-romix.h \
  twist/code/scrypt-jane-hash_skein512.h \
  twist/code/scrypt-jane-mix_chacha-xop.h \
  twist/code/scrypt-jane-mix_chacha-ssse3.h \
  twist/twist-jane.h \
  twist/alloc.h


JSON_H = \
  json/json_spirit.h \
  json/json_spirit_error_position.h \
  json/json_spirit_reader.h \
  json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h \
  json/json_spirit_utils.h \
  json/json_spirit_value.h \
  json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(abs_top_builddir)/src/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
version.o: obj/build.h

libflex_server_a_SOURCES = \
  net/addrman.cpp \
  net/alert.cpp \
  base/bloom.cpp \
  flexnode/calendar.cpp \
  currencies/currencies.cpp \
  deposit/deposits.cpp \
  deposit/deposit_secret.cpp \
  deposit/disclosure.cpp \
  deposit/transfer.cpp \
  deposit/withdraw.cpp \
  flexnode/init.cpp \
  flexnode/eventnotifier.cpp \
  initialize/datamessages.cpp \
  initialize/downloader.cpp \
  initialize/dataserver.cpp \
  json/json_spirit_reader.cpp \
  database/keystore.cpp \
  leveldbwrapper.cpp \
  flexnode/main.cpp \
  net/net.cpp \
  base/noui.cpp \
  relays/relays.cpp \
  relays/relay_messages.cpp \
  relays/relay_admission.cpp \
  relays/relay_inheritance.cpp \
  relays/successionsecret.cpp \
  net/resourcemonitor.cpp \
  rpc/rpcblockchain.cpp \
  rpc/rpcmining.cpp \
  rpc/rpcmisc.cpp \
  rpc/rpcnet.cpp \
  rpc/rpcobjects.cpp \
  rpc/rpcserver.cpp \
  flexnode/wallet.cpp \
  $(JSON_H) \
  $(BITCOIN_CORE_H)

libflex_wallet_a_SOURCES = \
  crypto/crypter.cpp \
  rpc/rpcdump.cpp \
  rpc/rpcwallet.cpp \
  $(BITCOIN_CORE_H)

libflex_common_a_SOURCES = \
  base/base58.cpp \
  base/allocators.cpp \
  base/chainparams.cpp \
  flexnode/batchchainer.cpp \
  base/core.cpp \
  credits/credits.cpp \
  credits/creditutil.cpp \
  flexnode/flexnode.cpp \
  crypto/25519/ed25519.cpp \
  crypto/hash.cpp \
  crypto/key.cpp \
  net/netbase.cpp \
  mining/pit.cpp \
  crypto/point.cpp \
  crypto/polynomial.cpp \
  base/protocol.cpp \
  rpc/rpcprotocol.cpp \
  base/script.cpp \
  relays/relaystate.cpp \
  base/sync.cpp \
  trade/trade.cpp \
  trade/trade_messages.cpp \
  trade/market.cpp \
  trade/trader.cpp \
  trade/relay.cpp \
  trade/thirdparty.cpp \
  mining/work.cpp \
  crypto/flexcrypto.cpp \
  base/util.cpp \
  base/version.cpp \
  $(BITCOIN_CORE_H)

if GLIBC_BACK_COMPAT
libflex_common_a_SOURCES += compat/glibc_compat.cpp
libflex_common_a_SOURCES += compat/glibcxx_compat.cpp
endif

libflex_cli_a_SOURCES = \
  rpc/rpcclient.cpp \
  $(BITCOIN_CORE_H)

nodist_libflex_common_a_SOURCES = $(top_srcdir)/src/obj/build.h
#

# flexd binary #
flexd_LDADD = \
  libflex_server.a \
  libflex_cli.a \
  libflex_common.a \
  $(LIBLEVELDB) \
  $(LIBMEMENV)
if ENABLE_WALLET
flexd_LDADD += libflex_wallet.a
endif
flexd_SOURCES = flexd.cpp
#

if TARGET_WINDOWS
flexd_SOURCES += flexd-res.rc
endif

AM_CPPFLAGS += $(BDB_CPPFLAGS)
flexd_LDADD += $(BOOST_LIBS) $(BDB_LIBS)

# flex-cli binary #
flex_cli_LDADD = \
  libflex_cli.a \
  libflex_common.a \
  $(BOOST_LIBS)
flex_cli_SOURCES = flex-cli.cpp
#

if TARGET_WINDOWS
flex_cli_SOURCES += flex-cli-res.rc
endif

# NOTE: This dependency is not strictly necessary, but without it make may try to build both in parallel, which breaks the LevelDB build system in a race
leveldb/libleveldb.a: leveldb/libmemenv.a

#	$(MAKE) -C leveldb libleveldb.a
#
#leveldb/libmemenv.a:
#	$(MAKE) -C leveldb libmemenv.a

leveldb/%.a:
	@echo "Building LevelDB ..." && $(MAKE) -C $(@D) $(@F) CXX="$(CXX)" \
	  CC="$(CC)" PLATFORM=$(TARGET_OS) AR="$(AR)" $(LEVELDB_TARGET_FLAGS) \
	  OPT="$(CXXFLAGS) $(CPPFLAGS)"

qt/bitcoinstrings.cpp: $(libflex_server_a_SOURCES) $(libflex_common_a_SOURCES) $(libflex_cli_a_SOURCES)
	@test -n $(XGETTEXT) || echo "xgettext is required for updating translations"
	@cd $(top_srcdir); XGETTEXT=$(XGETTEXT) share/qt/extract_strings_qt.py

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a *.gcda *.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = leveldb Makefile.include

clean-local:
	-$(MAKE) -C leveldb clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
