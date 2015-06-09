#include <gtest/gtest.h>

#include <kii.h>
#include "kii_core_impl.h"

TEST(kiiTest, authenticate)
{
    int ret = -1;
    char buffer[4096];
    kii_t kii;
    context_t context;

    kii_init(&kii, "JP", "9ab34d8b", "7a950d78956ed39f3b0815f0f001b43b");

    kii.kii_core.http_context.buffer = buffer;
    kii.kii_core.http_context.buffer_size = 4096;
    kii.kii_core.http_context.app_context = &context;

    strcpy(kii.kii_core.author.author_id,
            "th.396587a00022-51e9-4e11-5eec-07846c59");
    strcpy(kii.kii_core.author.access_token,
            "PlRI8O54j74Og7OtRnYLHu-ftQYINQwgDRuyhi3rGlQ");

    ret = kii_thing_authenticate(&kii, "4649", "1234");

    ASSERT_EQ(0, ret);
}
