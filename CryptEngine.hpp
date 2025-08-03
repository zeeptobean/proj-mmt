#include "sodium.h"

#include <array>
#include <vector>
#include <mutex>
#include <atomic>

const char CIPHER_AAD[] = "project-malware-mmt-24c06-zeept";     //perfectly 32 bytes!

//Todo: add support to key renewal
class CryptEngine {
    private:

    std::array<uint8_t, crypto_box_PUBLICKEYBYTES> publicKey;
    std::array<uint8_t, crypto_box_PUBLICKEYBYTES> otherPublicKey;
    std::array<uint8_t, crypto_box_SECRETKEYBYTES> secretKey;
    std::array<uint8_t, crypto_box_BEFORENMBYTES> sharedKey;
    std::mutex sharedKeyLock;
    bool hasSharedKey = false;
    std::atomic<bool> hasOtherPublicKey {false};

    std::array<uint8_t, 32> makeSharedKey() {
        std::lock_guard<std::mutex> lock(sharedKeyLock);
        if(hasOtherPublicKey.load()) crypto_box_beforenm(sharedKey.data(), otherPublicKey.data(), secretKey.data());
        hasSharedKey = true;
        return sharedKey;
    }

    public:
    CryptEngine(const std::array<uint8_t, 32>& clientPublicKey) {
        setOtherPublicKey(clientPublicKey);
        generateKey();
    }

    CryptEngine(const CryptEngine&) = delete;

    void generateKey() {
        crypto_box_keypair(publicKey.data(), secretKey.data());
        makeSharedKey();
    }

    std::array<uint8_t, 32> getPublicKey() const {
        return publicKey;
    }

    //return new shared key
    std::array<uint8_t, 32> setOtherPublicKey(const std::array<uint8_t, 32>& clientPublicKey) {
        hasOtherPublicKey.store(true);
        return makeSharedKey();
    }

    bool checkOtherPublicKeyStatus() const {
        return hasOtherPublicKey.load();
    }

    bool getSharedKey(std::array<uint8_t, 32>& outputSharedKey) {
        std::lock_guard<std::mutex> lock(sharedKeyLock);
        if(hasSharedKey) outputSharedKey = sharedKey;
        return hasSharedKey;
    } 

    bool encrypt(const std::vector<uint8_t>& plainText, std::vector<uint8_t>& cipherText, std::array<uint8_t, 12>& nonce) {
        cipherText = std::vector<uint8_t>(plainText.size()+16, 0);
        size_t cipherTextSizeOutput;
        randombytes_buf(nonce.data(), 12);

        sharedKeyLock.lock();
        int res = crypto_aead_chacha20poly1305_ietf_encrypt(cipherText.data(), &cipherTextSizeOutput, 
            plainText.data(), plainText.size(), 
            (const uint8_t*) CIPHER_AAD, sizeof(CIPHER_AAD), NULL, nonce.data(), sharedKey.data());
        sharedKeyLock.unlock();
        
        if(res == -1) {
            sodium_memzero(cipherText.data(), cipherText.size());
            sodium_memzero(nonce.data(), nonce.size());
            return false;
        }
        return true;
    }

    bool decrypt(const std::vector<uint8_t>& cipherText, const std::array<uint8_t, 12>& nonce, std::vector<uint8_t>& plainText) {
        plainText = std::vector<uint8_t>(cipherText.size()-16, 0);
        size_t plainTextSizeOutput;

        sharedKeyLock.lock();
        int res = crypto_aead_chacha20poly1305_ietf_decrypt(plainText.data(), &plainTextSizeOutput, 
            NULL, cipherText.data(), cipherText.size(), 
            (const uint8_t*) CIPHER_AAD, sizeof(CIPHER_AAD), nonce.data(), sharedKey.data());
        sharedKeyLock.unlock();

        if(res == -1) {
            sodium_memzero(plainText.data(), plainText.size());
            return false;
        }
        return true;
    }

    ~CryptEngine() {
        sodium_memzero(publicKey.data(), 32);
        sodium_memzero(secretKey.data(), 32);
        sodium_memzero(sharedKey.data(), 32);
    }
};