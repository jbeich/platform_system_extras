#include <stdint.h>
#include <string.h>
#include <winsock2.h>
#include <openssl/sha.h>

/* Definition from RFC-4122 */
struct uuid {
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint8_t clk_seq_hi_res;
    uint8_t clk_seq_low;
    uint16_t node0_1;
    uint32_t node2_5;
};

void sha256_uuid_generate(const char *namespace, const char *name, uint8_t out[16])
{
    SHA256_CTX ctx;
    uint8_t sha256[SHA256_DIGEST_LENGTH];
    struct uuid *uuid = (struct uuid *)out;

    SHA256_Init(&ctx);
    SHA256_Update(&ctx, namespace, strlen(namespace));
    SHA256_Update(&ctx, name, strlen(name));
    SHA256_Final(sha256, &ctx);
    memcpy(uuid, sha256, sizeof(struct uuid));

    uuid->time_low = ntohl(uuid->time_low);
    uuid->time_mid = ntohs(uuid->time_mid);
    uuid->time_hi_and_version = ntohs(uuid->time_hi_and_version);
    uuid->time_hi_and_version &= 0x0FFF;
    uuid->time_hi_and_version |= (5 << 12);
    uuid->clk_seq_hi_res &= ~(1 << 6);
    uuid->clk_seq_hi_res |= 1 << 7;
}
