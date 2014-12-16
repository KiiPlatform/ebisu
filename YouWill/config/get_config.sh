curl -v -X GET -H "Authorization: Bearer $KII_TOKEN" \
               -H "x-kii-appid:$KII_APPID" \
               -H "x-kii-appkey:$KII_APPKEY" \
               "https://$KII_HOST/api/apps/$KII_APPID"

