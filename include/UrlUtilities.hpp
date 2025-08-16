#pragma once

#include <curl/curl.h>
#include <bits/stdc++.h>

inline std::string translate_curl_error(const CURLcode& code) {
    return std::string("curl failed, error ") + curl_easy_strerror(code);
}

//in second
inline int64_t getCurrentUnixTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline std::string trimNameFromEmail(const std::string& tstr) {
    //trim name, keep email to avoid unicode problems
    size_t start_pos = tstr.find('<');
    if (start_pos == std::string::npos) {
        return tstr;
    }
    size_t end_pos = tstr.find('>', start_pos);

    if (end_pos == std::string::npos) {
        return tstr.substr(start_pos + 1);
    }
    return tstr.substr(start_pos + 1, end_pos - (start_pos + 1));
}

//web-safe base64 encode
inline std::string base64_encode(const std::string &in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

//web-safe base64 decode
inline std::string base64_decode(std::string input) {
    std::replace(input.begin(), input.end(), '-', '+');
    std::replace(input.begin(), input.end(), '_', '/');

    // Add padding if necessary
    while (input.size() % 4) {
        input += '=';
    }

    std::string output;
    output.clear();
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (c >= 'A' && c <= 'Z') c -= 'A';
        else if (c >= 'a' && c <= 'z') c = c - 'a' + 26;
        else if (c >= '0' && c <= '9') c = c - '0' + 52;
        else if (c == '+') c = 62;
        else if (c == '/') c = 63;
        else if (c == '=') break; // Padding character
        else continue;

        val = (val << 6) + c;
        valb += 6;
        if (valb >= 0) {
            output.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return output;
}

inline std::string urlStringEscape(const std::string& tstr) {
    CURL *curl = curl_easy_init();
    if(!curl) return "";
    char *output = curl_easy_escape(curl, tstr.c_str(), tstr.size());
    std::string result(output);
    curl_free(output);
    curl_easy_cleanup(curl);
    return result;
}

inline std::string makeURLField(const std::map<std::string, std::string>& mapper) {
    std::stringstream ss;
    for(auto& pp:mapper) {
        ss << pp.first << '=' << pp.second << '&';
    }
    std::string ret = ss.str();
    if(ret.length() > 0) ret.pop_back();
    return ret;
}

inline int decodeURLRequest(const std::string& response, std::map<std::string, std::string>& ret) {
    ret.clear();
    char *cres = (char*) calloc(response.size()+3, 1);
    char *tokptr = NULL;
    std::string resbody;

    strcpy(cres, response.c_str());
    tokptr = strtok(cres, " \n");
    if(tokptr == NULL || strcmp(tokptr, "GET") != 0) {
        free(cres);
        return -1;
    }
    tokptr = strtok(NULL, " \n");
    if (tokptr == NULL) {
        free(cres);
        return -1;
    }
    resbody = std::string(tokptr);
    free(cres);

    size_t query_start = resbody.find("?");
    if (query_start == std::string::npos) {
        return -1;
    }
    std::string query_string = resbody.substr(query_start + 1);
    std::string current_pair;
    std::istringstream iss(query_string);
    while(std::getline(iss, current_pair, '&')) {
        size_t eq_pos = current_pair.find("=");
        if (eq_pos != std::string::npos) {
            std::string key = current_pair.substr(0, eq_pos);
            std::string value = current_pair.substr(eq_pos + 1);
            ret[key] = value;
        }
    }
    return 0;
}