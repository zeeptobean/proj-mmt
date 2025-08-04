#pragma once

#include "sodium.h"

#include <array>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstring>

const char CIPHER_AAD[] = "project-malware-mmt-24c06-zeept";     //perfectly 32 bytes!

//Todo: add support to key renewal
class CryptHandler {
    private:

    std::array<uint8_t, crypto_box_PUBLICKEYBYTES> publicKey;
    std::array<uint8_t, crypto_box_PUBLICKEYBYTES> otherPublicKey;
    std::array<uint8_t, crypto_box_SECRETKEYBYTES> secretKey;
    std::array<uint8_t, crypto_box_BEFORENMBYTES> sharedKey;
    std::mutex sharedKeyLock;
    bool hasSharedKey = false;
    std::atomic<bool> hasOtherPublicKey {false};

    std::array<uint8_t, 32> makeSharedKey();

    public:
    CryptHandler();

    CryptHandler(const std::array<uint8_t, 32>& clientPublicKey);

    CryptHandler(const CryptHandler&) = delete;

    void generateKey();

    std::array<uint8_t, 32> getPublicKey() const;

    //return new shared key
    std::array<uint8_t, 32> setOtherPublicKey(const std::array<uint8_t, 32>& clientPublicKey);

    bool checkOtherPublicKeyStatus() const;

    bool getSharedKey(std::array<uint8_t, 32>& outputSharedKey);

    bool encrypt(const std::vector<uint8_t>& plainText, std::vector<uint8_t>& cipherText, std::array<uint8_t, 12>& nonce);

    bool decrypt(const std::vector<uint8_t>& cipherText, const std::array<uint8_t, 12>& nonce, std::vector<uint8_t>& plainText);

    ~CryptHandler();
};