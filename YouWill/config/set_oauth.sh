curl -v -X POST -H "content-type:application/vnd.kii.AppModificationRequest+json" \
         -H "authorization:Bearer $KII_TOKEN" \
         -H "x-kii-appid:$KII_APPID" -H "x-kii-appkey:$KII_APPKEY" \
         https://$KII_HOST/api/apps/$KII_APPID -d \
         '{
    "socialAuth": {
        "youwill.consumer_key": "7410868185255298048",
        "youwill.consumer_secret": "bvsXyR5FuQurRCxXx4ED",
        "youwill.token_url": "http://yw2oauth.youwill.com.cn/token.html",
        "youwill.authorization_url": "http://yw2oauth.youwill.com.cn/",
        "youwill.profile_url": "http://lhx.youwill.com.cn/oauth/userData"
    }
}
         ' 


