#include "kii.h"
#include "kii-core/kii.h"

int main() {
    kii_t kii;
    size_t buffer_size = 4096;
    char buffer[buffer_size];

    memset(buffer, 0x00, buffer_size);
    kii_init(&kii, "JP", "9ab34d8b", "7a950d78956ed39f3b0815f0f001b43b");
    kii.http_context.buffer = buffer;
    kii.http_context.buffer_size = buffer_size;

    kiiDev_getToken(&kii, "98477", "1234");
    printf("vendor thing id: %s\n", kii.author.author_id);
    printf("thing token: %s\n", kii.author.access_token);
}
