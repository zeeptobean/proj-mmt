#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <bits/stdc++.h>
#include <curl/curl.h>
#include "nlohmann/json.hpp"
#include "UrlUtilities.hpp"

using json = nlohmann::json;

inline const char *googleOAuthEndpoint = "https://accounts.google.com/o/oauth2/v2/auth";
inline const char *googleTokenEndpoint = "https://oauth2.googleapis.com/token";
inline const char *googleMailSendEndpoint = "https://www.googleapis.com/gmail/v1/users/me/messages/send";
inline const char *googleMailboxEndpoint = "https://www.googleapis.com/gmail/v1/users/me/messages/";
inline const char *appScope = "https://www.googleapis.com/auth/gmail.send%20https://www.googleapis.com/auth/gmail.readonly%20https://www.googleapis.com/auth/gmail.compose";
inline const char *httpLocalhost = "http://localhost:";

class HTTPServer {
    private:
    int socketfile = -1, connectfile = -1;
    uint16_t port;
    sockaddr_in sa;
    const size_t tcp_read_max = 1536;

    int allocated = 0;
    bool isCalled = false, logProcess = false;

    public:
    std::string state;
    HTTPServer() = delete;
    HTTPServer(const HTTPServer&) = delete;
    HTTPServer(const uint16_t tport, const std::string& generated_state, bool tlogProcess = true);

    //1 if authorized, 0 if unauthorized, -1 if fail internally, -2 if state has been tampered, -3 if invalid request
    //this function can only be called ONCE
    int run(std::string& retAuthCode, std::string *error_string = nullptr);

    virtual ~HTTPServer();
};

struct MailMessage {
    std::string to, from, subject, body_text;
    std::vector<std::string> attachment_paths;

    /**
     * @brief Creates a raw MIME message string for an email.
     * @return The raw MIME message as a string.
     */
    std::string createMimeMessage();

    void parse(const json& payload);

    MailMessage() = default;
    MailMessage(const json& payload);

    std::string getDate() const;

    private:
    std::string date;
};

class GmailHandler {
    private:
    std::string clientId, clientSecret, redirectPort;
    std::string accessToken, refreshToken, accessTokenOld;
    std::string credentialFilename;
    std::mutex accessTokenLock;
    std::mt19937 rng;
    std::atomic<bool> isAuthenticated{false};

    void initRng();

    void writeCredential();

    std::string make_state(size_t len = 8);

    json findMimePart(const json& payload, const std::string& mime_type);

public:
    GmailHandler();
    GmailHandler(std::string tclientId, std::string tclientSecret, uint16_t tredirectPort);
    GmailHandler(std::string filename);
    // GmailHandler(const GmailHandler&) = delete;
    // void operator=(const GmailHandler&) = delete;

    bool loadCredential(const std::string& filename, std::string *error_string = nullptr);

    std::string getClientId() const;
    std::string getClientSecret() const;
    std::string getRedirectPort() const;
    std::string getAccessToken() const;
    std::string getRefreshToken() const;

    bool auth(std::string *errorString = NULL);

    bool reauth(std::string expiredAccessToken, std::string *errorString = NULL);
    
    /**
     * @brief Sends a pre-constructed raw MIME message using the Gmail API.
     *
     * @param access_token A valid OAuth 2.0 access token.
     * @param mimeMessage The complete, raw MIME message string, ready to be base64 encoded.
     */
    bool sendEmail(const std::string& mimeMessage, std::string& messageIdOnSuccess, std::string *errorString = nullptr);

    bool getEmail(const std::string& messageId, MailMessage& mailMsgRet, std::string *errorString = nullptr);

    bool queryMessages(const std::string& query, std::vector<std::string>& messageIds, std::string *errorString = nullptr);
};

//GET and POST wrapper
//@param post_or_get 1 for post, 0 for get
//@param url url to POST/GET to. URL could have body embed
//@param header_override Override the default Content-Type: `application/x-www-form-urlencoded header`.
//@param post_field POST body. Leave empty if not needed
//@param response Response of the request
//@param error_string Pointer for error description. Pass NULL if omitted
//@return The HTTP status code. -1 if fail to make any request
int inapp_post_or_get(int post_or_get, const std::string& url, const std::vector<std::string>& header_override, const std::string& post_field, std::string& response, std::string *error_string = nullptr);