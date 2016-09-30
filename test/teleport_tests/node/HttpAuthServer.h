#ifndef TELEPORT_HTTPAUTHSERVER_H
#define TELEPORT_HTTPAUTHSERVER_H

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <ws2tcpip.h>
#if defined(_MSC_FULL_VER) && !defined (_SSIZE_T_DEFINED)
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif // !_SSIZE_T_DEFINED */
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#endif

#include <map>
#include <microhttpd.h>
#include <jsonrpccpp/server/iclientconnectionhandler.h>
#include <jsonrpccpp/server/abstractserverconnector.h>

namespace jsonrpc
{
    class HttpAuthServer: public AbstractServerConnector
    {
    public:
        std::map<std::string, std::string> headers;
        bool authentication_required{false};

        HttpAuthServer(int port, const std::string& username, const std::string& password,
                       const std::string& sslcert = "", const std::string& sslkey = "", int threads = 50);

        virtual bool StartListening();
        virtual bool StopListening();

        bool virtual SendResponse(const std::string& response, void* addInfo = NULL);
        bool virtual SendOptionsResponse(void* addInfo);

        void SetUrlHandler(const std::string &url, IClientConnectionHandler *handler);

        void RequireAuthentication();

    private:
        int port;
        int threads;
        bool running;
        std::string username;
        std::string password;
        std::string path_sslcert;
        std::string path_sslkey;
        std::string sslcert;
        std::string sslkey;

        struct MHD_Daemon *daemon;

        std::map<std::string, IClientConnectionHandler*> urlhandler;

        static int callback(void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls);

        IClientConnectionHandler* GetHandler(const std::string &url);

        static int header_iterator(void *cls, MHD_ValueKind kind, const char *key, const char *value);
    };

} /* namespace jsonrpc */


#endif //TELEPORT_HTTPAUTHSERVER_H
