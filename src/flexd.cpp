// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under version 3 of the Gnu Affero GPL software license, see the accompanying
// file COPYING 

#include "rpcserver.h"
#include "rpcclient.h"
#include "init.h"
#include "main.h"
#include "noui.h"
#include "ui_interface.h"
#include "datastore.h"
#include "message_handler.h"
#include "flexnode.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>



/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of the reference client for an experimental new digital currency called Bitcoin (http://www.bitcoin.org/),
 * which enables instant payments to anyone, anywhere in the world. Bitcoin uses peer-to-peer technology to operate
 * with no central authority: managing transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the Gnu Affero GPLv3 license.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or <code>Files</code> at the top of the page to start navigating the code.
 */



static bool fDaemon;

static bool fShutdownRequested = false;

void DetectShutdownThread(boost::thread_group* threadGroup)
{
    bool fShutdown = ShutdownRequested();
    // Tell the main threads to shutdown.
    while (!fShutdown)
    {
        MilliSleep(200);
        fShutdown = ShutdownRequested();
    }
    fShutdownRequested = true;
    if (threadGroup)
    {
        threadGroup->interrupt_all();
        threadGroup->join_all();
    }
    
    TraderLeaveMessage leave;
    if (leave.order_hashes.size() > 0)
    {
        log_ << "cancelling outstanding orders\n";
        flexnode.tradehandler.BroadcastMessage(leave);
    }
    log_ << "leaving relay pool\n";
    LeaveRelayPool();

    flexnode.ClearEncryptionKeys();
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
bool AppInit(int argc, char* argv[])
{
    boost::thread_group threadGroup;
    boost::thread* detectShutdownThread = NULL;

    bool fRet = false;
    try
    {
        //
        // Parameters
        //
        // If Qt is used, parameters/flex.conf are parsed in qt/bitcoin.cpp's main()
        
        ParseParameters(argc, argv);
        if (!boost::filesystem::is_directory(GetDataDir(false)))
        {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", mapArgs["-datadir"].c_str());
            return false;
        }
        try
        {
            ReadConfigFile(mapArgs, mapMultiArgs);
            
        } catch(std::exception &e) {
            fprintf(stderr,"Error reading configuration file: %s\n", e.what());
            return false;
        }
        
        if (mapArgs.count("-?") || mapArgs.count("--help"))
        {
            // First part of help message is specific to flexd / RPC client
            std::string strUsage = _("Flex Daemon") + " " + _("version") + " " + FormatFullVersion() + "\n\n" +
                _("Usage:") + "\n" +
                  "  flexd [options]                     " + _("Start Flex Core Daemon") + "\n" +
                _("Usage (deprecated, use flex-cli):") + "\n" +
                  "  flexd [options] <command> [params]  " + _("Send command to Flex Core") + "\n" +
                  "  flexd [options] help                " + _("List commands") + "\n" +
                  "  flexd [options] help <command>      " + _("Get help for a command") + "\n";

            strUsage += "\n" + HelpMessage(HMM_BITCOIND);
            strUsage += "\n" + HelpMessageCli(false);

            fprintf(stdout, "%s", strUsage.c_str());
            return false;
        }
        
        // Command-line RPC
        bool fCommandLine = false;
        for (int i = 1; i < argc; i++)
            if (!IsSwitchChar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "bitcoin:"))
                fCommandLine = true;

        if (fCommandLine)
        {
            int ret = CommandLineRPC(argc, argv);
            exit(ret);
        }
        
#ifndef WIN32
        fDaemon = GetBoolArg("-daemon", false);
        if (fDaemon)
        {
            fprintf(stdout, "Flex server starting\n");

            // Daemonize
            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
                return false;
            }
            if (pid > 0) // Parent process, pid is child process id
            {
                CreatePidFile(GetPidFile(), pid);
                return true;
            }
            // Child process falls through to rest of initialization

            pid_t sid = setsid();
            if (sid < 0)
                fprintf(stderr, "Error: setsid() returned %d errno %d\n", sid, errno);
        }
#endif
        SoftSetBoolArg("-server", true);
        

        detectShutdownThread = new boost::thread(boost::bind(&DetectShutdownThread, &threadGroup));
        
        fRet = AppInit2(threadGroup);
        
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(NULL, "AppInit()");
    }

    if (!fRet)
    {
        if (detectShutdownThread)
            detectShutdownThread->interrupt();

        threadGroup.interrupt_all();
        // threadGroup.join_all(); was left out intentionally here, because we didn't re-test all of
        // the startup-failure cases to make sure they don't result in a hang due to some
        // thread-blocking-waiting-for-another-thread-during-startup case
    }

    if (detectShutdownThread)
    {
        detectShutdownThread->join();
        delete detectShutdownThread;
        detectShutdownThread = NULL;
    }
    Shutdown();

    return fRet;
}

int main(int argc, char* argv[])
{
    SetupEnvironment();

    bool fRet = false;

    // Connect flexd signal handlers
    noui_connect();

    fRet = AppInit(argc, argv);

    if (fRet && fDaemon)
        return 0;

    return (fRet ? 0 : 1);
}
