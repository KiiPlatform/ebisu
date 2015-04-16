#include "kii.h"
#include "kii-core/kii.h"

int main() {
    kii_t kii;
    kii_init(&kii, "JP", "9ab34d8b", "7a950d78956ed39f3b0815f0f001b43b");
    kiiDev_getToken(&kii, "98477", "1234");
    printf("vendor thing id%s\n", kii.author.author_id);
    printf("thing token%s\n", kii.author.access_token);
}
