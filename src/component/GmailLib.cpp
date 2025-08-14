#include "GmailLib.hpp"

const uint16_t DEFAULT_REDIRECT_PORT = 62397;

HTTPServer::HTTPServer(const uint16_t tport, const std::string& generated_state, bool tlogProcess) 
                        : port(tport), state(generated_state), logProcess(tlogProcess) {
    allocated = 0;
}

//1 if authorized, 0 if unauthorized, -1 if fail internally, -2 if state has been tampered, -3 if invalid request
//this function can only be called ONCE
int HTTPServer::run(std::string& retAuthCode, std::string *error_string) {
    if(isCalled) return 1000;
    isCalled = true;

    const std::string headerConst = "Server: project mmt local response\r\nContent-type: text/html\r\n\r\n<body><h2>project mmt</h2>";
    const std::pair<std::string, std::string> httpElement[] = {
        std::make_pair("HTTP/1.1 200 OK\r\n", "<h4>You have successfully authorized. Return to the application to continue</h4></body>\r\n"),
        std::make_pair("HTTP/1.1 401 Unauthorized\r\n", "<h4>You did not accept authorization.</h4></body>\r\n"),
        std::make_pair("HTTP/1.1 403 Forbidden\r\n", "<h4>An error has occurred. You should reauthenticate</h4></body>\r\n"),
        std::make_pair("HTTP/1.1 400 Bad Request\r\n", "<h4>Bad request. You should reauthenticate</h4></body>\r\n"),
    };

    socketfile = (int) socket(AF_INET, SOCK_STREAM, 0);
    if (socketfile == -1) {
        if(error_string) *error_string = "socket creation failed";
        return -1;
    }
    if(logProcess) fprintf(stderr, "[OK] Successfully init socket\n");
    allocated++;

    int setsockopt_opt = 1;
    setsockopt(socketfile, SOL_SOCKET, SO_REUSEADDR, (const char*) &setsockopt_opt, sizeof(setsockopt_opt));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(socketfile, (sockaddr*) &sa, sizeof(sa)) == -1) {
        if(error_string) *error_string = "socket bind failed";
        return -1;
    }
    if(logProcess) fprintf(stderr, "[OK] socket binded\n");

    if(listen(socketfile, 5) == -1) {
        if(error_string) *error_string = "socket listen failed";
        return -1;
    };
    if(logProcess) fprintf(stderr, "[OK] socket listening on port %hhu\n", port);

    char tcp_data[tcp_read_max+3];
    std::string storemore;
    int readcnt = 0;

    connectfile = (int) accept(socketfile, NULL, NULL);
    if (connectfile == -1) {
        perror("[ERROR] fail to accept connection");
        return -1;
    }
    allocated++;
    if(logProcess) fprintf(stderr, "[OK] connection accepted!\n");

    memset(tcp_data, 0, sizeof(tcp_data));
    readcnt = (int) recv(connectfile, tcp_data, tcp_read_max, 0);
    if(readcnt > 0) {
        storemore.append(tcp_data, readcnt);
    }
    if(logProcess) fputs("[OK] Done reading. Now prepare to write back\n", stderr);

    int retCode = 1;
    std::string httpResponse;
    std::map<std::string, std::string> mapper;
    if(decodeURLRequest(storemore, mapper) == -1) {
        retCode = -3;
    } else if(mapper.find("error") != mapper.end()) {
        retCode = 0;
    } else if(mapper.find("state") == mapper.end() || mapper["state"] != this->state) {
        retCode = -2;
    } else if (mapper.find("code") != mapper.end()) {
        retAuthCode = mapper["code"];
    } else {
        retCode = -3;
    }

    switch(retCode) {
        case 1: httpResponse = httpElement[0].first + headerConst + httpElement[0].second; break;
        case 0: httpResponse = httpElement[1].first + headerConst + httpElement[1].second; break;
        case -2: httpResponse = httpElement[2].first + headerConst + httpElement[2].second; break;
        default: httpResponse = httpElement[3].first + headerConst + httpElement[3].second; break;
    }

    send(connectfile, httpResponse.c_str(), httpResponse.size(), 0);
    if(logProcess) fprintf(stderr, "[OK] Successfully write back to user\n");

    shutdown(connectfile, 2);
    if(logProcess) fprintf(stderr, "[OK] Connection completed! Closing...\n");
    return retCode;
}

HTTPServer::~HTTPServer() {
    if(allocated > 1) {
    #ifdef WIN32
                closesocket(connectfile);
    #else
                close(connectfile);
    #endif
            }
            if(allocated > 0) {
    #ifdef WIN32
                closesocket(socketfile);
    #else
                close(socketfile);
    #endif
            }
}

/**
 * @brief Creates a raw MIME message string for an email.
 * @return The raw MIME message as a string.
 */
std::string MailMessage::createMimeMessage() {
    std::stringstream message_stream;
    message_stream << "To: " << to << "\r\n";
    message_stream << "From: " << from << "\r\n";
    message_stream << "Subject: " << subject << "\r\n";

    // If there are no attachments, create a simple text/plain message
    if (attachment_paths.empty()) {
        message_stream << "Content-Type: text/plain; charset=\"UTF-8\"\r\n\r\n";
        message_stream << body_text;
    }
    // If there are attachments, create a multipart/mixed message
    else {
        std::string boundary = "boundary_string_for_gmail_api"; // A unique boundary string
        message_stream << "MIME-Version: 1.0\r\n";
        message_stream << "Content-Type: multipart/mixed; boundary=\"" << boundary << "\"\r\n\r\n";

        // Add the text part
        message_stream << "--" << boundary << "\r\n";
        message_stream << "Content-Type: text/plain; charset=\"UTF-8\"\r\n\r\n";
        message_stream << body_text << "\r\n\r\n";

        // Add each attachment part by looping through the file paths
        for (const auto& file_path : attachment_paths) {
            std::ifstream file(file_path, std::ios::binary);
            if (!file.is_open()) {
                std::cerr << "Warning: Could not open file, skipping attachment: " << file_path << std::endl;
                continue;
            }

            std::stringstream file_content_ss;
            file_content_ss << file.rdbuf();
            std::string file_content = file_content_ss.str();
            std::string file_name = file_path.substr(file_path.find_last_of("/\\") + 1);

            // Determine the Content-Type based on the file extension
            std::string content_type = "application/octet-stream"; // Default
            if (file_name.find(".txt") != std::string::npos) content_type = "text/plain";
            else if (file_name.find(".jpg") != std::string::npos || file_name.find(".jpeg") != std::string::npos) content_type = "image/jpeg";
            else if (file_name.find(".png") != std::string::npos) content_type = "image/png";
            else if (file_name.find(".pdf") != std::string::npos) content_type = "application/pdf";
            else if (file_name.find(".mp4") != std::string::npos) content_type = "video/mp4";
            else if (file_name.find(".webm") != std::string::npos) content_type = "video/webm";

            message_stream << "--" << boundary << "\r\n";
            message_stream << "Content-Type: " << content_type << "; name=\"" << file_name << "\"\r\n";
            message_stream << "Content-Disposition: attachment; filename=\"" << file_name << "\"\r\n";
            message_stream << "Content-Transfer-Encoding: base64\r\n\r\n";
            message_stream << base64_encode(file_content) << "\r\n\r\n";
        }

        // Add the final closing boundary
        message_stream << "--" << boundary << "--";
    }

    return message_stream.str();
}

void MailMessage::parse(const json& payload) {
    for (const auto& header : payload["headers"]) {
        if (header["name"] == "From") from = header["value"];
        else if (header["name"] == "Subject") subject = header["value"];
        else if (header["name"] == "Date") date = header["value"];
        else if (header["name"] == "To") to = header["value"];
    }
}

MailMessage::MailMessage(const json& payload) {
    parse(payload);
}

std::string MailMessage::getDate() const {
    return date;
}

void GmailHandler::initRng() {
    rng.seed((uint32_t) std::chrono::steady_clock::now().time_since_epoch().count());
}

void GmailHandler::writeCredential() {
    std::ofstream outfile(credentialFilename, std::ios::out | std::ios::binary);
    if(!outfile) return;
    
    json jsonData;
    jsonData["clientId"] = clientId;
    jsonData["clientSecret"] = clientSecret;
    jsonData["redirectPort"] = redirectPort;
    jsonData["refreshToken"] = refreshToken;
    std::string jsonStr = jsonData.dump(4);
    outfile.write(jsonStr.data(), jsonStr.size());
    outfile.close();
}

std::string GmailHandler::make_state(size_t len) {
    const char sample[] = "abcdef0123456789";
    std::string ret;
    for(size_t i=0; i < len; i++) {
        ret += sample[std::uniform_int_distribution<int>(0, sizeof(sample) - 2)(rng)];
    }
    return ret;
}

json GmailHandler::findMimePart(const json& payload, const std::string& mime_type) {
    if (payload.value("mimeType", "") == mime_type) {
        return payload;
    }

    if (payload.contains("parts")) {
        for (const auto& part : payload["parts"]) {
            json found = findMimePart(part, mime_type);
            if (!found.is_null()) {
                return found;
            }
        }
    }
    return json(); // Return null JSON object if not found
}

GmailHandler::GmailHandler() {initRng();};
GmailHandler::GmailHandler(std::string tclientId, std::string tclientSecret, uint16_t tredirectPort) 
    : clientId(tclientId), clientSecret(tclientSecret), redirectPort(std::to_string(tredirectPort)) {initRng();}
GmailHandler::GmailHandler(std::string filename) {
    initRng();
    (void) loadCredential(filename, NULL);
}

bool GmailHandler::loadCredential(const std::string& filename, std::string *error_string) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        if(error_string) *error_string = "could not open credential file";
        return 0;
    }
    this->credentialFilename = filename;

    json jsonData;
    try {
        jsonData = json::parse(infile);
    } catch (json::parse_error& e) {
        if(error_string) *error_string = "JSON parse error: " + std::string(e.what());
        return 0;
    }

    if(!jsonData.contains("clientId") || !jsonData.contains("clientSecret")) {
        if(error_string) *error_string = "credential file missing required fields";
        return 0;
    }

    clientId = jsonData.at("clientId");
    clientSecret = jsonData.at("clientSecret");
    redirectPort = std::to_string(DEFAULT_REDIRECT_PORT);
    
    if(jsonData.contains("redirectPort")) {
        redirectPort = jsonData.at("redirectPort");
    }
    if(jsonData.contains("refreshToken")) {
        refreshToken = jsonData.at("refreshToken");
    }

    if(error_string) *error_string = "OK";
    return 1;
}

void GmailHandler::getEmailAddressApiCall() {
    std::string url = "https://www.googleapis.com/gmail/v1/users/me/profile";
    std::string token_for_this_attempt = this->getAccessToken();
    std::vector<std::string> headers;
    headers.push_back("Authorization: Bearer " + token_for_this_attempt);

    std::string response, api_error;
    int status_code = inapp_post_or_get(0, url, headers, "", response, &api_error);

    if (status_code == 200) {
        try {
            json json_response = json::parse(response);
            if (json_response.contains("emailAddress")) {
                this->emailAddress = json_response["emailAddress"];
                return;
            }
        } catch (const json::parse_error& e) {
            return;
        }
    }
}

std::string GmailHandler::getClientId() const { return clientId; }
std::string GmailHandler::getClientSecret() const { return clientSecret; }
std::string GmailHandler::getRedirectPort() const { return redirectPort; }
std::string GmailHandler::getAccessToken() const { return accessToken; }
std::string GmailHandler::getRefreshToken() const { return refreshToken; }
std::string GmailHandler::getEmailAddress() const { return emailAddress; }

bool GmailHandler::auth(std::string *errorString) {
    // 1. Get Authorization Code
    std::string state = make_state();
    std::string redirectUri = std::string(httpLocalhost) + redirectPort;
    std::stringstream ss;
    ss << googleOAuthEndpoint << '?';
    ss << "client_id=" << clientId << '&';
    ss << "redirect_uri=" << redirectUri << '&';
    ss << "scope=" << appScope << '&';
    ss << "response_type=code" << '&';
    ss << "access_type=offline" << '&';
    ss << "prompt=consent" << '&'; // Force prompt for refresh token
    ss << "state=" << state;
    std::string openUrl = ss.str();
    ss.str() = "";

    std::cout << "Open URL if not automatically: " << openUrl << "\n";

    HINSTANCE hInst = ShellExecuteA(NULL, "open", openUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if(reinterpret_cast<INT_PTR>(hInst) <= 32) {
        if(errorString) *errorString = "can't auto url";
        return false;
    }

    HTTPServer server((uint16_t) stoi(this->getRedirectPort()), state);
    std::string authCode, error_string;
    int status_code = server.run(authCode, &error_string);

    if (status_code != 1 || authCode.empty()) {
        if(errorString)  *errorString = "Authorization failed. Status: " + std::to_string(status_code) + ", Error: " + error_string;
        return false;
    }

    std::map<std::string, std::string> mapper;
    mapper["client_id"] = clientId;
    mapper["client_secret"] = clientSecret;
    mapper["code"] = authCode;
    mapper["redirect_uri"] = redirectUri;
    mapper["grant_type"] = "authorization_code";

    std::string response;
    status_code = inapp_post_or_get(1, googleTokenEndpoint, {}, makeURLField(mapper), response, &error_string);

    if (status_code == 200) {
        try {
            json json_response = json::parse(response);
            accessToken = json_response["access_token"];
            if (json_response.contains("refresh_token")) {
                refreshToken = json_response["refresh_token"];
            }
            isAuthenticated.store(true);
            getEmailAddressApiCall();
            this->writeCredential();
            if(errorString) *errorString = "OK";
            return true;
        } catch (const json::parse_error& e) {
            if(errorString) *errorString = "JSON parse error on token response: " + std::string(e.what());
            return false;
        }
    } else {
        if(errorString) {
            ss << "\nToken request failed. Status: " << status_code << ", Response: " << response << ", Error: " << error_string;
            *errorString = ss.str();
            ss.str() = "";
        }
        return false;
    }
}

bool GmailHandler::reauth(std::string expiredAccessToken, std::string *errorString) {
    std::lock_guard<std::mutex> lock(accessTokenLock);

    if(expiredAccessToken != this->accessToken) {
        if(errorString) *errorString = "reauth: OK (from other thread)";
        return true;
    }

    if (refreshToken.empty()) {
        if(errorString) *errorString = "reauth: No refresh token";
        return false;
    }

    std::map<std::string, std::string> mapper;
    mapper["client_id"] = clientId;
    mapper["client_secret"] = clientSecret;
    mapper["refresh_token"] = refreshToken;
    mapper["grant_type"] = "refresh_token";

    std::string response, error_string;
    int status_code = inapp_post_or_get(1, googleTokenEndpoint, {}, makeURLField(mapper), response, &error_string);

    if (status_code == 200) {
        try {
            json json_response = json::parse(response);
            accessToken = json_response["access_token"];
            getEmailAddressApiCall();
            if(errorString) *errorString = "reauth: OK";
            isAuthenticated.store(true);
            return true;
        } catch (const json::parse_error& e) {
            if(errorString) *errorString = "reauth: JSON parse error " + std::string(e.what());
        }
    } else {
        if(errorString) {
            std::stringstream ss;
            ss << "reauth: error " << status_code << ", Response: " << response << ", Error: " << error_string;
            *errorString = ss.str();
            ss.str() = "";
        }
    }
    
    return false;
}

/**
 * @brief Sends a pre-constructed raw MIME message using the Gmail API.
 *
 * @param access_token A valid OAuth 2.0 access token.
 * @param mimeMessage The complete, raw MIME message string, ready to be base64 encoded.
 */
bool GmailHandler::sendEmail(const std::string& mimeMessage, std::string& messageIdOnSuccess, std::string *errorString) {
    if(!isAuthenticated.load()) {
        if(errorString) *errorString = "Not authenticated yet";
        return false;
    }

    if (mimeMessage.empty()) {
        if(errorString) *errorString = "Empty message";
        return false;
    }

    std::vector<std::string> headers;
    headers.push_back("Authorization: Bearer " + this->getAccessToken());
    headers.push_back("Content-Type: application/json");

    json payload;
    payload["raw"] = base64_encode(mimeMessage);

    std::string response, error_string;
    for(int i=0; i < 2; i++) {
        int status_code = inapp_post_or_get(1, googleMailSendEndpoint, headers, payload.dump(), response, &error_string);
        if (status_code == 200) {
            try {
                messageIdOnSuccess = json::parse(response)["id"];
                if(errorString) *errorString = "send_email: OK";
            } catch(...) {
                if(errorString) *errorString = "send_email: OK, failed parsing message ID";
            }
            return true;
        } else if(status_code == 401) {
            if(reauth(getAccessToken(), nullptr)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            } else return false;
        } else {
            if(errorString) *errorString = "send_email: failed, " + response;
            return false;
        }
    }
    return false;
}

bool GmailHandler::getEmail(const std::string& messageId, MailMessage& mailMsgRet, std::string *errorString) {
    if(!isAuthenticated.load()) {
        if(errorString) *errorString = "Not authenticated yet";
        return false;
    }

    std::string url = googleMailboxEndpoint + messageId + "?format=full";
    std::vector<std::string> headers;
    headers.push_back("Authorization: Bearer " + this->getAccessToken());
    std::string response, error_string;

    MailMessage mailMsg;

    for(int i=0; i < 2; i++) {
        int status_code = inapp_post_or_get(0, url, headers, "", response, &error_string);
        if (status_code == 200) {
            try {
                json msg = json::parse(response);
                json payload = msg["payload"];

                mailMsg.parse(payload);

                // Find the plain text part of the email
                json text_part = findMimePart(payload, "text/plain");
                if (!text_part.is_null() && text_part.contains("body") && text_part["body"].contains("data")) {
                    std::string encoded_body = text_part["body"]["data"];
                    std::string decoded_body = base64_decode(encoded_body);
                    mailMsg.body_text = decoded_body;
                }

                if(errorString) *errorString = "getEmail: OK";
                mailMsgRet = mailMsg;
                return true;

            } catch (const json::parse_error& e) {
                if(errorString) *errorString = "getEmail: failed; json parse error " + std::string(e.what());
                break;
            }
        } else if(status_code == 401) {
            if(reauth(this->getAccessToken())) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            } else {
                break;
            }
        } else {
            if(errorString) *errorString = "getEmail: failed, " + response;
            break;
        }
    }
    return false;
}

bool GmailHandler::queryMessages(const std::string& query, std::vector<std::string>& messageIds, std::string *errorString) {
    if(!isAuthenticated.load()) {
        if(errorString) *errorString = "Not authenticated yet";
        return 0;
    }

    messageIds.clear();
    std::string encoded_query = urlStringEscape(query);
    std::string url = "https://www.googleapis.com/gmail/v1/users/me/messages?q=" + encoded_query;

    std::vector<std::string> headers;
    headers.push_back("Authorization: Bearer " + this->getAccessToken());
    std::string response, error_string;
    
    for(int i=0; i < 2; i++) {
        int status_code = inapp_post_or_get(0, url, headers, "", response, &error_string);
        if (status_code == 200) {
            json json_response = json::parse(response);
            if (json_response.contains("messages")) {
                for (const auto& msg : json_response["messages"]) {
                    messageIds.push_back(msg["id"]);
                }
            }
            if(errorString) *errorString = "OK " + std::to_string(messageIds.size()) + " elements";
            return 1;
        } else if(status_code == 401) {
            if(reauth(this->getAccessToken())) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            } else {
                return 0;
            }
        } else {
            if(errorString) *errorString = "error " + std::to_string(status_code);
            return 0;
        }
    }
    return 0;
}

size_t curlWriteCallback(char *ptr, size_t charsz, size_t strsz, void *_stdstringret) {
    size_t realsize = charsz * strsz;
    ((std::string*)  _stdstringret)->append(ptr, realsize);
    return realsize;
}

//GET and POST wrapper
//@param post_or_get 1 for post, 0 for get
//@param url url to POST/GET to. URL could have body embed
//@param header_override Override the default Content-Type: `application/x-www-form-urlencoded header`.
//@param post_field POST body. Leave empty if not needed
//@param response Response of the request
//@param error_string Pointer for error description. Pass NULL if omitted
//@return The HTTP status code. -1 if fail to make any request
int inapp_post_or_get(int post_or_get, const std::string& url, const std::vector<std::string>& header_override, const std::string& post_field, std::string& response, std::string *error_string) {
    long response_code;
    curl_slist *headerlist = NULL;
    bool do_header_override = !header_override.empty();
    bool do_write_error_string = error_string != NULL;
    
    CURL *curl = curl_easy_init();
    if(!curl) {
        if(error_string) *error_string = "CURL failed to setup";
        return -1;
    }

    //Need to have curl-ca-bundle.crt to successfully authenticated SSL
    //Otherwise risk exposing yourself to man-in-the-middle attack
    {
        std::string testCrtFilename = "curl-ca-bundle.crt";
        std::ifstream testCrtFile(testCrtFilename);
        if(testCrtFile) {
            testCrtFile.close();
            curl_easy_setopt(curl, CURLOPT_CAINFO, testCrtFilename.c_str());
        } else {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        }
    }
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    // Reset headers for each call
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);

    if(do_header_override) {
        for(const auto& header : header_override) {
            headerlist = curl_slist_append(headerlist, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
    }
    if(!post_field.empty()) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_field.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, (long)post_or_get);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    response.clear();
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode curlres = curl_easy_perform(curl);
    if(do_header_override) curl_slist_free_all(headerlist);
    if(curlres != CURLE_OK) {
        if(do_write_error_string) *error_string = translate_curl_error(curlres);
        return -1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if(do_write_error_string) {
        *error_string = "HTTP " + std::to_string(response_code);
    }

    curl_easy_cleanup(curl);

    return (int)response_code;
}
