cmake_minimum_required(VERSION 3.3)
project(teleport)

set(BOOST_THREAD boost_thread)

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/usr/local/opt/openssl/lib -I/usr/local/opt/openssl/include")
    set(BOOST_THREAD boost_thread-mt)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

set(CMAKE_CXX_FLAGS "-O3 ${CMAKE_CXX_FLAGS} -std=c++11 -lboost_system -lssl -l${BOOST_THREAD} -pthread -v")

add_subdirectory(test)
add_subdirectory(test/teleport_tests)
add_subdirectory(test/lib/googlemock)

include_directories(test/lib/googletest/include
        test/lib/googletest
        test/lib/googlemock/include
        test/lib/googlemock
        .
        src/leveldb/include
        src/leveldb/helpers/memenv
        src/leveldb
        src)

set(NET_FILES
        src/net/addrman.cpp
        src/net/addrman.h
        src/net/net.cpp
        src/net/net.h
        src/net/netbase.cpp
        src/net/netbase.h
        src/net/net_caddrdb.cpp
        src/net/net_cnode.cpp
        src/net/net_relay.cpp
        src/net/net_cnode.h
        src/net/net_relay.h
        src/net/net_caddrdb.h
        src/net/net_services.cpp
        src/net/net_services.h
        src/net/net_externalip.cpp
        src/net/net_externalip.h
        src/net/net_sockets.h
        src/net/net_sockets.cpp
        src/net/net_connections.h
        src/net/net_connections.cpp
        src/net/net_cnetmessage.cpp
        src/net/net_cnetmessage.h
        src/net/net_addresses.cpp
        src/net/net_addresses.h
        src/net/net_startstop.cpp
        src/net/net_startstop.h
        src/net/net_messagehandler.cpp
        src/net/net_messagehandler.h
        src/net/net_cleanup.cpp
        src/net/net_sync.h
        src/net/net_sync.cpp
        src/net/net_upnp.cpp)

set(MESSAGING_FILES
        ${NET_FILES}
        src/teleportnode/main.h
        src/teleportnode/main_signals.cpp
        src/teleportnode/main_send_messages.cpp
        src/teleportnode/main_process_messages.cpp
        src/teleportnode/MockInit.h
        src/teleportnode/MockInit.cpp
        test/teleport_tests/main_tests/Communicator.cpp
        test/teleport_tests/main_tests/Communicator.h test/teleport_tests/node/handler_modes.h)

set(LEVELDB_FILES
        src/leveldb/util/arena.cc
        src/leveldb/util/options.cc
        src/leveldb/util/coding.cc
        src/leveldb/util/hash.h
        src/leveldb/util/status.cc
        src/leveldb/util/testharness.h
        src/leveldb/util/bloom.cc
        src/leveldb/util/cache.cc
        src/leveldb/util/histogram.cc
        src/leveldb/util/testutil.cc
        src/leveldb/util/testutil.h
        src/leveldb/util/coding.h
        src/leveldb/util/filter_policy.cc
        src/leveldb/util/crc32c.h
        src/leveldb/util/comparator.cc
        src/leveldb/util/posix_logger.h
        src/leveldb/util/crc32c.cc
        src/leveldb/util/arena.h
        src/leveldb/util/env_posix.cc
        src/leveldb/util/mutexlock.h
        src/leveldb/util/logging.cc
        src/leveldb/util/hash.cc
        src/leveldb/util/random.h
        src/leveldb/util/histogram.h
        src/leveldb/util/env.cc
        src/leveldb/util/logging.h
        src/leveldb/util/testharness.cc
        src/leveldb/include/leveldb/comparator.h
        src/leveldb/include/leveldb/c.h
        src/leveldb/include/leveldb/table.h
        src/leveldb/include/leveldb/table_builder.h
        src/leveldb/include/leveldb/dumpfile.h
        src/leveldb/include/leveldb/env.h
        src/leveldb/include/leveldb/slice.h
        src/leveldb/include/leveldb/filter_policy.h
        src/leveldb/include/leveldb/options.h
        src/leveldb/include/leveldb/status.h
        src/leveldb/include/leveldb/write_batch.h
        src/leveldb/include/leveldb/iterator.h
        src/leveldb/include/leveldb/db.h
        src/leveldb/include/leveldb/cache.h
        src/leveldb/helpers/memenv/memenv.cc
        src/leveldb/helpers/memenv/memenv.h
        src/leveldb/db/filename.h
        src/leveldb/db/db_iter.cc
        src/leveldb/db/snapshot.h
        src/leveldb/db/dbformat.cc
        src/leveldb/db/version_edit.h
        src/leveldb/db/version_set.cc
        src/leveldb/db/dbformat.h
        src/leveldb/db/skiplist.h
        src/leveldb/db/log_writer.h
        src/leveldb/db/write_batch_internal.h
        src/leveldb/db/filename.cc
        src/leveldb/db/version_edit.cc
        src/leveldb/db/version_set.h
        src/leveldb/db/c.cc
        src/leveldb/db/db_impl.cc
        src/leveldb/db/db_impl.h
        src/leveldb/db/builder.cc
        src/leveldb/db/log_writer.cc
        src/leveldb/db/db_iter.h
        src/leveldb/db/table_cache.cc
        src/leveldb/db/builder.h
        src/leveldb/db/write_batch.cc
        src/leveldb/db/log_reader.h
        src/leveldb/db/log_reader.cc
        src/leveldb/db/memtable.h
        src/leveldb/db/memtable.cc
        src/leveldb/db/table_cache.h
        src/leveldb/db/repair.cc
        src/leveldb/db/log_format.h
        src/leveldb/db/dumpfile.cc
        src/leveldb/port/port.h
        src/leveldb/port/win/stdint.h
        src/leveldb/port/thread_annotations.h
        src/leveldb/port/port_posix.cc
        src/leveldb/port/port_example.h
        src/leveldb/port/port_posix.h
        src/leveldb/port/atomic_pointer.h
        src/leveldb/table/filter_block.h
        src/leveldb/table/block_builder.cc
        src/leveldb/table/block.h
        src/leveldb/table/two_level_iterator.cc
        src/leveldb/table/filter_block.cc
        src/leveldb/table/block.cc
        src/leveldb/table/merger.cc
        src/leveldb/table/format.h
        src/leveldb/table/merger.h
        src/leveldb/table/iterator.cc
        src/leveldb/table/table.cc
        src/leveldb/table/iterator_wrapper.h
        src/leveldb/table/table_builder.cc
        src/leveldb/table/format.cc
        src/leveldb/table/block_builder.h
        src/leveldb/table/two_level_iterator.h)

set(MEMORY_DATASTORE_FILES
        test/teleport_tests/teleport_data/MemoryObject.cpp
        test/teleport_tests/teleport_data/MemoryObject.h
        test/teleport_tests/teleport_data/MemoryProperty.cpp
        test/teleport_tests/teleport_data/MemoryProperty.h
        test/teleport_tests/teleport_data/MemoryData.cpp
        test/teleport_tests/teleport_data/MemoryData.h
        test/teleport_tests/teleport_data/MemoryLocationIterator.cpp
        test/teleport_tests/teleport_data/MemoryLocationIterator.h
        test/teleport_tests/teleport_data/MemoryDimension.cpp
        test/teleport_tests/teleport_data/MemoryDimension.h
        test/teleport_tests/teleport_data/MemoryDataStore.cpp
        test/teleport_tests/teleport_data/MemoryLocation.cpp
        test/teleport_tests/teleport_data/MemoryLocation.h
        )

set(DATASTORE_FILES
        ${LEVELDB_FILES}
        src/leveldbwrapper.cpp
        src/leveldbwrapper.h
        ${MEMORY_DATASTORE_FILES}
        src/database/DiskDataStore.h
        test/teleport_tests/teleport_data/DataStore.cpp
        test/teleport_tests/teleport_data/DataStore.h
        test/teleport_tests/teleport_data/DataObject.cpp
        test/teleport_tests/teleport_data/DataObject.h
        test/teleport_tests/teleport_data/DataProperty.cpp
        test/teleport_tests/teleport_data/DataProperty.h
        test/teleport_tests/teleport_data/DataLocation.cpp
        test/teleport_tests/teleport_data/DataLocation.h
        test/teleport_tests/teleport_data/LocationIterator.cpp
        test/teleport_tests/teleport_data/LocationIterator.h)

set(DATABASE_FILES
        ${DATASTORE_FILES}
        src/database/memdb.h)

set(BASE_FILES
        src/base/allocators.cpp
        src/base/allocators.h
        src/base/base58.cpp
        src/base/base58.h
        src/base/chainparams.cpp
        src/base/chainparams.h
        src/base/clientversion.h
        src/base/core.cpp
        src/base/core.h
        src/base/limitedmap.h
        src/base/mruset.h
        src/base/noui.cpp
        src/base/noui.h
        src/base/protocol.h
        src/base/serialize.h
        src/base/sync.cpp
        src/base/sync.h
        src/base/threadsafety.h
        src/base/tinyformat.h
        src/base/ui_interface.h
        src/base/util.cpp
        src/base/util_log.cpp
        src/base/util.h
        src/base/version.cpp
        src/base/version.h)

SET(REMAINING_BASE_FILES
        src/base/allocators.cpp
        src/base/allocators.h
        src/base/base58.cpp
        src/base/base58.h
        src/base/base64.cpp
        src/base/base64.h
        src/base/clientversion.h
        src/base/chainparams.cpp
        src/base/limitedmap.h
        src/base/mruset.h
        src/base/noui.cpp
        src/base/noui.h
        src/base/protocol.h
        src/base/protocol.cpp
        src/base/serialize.h
        src/base/sync.cpp
        src/base/sync.h
        src/base/threadsafety.h
        src/base/tinyformat.h
        src/base/ui_interface.h
        src/base/util.h
        src/base/version.cpp
        src/base/version.h
        src/base/util_log.cpp
        src/base/util_file.cpp
        src/base/util_args.cpp
        src/base/util_money.cpp
        src/base/util_data.cpp
        src/base/util_pid.cpp
        src/base/util_rand.cpp
        src/base/util_time.cpp
        src/net/netbase.cpp
        src/base/util_format.cpp
        src/base/util_init.cpp
        src/base/util_init.h
        src/base/util_log.h
        src/base/util_time.h
        src/base/util_hex.cpp
        src/base/util_hex.h
        src/base/util_rand.h
        src/base/util_format.h
        src/base/util_file.h
        src/base/util_data.h
        src/base/util_args.h
        src/base/util_compare.h)

set(MINCRYPTO_FILES
        src/crypto/25519/curve25519-donna-32bit.h
        src/crypto/25519/curve25519-donna-64bit.h
        src/crypto/25519/curve25519-donna-helpers.h
        src/crypto/25519/curve25519-donna-sse2.h
        src/crypto/25519/ed25519-donna-32bit-sse2.h
        src/crypto/25519/ed25519-donna-32bit-tables.h
        src/crypto/25519/ed25519-donna-64bit-sse2.h
        src/crypto/25519/ed25519-donna-64bit-tables.h
        src/crypto/25519/ed25519-donna-64bit-x86-32bit.h
        src/crypto/25519/ed25519-donna-64bit-x86.h
        src/crypto/25519/ed25519-donna-basepoint-table.h
        src/crypto/25519/ed25519-donna-batchverify.h
        src/crypto/25519/ed25519-donna-impl-base.h
        src/crypto/25519/ed25519-donna-impl-sse2.h
        src/crypto/25519/ed25519-donna-portable-identify.h
        src/crypto/25519/ed25519-donna-portable.h
        src/crypto/25519/ed25519-donna.h
        src/crypto/25519/ed25519-hash-custom.h
        src/crypto/25519/ed25519-hash.h
        src/crypto/25519/ed25519-randombytes-custom.h
        src/crypto/25519/ed25519-randombytes.h
        src/crypto/25519/ed25519.cpp
        src/crypto/25519/ed25519.h
        src/crypto/25519/modm-donna-32bit.h
        src/crypto/25519/modm-donna-64bit.h
        src/crypto/25519/regression.h
        src/crypto/25519points.h
        src/crypto/bignum.h
        src/crypto/hash.cpp
        src/crypto/hash.h
        src/crypto/hashtrees.h
        src/crypto/key.h
        src/crypto/key.cpp
        src/crypto/point.cpp
        src/crypto/point.h
        src/crypto/polynomial.cpp
        src/crypto/polynomial.h
        src/crypto/secp256k1point.h
        src/crypto/shorthash.h
        src/crypto/signature.h
        src/crypto/uint256.h)

set(CRYPTO_FILES
        ${MINCRYPTO_FILES}
        src/crypto/teleportcrypto.cpp
        src/crypto/teleportcrypto.h
        test/teleport_tests/crypto/DistributedSecret.cpp
        test/teleport_tests/crypto/DistributedSecret.h
        test/teleport_tests/crypto/EncryptedSecret.cpp
        test/teleport_tests/crypto/EncryptedSecret.h)

set(WORK_FILES
        src/mining/work.cpp
        src/mining/work.h)

set(CREDITS_FILES
        ${WORK_FILES}
        src/credits/Credit.cpp
        src/credits/Credit.h
        src/credits/CreditInBatch.cpp
        src/credits/CreditInBatch.h
        src/credits/CreditBatch.cpp
        src/credits/CreditBatch.h
        src/credits/UnsignedTransaction.cpp
        src/credits/UnsignedTransaction.h
        src/vector_tools.h
        src/credits/SignedTransaction.cpp
        src/credits/SignedTransaction.h
        src/credits/creditsign.h
        test/teleport_tests/node/deposits/messages/DepositAddressRequest.cpp
        test/teleport_tests/node/deposits/messages/DepositAddressRequest.h)

set(NEW_CREDITS_FILES
        ${WORK_FILES}
        src/crypto/key.h
        src/crypto/key.cpp
        src/base/BitcoinAddress.cpp
        src/base/BitcoinAddress.h
        src/credits/Credit.cpp
        src/credits/Credit.h
        src/credits/CreditInBatch.cpp
        src/credits/CreditInBatch.h
        src/credits/CreditBatch.cpp
        src/credits/CreditBatch.h
        src/credits/UnsignedTransaction.cpp
        src/credits/UnsignedTransaction.h
        src/vector_tools.h
        src/credits/SignedTransaction.cpp
        src/credits/SignedTransaction.h
        src/credits/creditsign.h
        test/teleport_tests/node/credit/structures/CreditSystem.cpp
        test/teleport_tests/node/credit/structures/CreditSystem.h
        test/teleport_tests/node/credit/handlers/MinedCreditMessageValidator.cpp
        test/teleport_tests/node/credit/handlers/MinedCreditMessageValidator.h
        test/teleport_tests/node/credit/handlers/TransactionValidator.cpp
        test/teleport_tests/node/credit/handlers/TransactionValidator.h
        test/teleport_tests/node/calendar/Diurn.cpp
        test/teleport_tests/node/calendar/Diurn.h
        test/teleport_tests/node/calendar/Calendar.cpp
        test/teleport_tests/node/calendar/Calendar.h
        test/teleport_tests/node/calendar/Calend.cpp
        test/teleport_tests/node/calendar/Calend.h
        test/teleport_tests/node/credit/handlers/CreditMessageHandler.cpp
        test/teleport_tests/node/credit/handlers/CreditMessageHandler.h
        test/teleport_tests/node/TestPeer.h
        test/teleport_tests/node/credit/messages/BadBatchMessage.cpp
        test/teleport_tests/node/credit/messages/BadBatchMessage.h
        test/teleport_tests/node/wallet/Wallet.cpp
        test/teleport_tests/node/wallet/Wallet.h
        test/teleport_tests/node/credit/handlers/TipController.cpp
        test/teleport_tests/node/credit/handlers/TipController.h
        test/teleport_tests/node/credit/handlers/MinedCreditMessageBuilder.cpp
        test/teleport_tests/node/credit/handlers/MinedCreditMessageBuilder.h test/teleport_tests/node/credit/structures/CreditTracker.cpp test/teleport_tests/node/credit/structures/CreditTracker.h test/teleport_tests/node/credit/structures/TimeAndRecipient.cpp test/teleport_tests/node/credit/structures/TimeAndRecipient.h)

set(DATA_HANDLER_FILES
        test/teleport_tests/node/historical_data/LoadHashesIntoDataStore.h
        test/teleport_tests/node/historical_data/LoadHashesIntoDataStore.cpp
        test/teleport_tests/node/historical_data/messages/KnownHistoryMessage.cpp
        test/teleport_tests/node/historical_data/messages/KnownHistoryMessage.h
        test/teleport_tests/node/historical_data/messages/KnownHistoryDeclaration.cpp
        test/teleport_tests/node/historical_data/messages/KnownHistoryDeclaration.h
        test/teleport_tests/node/historical_data/messages/DiurnMessageData.cpp
        test/teleport_tests/node/historical_data/messages/DiurnMessageData.h
        test/teleport_tests/node/historical_data/messages/EnclosedMessageData.h
        test/teleport_tests/node/historical_data/messages/EnclosedMessageData.cpp
        test/teleport_tests/node/historical_data/handlers/TipHandler.cpp
        test/teleport_tests/node/historical_data/handlers/TipHandler.h
        test/teleport_tests/node/historical_data/handlers/CalendarHandler.cpp
        test/teleport_tests/node/historical_data/handlers/CalendarHandler.h
        test/teleport_tests/node/historical_data/handlers/InitialDataHandler.cpp
        test/teleport_tests/node/historical_data/handlers/InitialDataHandler.h
        test/teleport_tests/node/historical_data/handlers/KnownHistoryHandler.cpp
        test/teleport_tests/node/historical_data/handlers/KnownHistoryHandler.h
        test/teleport_tests/node/historical_data/messages/KnownHistoryRequest.cpp
        test/teleport_tests/node/historical_data/messages/KnownHistoryRequest.h
        test/teleport_tests/node/historical_data/messages/DiurnDataRequest.cpp
        test/teleport_tests/node/historical_data/messages/DiurnDataRequest.h
        test/teleport_tests/node/historical_data/messages/DiurnDataMessage.cpp
        test/teleport_tests/node/historical_data/messages/DiurnDataMessage.h
        test/teleport_tests/node/historical_data/messages/DiurnFailureDetails.cpp
        test/teleport_tests/node/historical_data/messages/DiurnFailureDetails.h
        test/teleport_tests/node/historical_data/messages/DiurnFailureMessage.cpp
        test/teleport_tests/node/historical_data/messages/DiurnFailureMessage.h)

add_executable(runRPCTests
        test/teleport_tests/test_main.cpp
        test/teleport_tests/rpc/test_rpc.cpp)

add_executable(runCryptoTests
        test/teleport_tests/teleport_data/TestData.h
        ${REMAINING_BASE_FILES}
        test/teleport_tests/teleport_data/MemoryDataStore.cpp
        ${DATABASE_FILES}
        ${CREDITS_FILES}
        ${MINCRYPTO_FILES}
        test/teleport_tests/crypto/DistributedSecret.cpp
        test/teleport_tests/crypto/DistributedSecret.h
        test/teleport_tests/crypto/EncryptedSecret.cpp
        test/teleport_tests/crypto/EncryptedSecret.h
        test/teleport_tests/test_main.cpp
        test/teleport_tests/crypto/test_point.cpp
        test/teleport_tests/crypto/test_secret.cpp)

add_executable(runTwistWorkProofTests
        test/teleport_tests/teleport_data/TestData.h
        ${REMAINING_BASE_FILES}
        ${MINCRYPTO_FILES}
        test/teleport_tests/teleport_data/MemoryDataStore.cpp
        ${DATABASE_FILES}
        ${WORK_FILES}
        test/teleport_tests/test_main.cpp
        test/teleport_tests/mining/testSimpleWorkProof.cpp)

add_executable(runNetworkSpecificProofOfWorkTests
        ${REMAINING_BASE_FILES}
        ${MINCRYPTO_FILES}
        ${WORK_FILES}
        test/teleport_tests/test_main.cpp
        test/teleport_tests/mining/MiningHashTree.cpp
        test/teleport_tests/mining/MiningHashTree.h
        test/teleport_tests/mining/testNetworkSpecificProofOfWork.cpp
        test/teleport_tests/mining/NetworkSpecificProofOfWork.cpp
        test/teleport_tests/mining/NetworkSpecificProofOfWork.h)

add_executable(runMiningTests
        ${WORK_FILES}
        test/teleport_tests/test_main.cpp
        test/teleport_tests/mining/TeleportMiner.cpp
        test/teleport_tests/mining/TeleportMiner.h
        test/teleport_tests/mining/NetworkMiningInfo.cpp
        test/teleport_tests/mining/NetworkMiningInfo.h
        test/teleport_tests/mining/MiningHashTree.cpp
        test/teleport_tests/mining/MiningHashTree.h
        test/teleport_tests/mining/NetworkSpecificProofOfWork.cpp
        test/teleport_tests/mining/NetworkSpecificProofOfWork.h
        test/teleport_tests/mining/MiningRPCServer.cpp
        test/teleport_tests/mining/MiningRPCServer.h
        test/teleport_tests/mining/server_key.h
        test/teleport_tests/node/server/HttpAuthServer.cpp
        test/teleport_tests/node/server/HttpAuthServer.h
        test/teleport_tests/mining/testNetworkSpecificProofOfWork.cpp
        test/teleport_tests/mining/testMiningRPCServer.cpp
        test/teleport_tests/mining/test_mining.cpp
        )

add_executable(minerd
        ${WORK_FILES}
        test/teleport_tests/mining/TeleportMiner.cpp
        test/teleport_tests/mining/TeleportMiner.h
        test/teleport_tests/mining/NetworkMiningInfo.cpp
        test/teleport_tests/mining/NetworkMiningInfo.h
        test/teleport_tests/mining/MiningHashTree.cpp
        test/teleport_tests/mining/MiningHashTree.h
        test/teleport_tests/mining/MiningRPCServer.cpp
        test/teleport_tests/mining/MiningRPCServer.h
        test/teleport_tests/mining/NetworkSpecificProofOfWork.cpp
        test/teleport_tests/mining/NetworkSpecificProofOfWork.h
        test/teleport_tests/mining/minerd.cpp
        test/teleport_tests/node/server/HttpAuthServer.cpp
        test/teleport_tests/node/server/HttpAuthServer.h)

add_executable(runSchedulerTests
        test/teleport_tests/teleport_data/TestData.h
        src/teleportnode/schedule.h
        ${REMAINING_BASE_FILES}
        ${MINCRYPTO_FILES}
        test/teleport_tests/teleport_data/MemoryDataStore.cpp
        ${DATABASE_FILES}
        test/teleport_tests/test_main.cpp
        test/teleport_tests/scheduler/test_scheduler.cpp)

add_executable(runDataTests
        test/teleport_tests/teleport_data/TestData.h
        ${REMAINING_BASE_FILES}
        ${MINCRYPTO_FILES}
        ${DATABASE_FILES}
        test/teleport_tests/test_main.cpp
        test/teleport_tests/teleport_data/testMemoryDataStore.cpp
        test/teleport_tests/teleport_data/testDiskDataStore.cpp
        test/teleport_tests/teleport_data/testDataStore.cpp
        test/teleport_tests/node/tests/testShortHashList.cpp)

set(CONFIG_FILES
        test/teleport_tests/node/config/TeleportConfig.cpp
        test/teleport_tests/node/config/TeleportConfig.h
        test/teleport_tests/node/config/TeleportConfigDefaults.h
        test/teleport_tests/node/config/ConfigParser.cpp
        test/teleport_tests/node/config/ConfigParser.h test/teleport_tests/currency/CryptoCurrencyAddress.cpp test/teleport_tests/currency/CryptoCurrencyAddress.h test/teleport_tests/currency/CryptoCurrencyPrivateKey.cpp test/teleport_tests/currency/CryptoCurrencyPrivateKey.h)

set(CURRENCY_FILES
        test/teleport_tests/node/server/HttpAuthServer.cpp
        test/teleport_tests/node/server/HttpAuthServer.h
        test/teleport_tests/currency/CurrencyRPC.cpp
        test/teleport_tests/currency/CurrencyRPC.h
        test/teleport_tests/currency/string_functions.h
        test/teleport_tests/currency/CurrencyCommands.cpp
        test/teleport_tests/currency/CurrencyCommands.h
        test/teleport_tests/currency/CryptoCurrencyCommands.cpp
        test/teleport_tests/currency/CryptoCurrencyCommands.h
        test/teleport_tests/currency/Currency.cpp
        test/teleport_tests/currency/Currency.h
        test/teleport_tests/currency/CurrencyCrypto.cpp
        test/teleport_tests/currency/CurrencyCrypto.h
        test/teleport_tests/currency/NotaryRPC.cpp
        test/teleport_tests/currency/NotaryRPC.h)

add_executable(runCurrencyTests
        test/teleport_tests/node/tests/testCurrency.cpp
        ${REMAINING_BASE_FILES}
        ${MINCRYPTO_FILES}
        ${DATABASE_FILES}
        ${CONFIG_FILES}
        ${CURRENCY_FILES}
        test/teleport_tests/test_main.cpp
        test/teleport_tests/currency/TestElectrumServer.cpp
        test/teleport_tests/currency/TestElectrumServer.h)


set(NODE_TEST_FILES
        test/teleport_tests/teleport_data/TestData.h
        ${REMAINING_BASE_FILES}
        ${MINCRYPTO_FILES}
        ${NEW_CREDITS_FILES}
        ${NET_FILES}
        test/teleport_tests/teleport_data/MemoryDataStore.cpp
        ${DATABASE_FILES}
        ${WORK_FILES}
        ${MESSAGING_FILES}
        ${CONFIG_FILES}
        src/teleportnode/main.cpp
        src/teleportnode/main.h
        test/teleport_tests/node/TeleportNetworkNode.cpp
        test/teleport_tests/node/TeleportNetworkNode.h
        test/teleport_tests/node/server/TeleportRPCServer.cpp
        test/teleport_tests/node/server/TeleportRPCServer.h
        test/teleport_tests/node/server/TeleportLocalServer.cpp
        test/teleport_tests/node/server/TeleportLocalServer.h
        src/base/base58.cpp
        src/base/base58.h
        test/teleport_tests/node/server/HttpAuthServer.cpp
        test/teleport_tests/node/server/HttpAuthServer.h
        test/teleport_tests/mining/TeleportMiner.cpp
        test/teleport_tests/mining/TeleportMiner.h
        test/teleport_tests/mining/NetworkMiningInfo.cpp
        test/teleport_tests/mining/NetworkMiningInfo.h
        test/teleport_tests/mining/MiningHashTree.cpp
        test/teleport_tests/mining/MiningHashTree.h
        test/teleport_tests/mining/MiningRPCServer.cpp
        test/teleport_tests/mining/MiningRPCServer.h
        test/teleport_tests/mining/NetworkSpecificProofOfWork.cpp
        test/teleport_tests/mining/NetworkSpecificProofOfWork.h
        test/teleport_tests/node/credit/messages/MinedCreditMessage.cpp
        test/teleport_tests/node/credit/messages/MinedCreditMessage.h
        test/teleport_tests/node/historical_data/handlers/DataMessageHandler.cpp
        test/teleport_tests/node/historical_data/handlers/DataMessageHandler.h
        test/teleport_tests/node/historical_data/messages/TipRequestMessage.cpp
        test/teleport_tests/node/historical_data/messages/TipRequestMessage.h
        test/teleport_tests/node/historical_data/messages/TipMessage.cpp
        test/teleport_tests/node/historical_data/messages/TipMessage.h
        test/teleport_tests/node/historical_data/messages/CalendarRequestMessage.cpp
        test/teleport_tests/node/historical_data/messages/CalendarRequestMessage.h
        test/teleport_tests/node/historical_data/messages/CalendarMessage.cpp
        test/teleport_tests/node/historical_data/messages/CalendarMessage.h
        test/teleport_tests/node/historical_data/messages/CalendarFailureDetails.cpp
        test/teleport_tests/node/historical_data/messages/CalendarFailureDetails.h
        test/teleport_tests/node/historical_data/messages/CalendarFailureMessage.cpp
        test/teleport_tests/node/historical_data/messages/CalendarFailureMessage.h
        test/teleport_tests/node/historical_data/messages/InitialDataRequestMessage.cpp
        test/teleport_tests/node/historical_data/messages/InitialDataRequestMessage.h
        test/teleport_tests/node/historical_data/messages/InitialDataMessage.cpp
        test/teleport_tests/node/historical_data/messages/InitialDataMessage.h
        test/teleport_tests/node/TestPeerWithNetworkNode.h
        test/teleport_tests/node/credit/messages/ListExpansionRequestMessage.cpp
        test/teleport_tests/node/credit/messages/ListExpansionRequestMessage.h
        test/teleport_tests/node/credit/messages/ListExpansionMessage.cpp
        test/teleport_tests/node/credit/messages/ListExpansionMessage.h)

set(RELAY_FILES
        test/teleport_tests/node/relays/structures/RelayState.cpp
        test/teleport_tests/node/relays/structures/RelayState.h
        test/teleport_tests/node/relays/structures/Relay.cpp
        test/teleport_tests/node/relays/structures/Relay.h
        test/teleport_tests/node/relays/messages/RelayJoinMessage.cpp
        test/teleport_tests/node/relays/messages/RelayJoinMessage.h
        test/teleport_tests/node/relays/handlers/RelayMessageHandler.cpp
        test/teleport_tests/node/relays/handlers/RelayMessageHandler.h
        test/teleport_tests/node/relays/messages/KeyDistributionMessage.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionMessage.h
        test/teleport_tests/node/relays/messages/SecretRecoveryMessage.cpp
        test/teleport_tests/node/relays/messages/SecretRecoveryMessage.h
        test/teleport_tests/node/relays/messages/KeyDistributionComplaint.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionComplaint.h
        test/teleport_tests/node/relays/structures/DurationWithoutResponse.cpp
        test/teleport_tests/node/relays/structures/DurationWithoutResponse.h
        test/teleport_tests/node/relays/structures/DurationWithoutResponseFromRelay.cpp
        test/teleport_tests/node/relays/structures/DurationWithoutResponseFromRelay.h
        test/teleport_tests/node/relays/messages/GoodbyeMessage.cpp
        test/teleport_tests/node/relays/messages/GoodbyeMessage.h
        test/teleport_tests/node/relays/messages/SecretRecoveryComplaint.cpp
        test/teleport_tests/node/relays/messages/SecretRecoveryComplaint.h
        test/teleport_tests/node/relays/structures/RelayPublicKeySet.cpp
        test/teleport_tests/node/relays/structures/RelayPublicKeySet.h
        test/teleport_tests/node/relays/messages/SecretRecoveryFailureMessage.cpp
        test/teleport_tests/node/relays/messages/SecretRecoveryFailureMessage.h
        test/teleport_tests/node/relays/messages/RecoveryFailureAuditMessage.cpp
        test/teleport_tests/node/relays/messages/RecoveryFailureAuditMessage.h
        test/teleport_tests/node/relays/structures/RelayMessageHashData.cpp
        test/teleport_tests/node/relays/structures/RelayMessageHashData.h
        test/teleport_tests/node/relays/structures/RelayMemoryCache.cpp
        test/teleport_tests/node/relays/structures/RelayMemoryCache.h
        test/teleport_tests/node/relays/handlers/RelayAdmissionHandler.cpp
        test/teleport_tests/node/relays/handlers/RelayAdmissionHandler.h
        test/teleport_tests/node/relays/handlers/RelaySuccessionHandler.cpp
        test/teleport_tests/node/relays/handlers/RelaySuccessionHandler.h
        test/teleport_tests/node/Data.h
        test/teleport_tests/node/Data.cpp
        test/teleport_tests/node/relays/messages/SuccessionCompletedMessage.cpp
        test/teleport_tests/node/relays/messages/SuccessionCompletedMessage.h
        test/teleport_tests/node/relays/handlers/InheritedTaskHandler.cpp
        test/teleport_tests/node/relays/handlers/InheritedTaskHandler.h
        test/teleport_tests/node/relays/handlers/RelayTipHandler.cpp
        test/teleport_tests/node/relays/handlers/RelayTipHandler.h
        test/teleport_tests/node/relays/messages/KeyDistributionAuditMessage.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionAuditMessage.h
        test/teleport_tests/node/relays/structures/KeyDistributionAuditorSelection.cpp
        test/teleport_tests/node/relays/structures/KeyDistributionAuditorSelection.h
        test/teleport_tests/node/relays/messages/KeyDistributionAuditComplaint.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionAuditComplaint.h
        test/teleport_tests/node/relays/messages/KeyDistributionAuditFailureMessage.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionAuditFailureMessage.h
        test/teleport_tests/node/relays/structures/Task.cpp
        test/teleport_tests/node/relays/structures/Task.h
        test/teleport_tests/node/relays/structures/KeyQuarterLocation.cpp
        test/teleport_tests/node/relays/structures/KeyQuarterLocation.h
        test/teleport_tests/node/relays/structures/EncryptedKeyQuarter.cpp
        test/teleport_tests/node/relays/structures/EncryptedKeyQuarter.h
        test/teleport_tests/node/relays/structures/SuccessionAttempt.cpp
        test/teleport_tests/node/relays/structures/SuccessionAttempt.h
        test/teleport_tests/node/relays/structures/RecoveryFailureAudit.cpp
        test/teleport_tests/node/relays/structures/RecoveryFailureAudit.h
        test/teleport_tests/node/relays/messages/FourSecretRecoveryMessages.cpp
        test/teleport_tests/node/relays/messages/FourSecretRecoveryMessages.h
        test/teleport_tests/node/relays/messages/FourRecoveryFailureAuditMessages.cpp
        test/teleport_tests/node/relays/messages/FourRecoveryFailureAuditMessages.h
        test/teleport_tests/node/relays/messages/SuccessionCompletedAuditMessage.cpp
        test/teleport_tests/node/relays/messages/SuccessionCompletedAuditMessage.h
        test/teleport_tests/node/relays/messages/SuccessionCompletedAuditComplaint.cpp
        test/teleport_tests/node/relays/messages/SuccessionCompletedAuditComplaint.h)

set(MESSAGE_HANDLER_WITH_ORPHANAGE_FILES
        test/teleport_tests/node/MessageHandlerWithOrphanage.cpp
        test/teleport_tests/node/MessageHandlerWithOrphanage.h
        )

set(DEPOSIT_FILES
        src/teleportnode/schedule.h
        test/teleport_tests/node/deposits/handlers/DepositMessageHandler.cpp
        test/teleport_tests/node/deposits/handlers/DepositMessageHandler.h
        test/teleport_tests/node/deposits/messages/DepositAddressRequest.h
        test/teleport_tests/node/deposits/messages/DepositAddressRequest.cpp
        test/teleport_tests/node/deposits/messages/DepositAddressPartMessage.h
        test/teleport_tests/node/deposits/messages/DepositAddressPartMessage.cpp
        test/teleport_tests/node/deposits/messages/DepositAddressPartComplaint.cpp
        test/teleport_tests/node/deposits/messages/DepositAddressPartComplaint.h
        test/teleport_tests/node/deposits/messages/DepositAddressPartDisclosure.cpp
        test/teleport_tests/node/deposits/messages/DepositAddressPartDisclosure.h
        test/teleport_tests/node/deposits/messages/DepositDisclosureComplaint.cpp
        test/teleport_tests/node/deposits/messages/DepositDisclosureComplaint.h
        test/teleport_tests/node/deposits/messages/DepositTransferMessage.cpp
        test/teleport_tests/node/deposits/messages/DepositTransferMessage.h
        test/teleport_tests/node/deposits/messages/TransferAcknowledgement.cpp
        test/teleport_tests/node/deposits/messages/TransferAcknowledgement.h
        test/teleport_tests/node/deposits/messages/WithdrawalRequestMessage.cpp
        test/teleport_tests/node/deposits/messages/WithdrawalRequestMessage.h
        test/teleport_tests/node/deposits/messages/WithdrawalMessage.cpp
        test/teleport_tests/node/deposits/messages/WithdrawalMessage.h
        test/teleport_tests/node/deposits/messages/WithdrawalComplaint.cpp
        test/teleport_tests/node/deposits/messages/WithdrawalComplaint.h
        test/teleport_tests/node/deposits/handlers/DepositAddressRequestHandler.cpp
        test/teleport_tests/node/deposits/handlers/DepositAddressRequestHandler.h
        test/teleport_tests/node/deposits/messages/AddressPartSecret.cpp
        test/teleport_tests/node/deposits/messages/AddressPartSecret.h
        test/teleport_tests/node/deposits/messages/AddressPartSecretDisclosure.cpp
        test/teleport_tests/node/deposits/messages/AddressPartSecretDisclosure.h
        test/teleport_tests/node/deposits/handlers/DepositAddressDisclosureHandler.cpp
        test/teleport_tests/node/deposits/handlers/DepositAddressDisclosureHandler.h
        test/teleport_tests/node/deposits/handlers/DepositAddressTransferHandler.cpp
        test/teleport_tests/node/deposits/handlers/DepositAddressTransferHandler.h
        test/teleport_tests/node/deposits/handlers/DepositAddressWithdrawalHandler.cpp
        test/teleport_tests/node/deposits/handlers/DepositAddressWithdrawalHandler.h)

add_executable(runRelayTests
        ${RELAY_FILES}
        ${NEW_CREDITS_FILES}
        ${REMAINING_BASE_FILES}
        ${MINCRYPTO_FILES}
        ${DATABASE_FILES}
        ${CONFIG_FILES}
        ${NET_FILES}
        test/teleport_tests/node/MessageHandlerWithOrphanage.cpp
        test/teleport_tests/node/MessageHandlerWithOrphanage.h
        test/teleport_tests/mining/MiningHashTree.cpp
        test/teleport_tests/mining/MiningHashTree.h
        test/teleport_tests/mining/NetworkSpecificProofOfWork.cpp
        test/teleport_tests/mining/NetworkSpecificProofOfWork.h
        test/teleport_tests/node/historical_data/messages/EnclosedMessageData.h
        test/teleport_tests/node/historical_data/messages/EnclosedMessageData.cpp
        test/teleport_tests/node/historical_data/LoadHashesIntoDataStore.h
        test/teleport_tests/node/historical_data/LoadHashesIntoDataStore.cpp
        test/teleport_tests/test_main.cpp
        test/teleport_tests/node/tests/testRelayMemoryCache.cpp
        test/teleport_tests/node/tests/testRelayState.cpp
        test/teleport_tests/node/tests/testRelay.cpp
        test/teleport_tests/node/tests/testRelayMessageHandler.cpp
        test/teleport_tests/node/tests/testRelayPublicKeySet.cpp
        test/teleport_tests/node/tests/testRelayJoinMessage.cpp
        test/teleport_tests/node/relays/messages/SuccessionCompletedMessage.cpp
        test/teleport_tests/node/relays/messages/SuccessionCompletedMessage.h
        test/teleport_tests/node/relays/handlers/InheritedTaskHandler.cpp
        test/teleport_tests/node/relays/handlers/InheritedTaskHandler.h
        test/teleport_tests/node/tests/testRelayTipHandler.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionAuditMessage.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionAuditMessage.h
        test/teleport_tests/node/tests/testKeyDistributionAuditorSelection.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionAuditComplaint.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionAuditComplaint.h
        test/teleport_tests/node/relays/messages/KeyDistributionAuditFailureMessage.cpp
        test/teleport_tests/node/relays/messages/KeyDistributionAuditFailureMessage.h
        test/teleport_tests/node/relays/structures/Task.cpp
        test/teleport_tests/node/relays/structures/Task.h
        test/teleport_tests/node/relays/structures/KeyQuarterLocation.cpp
        test/teleport_tests/node/relays/structures/KeyQuarterLocation.h
        test/teleport_tests/node/relays/messages/SuccessionCompletedAuditComplaint.cpp
        test/teleport_tests/node/relays/messages/SuccessionCompletedAuditComplaint.h)

add_executable(runNodeTests
        ${NODE_TEST_FILES}
        ${DATA_HANDLER_FILES}
        ${RELAY_FILES}
        ${DEPOSIT_FILES}
        test/teleport_tests/node/MessageHandlerWithOrphanage.cpp
        test/teleport_tests/node/MessageHandlerWithOrphanage.h
        test/teleport_tests/test_main.cpp
        test/teleport_tests/main_tests/testCommunicator.cpp
        test/teleport_tests/node/tests/testDataMessageHandler.cpp
        test/teleport_tests/main_tests/testNetwork.cpp
        test/teleport_tests/node/tests/testConfigParser.cpp
        test/teleport_tests/node/tests/testTeleportLocalServer.cpp
        test/teleport_tests/node/tests/testMessageHandlerWithOrphanage.cpp
        test/teleport_tests/node/tests/testHttpAuthServer.cpp
        test/teleport_tests/node/tests/testTeleportConfig.cpp
        test/teleport_tests/node/tests/testTeleportRPCServer.cpp
        test/teleport_tests/node/tests/testRelayState.cpp
        test/teleport_tests/node/tests/testRelay.cpp
        test/teleport_tests/node/tests/testRelayMessageHandler.cpp
        test/teleport_tests/node/tests/testRelayJoinMessage.cpp
        test/teleport_tests/node/Data.h
        test/teleport_tests/node/tests/testPointPrecomputation.cpp
        test/teleport_tests/node/tests/testRelayPublicKeySet.cpp
        test/teleport_tests/node/tests/testRelayMemoryCache.cpp
        test/teleport_tests/node/tests/testTeleportNetworkNode.cpp
        test/teleport_tests/node/tests/testCalendar.cpp
        test/teleport_tests/node/tests/testCalend.cpp
        test/teleport_tests/node/tests/testCreditMessageHandler.cpp
        test/teleport_tests/node/tests/testCreditSystem.cpp
        test/teleport_tests/node/tests/testDiurn.cpp
        test/teleport_tests/node/tests/testWallet.cpp
        test/teleport_tests/node/tests/testMinedCreditMessageValidator.cpp
        test/teleport_tests/node/tests/testTransactionValidator.cpp
        test/teleport_tests/node/tests/testRelayTipHandler.cpp)

add_executable(runDepositTests
        ${NODE_TEST_FILES}
        ${DATA_HANDLER_FILES}
        ${RELAY_FILES}
        ${MESSAGE_HANDLER_WITH_ORPHANAGE_FILES}
        ${DEPOSIT_FILES}
        ${CURRENCY_FILES}
        test/teleport_tests/node/tests/testDepositHandler.cpp)

add_executable(teleportd
        test/teleport_tests/teleport_data/TestData.h
        test/teleport_tests/node/MessageHandlerWithOrphanage.cpp
        test/teleport_tests/node/MessageHandlerWithOrphanage.h
        ${NODE_TEST_FILES}
        ${DATA_HANDLER_FILES}
        ${RELAY_FILES}
        ${DEPOSIT_FILES}
        ${DATABASE_FILES}
        test/teleport_tests/node/teleportd.cpp)

target_link_libraries(runRPCTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD} microhttpd jsoncpp jsonrpccpp-common jsonrpccpp-server jsonrpccpp-client)

target_link_libraries(runNodeTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD} microhttpd jsoncpp jsonrpccpp-common jsonrpccpp-server jsonrpccpp-client
        curl curl)

target_link_libraries(runRelayTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD} microhttpd jsoncpp jsonrpccpp-common jsonrpccpp-server jsonrpccpp-client
        curl curl)

target_link_libraries(runDepositTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD} microhttpd jsoncpp jsonrpccpp-common jsonrpccpp-server jsonrpccpp-client
        curl curl)

target_link_libraries(teleportd gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD} microhttpd jsoncpp jsonrpccpp-common jsonrpccpp-server jsonrpccpp-client
        curl curl)

target_link_libraries(runCurrencyTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD} microhttpd jsoncpp jsonrpccpp-common jsonrpccpp-server jsonrpccpp-client
        curl curl)

target_link_libraries(runMiningTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD} microhttpd jsoncpp jsonrpccpp-common jsonrpccpp-server jsonrpccpp-client
        curl curl)

target_link_libraries(minerd ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD} microhttpd jsoncpp jsonrpccpp-common jsonrpccpp-server jsonrpccpp-client)

target_link_libraries(runCryptoTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD})

target_link_libraries(runNetworkSpecificProofOfWorkTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD})

target_link_libraries(runTwistWorkProofTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD})

target_link_libraries(runSchedulerTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD})

target_link_libraries(runDataTests gmock gmock_main ssl crypto boost_system boost_program_options
        boost_filesystem ${BOOST_THREAD})