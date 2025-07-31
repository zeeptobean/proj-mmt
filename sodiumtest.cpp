#include <bits/stdc++.h>
#include <sodium.h>
using namespace std;

const char CIPHER_AAD[] = "project-malware-mmt-24c06-zeept";     //perfectly 32 bytes!

class ChaCha20Poly1305NonceGenerator {
public:
    // Returns the next unique 12-byte nonce (96-bit) of ChaCha20Poly1305 IETF standard
    static void next_nonce(std::array<uint8_t, 12>& ret) {
        ret = get_instance().generate_nonce();
    }

    static void next_nonce(uint8_t *ret) {
        std::array<uint8_t, 12> tret = get_instance().generate_nonce();
        memcpy(ret, tret.data(), 12);
    }

private:
    static ChaCha20Poly1305NonceGenerator& get_instance() {
        static ChaCha20Poly1305NonceGenerator instance;
        return instance;
    }

    ChaCha20Poly1305NonceGenerator() {
        std::random_device rd;
        std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
        fixed_iv_ = dist(rd);
        counter_.store(0);
    }

    ChaCha20Poly1305NonceGenerator(const ChaCha20Poly1305NonceGenerator&) = delete;
    ChaCha20Poly1305NonceGenerator& operator=(const ChaCha20Poly1305NonceGenerator&) = delete;

    // Internal nonce generation
    std::array<uint8_t, 12> generate_nonce() {
        uint64_t counter_value = counter_.fetch_add(1);
        /*
        if (counter_value == UINT64_MAX) {
            throw std::overflow_error("Nonce counter exhausted");
        }
        */

        std::array<uint8_t, 12> nonce;

        // Fixed IV (4 bytes, big-endian)
        nonce[0] = static_cast<uint8_t>(fixed_iv_ >> 24);
        nonce[1] = static_cast<uint8_t>((fixed_iv_ >> 16) & 0xFF);
        nonce[2] = static_cast<uint8_t>((fixed_iv_ >> 8) & 0xFF);
        nonce[3] = static_cast<uint8_t>(fixed_iv_ & 0xFF);

        // Counter (8 bytes, big-endian)
        for (int i = 0; i < 8; ++i) {
            nonce[11 - i] = static_cast<uint8_t>((counter_value >> (8 * i)) & 0xFF);
        }

        return nonce;
    }

    uint32_t fixed_iv_;
    std::atomic<uint64_t> counter_;
};

void PrintHexByteArray(const std::string& prefix, uint8_t *arr, int size, bool spacing = false) {
    printf("%s ", prefix.c_str());
    if(spacing) for(int i=0; i < size; i++) printf("%02x ", arr[i]);
    else for(int i=0; i < size; i++) printf("%02x", arr[i]);
    printf("\n");
}

int main() {
    sodium_init();

    string plaintext;
    cout << "Input message\n";
    cin >> plaintext;

    uint8_t client_public[crypto_box_PUBLICKEYBYTES];
    uint8_t client_secret[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair(client_public, client_secret);
    uint8_t server_public[crypto_box_PUBLICKEYBYTES];
    uint8_t server_secret[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair(server_public, server_secret);

    PrintHexByteArray("Client public:", client_public, crypto_box_PUBLICKEYBYTES);
    PrintHexByteArray("Client secret:", client_secret, crypto_box_SECRETKEYBYTES);
    PrintHexByteArray("Server public:", server_public, crypto_box_PUBLICKEYBYTES);
    PrintHexByteArray("Server secret:", server_secret, crypto_box_SECRETKEYBYTES);

    uint8_t shared_secret[crypto_box_BEFORENMBYTES];
    int res = crypto_box_beforenm(shared_secret, server_public, client_secret);
    //same thing
    // uint8_t server_shared_secret[crypto_box_BEFORENMBYTES];
    // res = crypto_box_beforenm(server_shared_secret, client_public, server_secret);

    PrintHexByteArray("Client shared secret:", shared_secret, crypto_box_BEFORENMBYTES);
    // PrintHexByteArray("Server shared secret:", server_shared_secret, crypto_box_BEFORENMBYTES);

    uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
    ChaCha20Poly1305NonceGenerator::next_nonce(nonce);
    uint8_t *ciphertext = new uint8_t[plaintext.size() + crypto_aead_chacha20poly1305_ietf_ABYTES];
    sodium_memzero(ciphertext, plaintext.size() + crypto_aead_chacha20poly1305_ietf_ABYTES);
    size_t ciphertextSize;

    res = crypto_aead_chacha20poly1305_ietf_encrypt(ciphertext, &ciphertextSize, 
        (uint8_t*) plaintext.c_str(), plaintext.size(), 
        (const uint8_t*) CIPHER_AAD, sizeof(CIPHER_AAD), NULL, nonce, shared_secret);

    PrintHexByteArray("Nonce", nonce, crypto_aead_chacha20poly1305_ietf_NPUBBYTES);
    PrintHexByteArray("Ciphertext", ciphertext, ciphertextSize, true);

    uint8_t *decrypted = new uint8_t[ciphertextSize];
    sodium_memzero(decrypted, ciphertextSize);  
    size_t decryptedSize;
    res = crypto_aead_chacha20poly1305_ietf_decrypt(decrypted, &decryptedSize, NULL,
        ciphertext, ciphertextSize, (const uint8_t*) CIPHER_AAD, sizeof(CIPHER_AAD), nonce, shared_secret);
    
    if(res == -1) printf("decryption failed!\n");
    else printf("decrypted: %s\n", decrypted);
}