curl -v -X POST\
           -H "content-type:application/json"\
           -H "x-kii-appid:$KII_APPID"\
           -H "x-kii-appkey:$KII_APPKEY"\
           "https://$KII_HOST/api/oauth2/token" -d "{\"client_id\":\"$KII_CLIENT_ID\", \"client_secret\":\"$KII_CLIENT_SECRET\"}"

