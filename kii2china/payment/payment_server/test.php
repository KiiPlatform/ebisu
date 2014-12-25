<?php
header("Content-Type:text/html;charset=utf8");
$order_info = '{"partner":"2088701768276994","discount":"0.00","payment_type":"1","subject":"Big Sword","trade_no":"2014032133271888","buyer_email":"kiitochina003@gmail.com","gmt_create":"2014-03-21 10:04:46","quantity":"1","out_trade_no":"0TM1LPcUDAcj7zyKbIvGlGn9UTKVDyEiPJokMx6IVratf30pZTAqjXv0TK5izpP4","seller_id":"2088701768276994","trade_status":"TRADE_FINISHED","is_total_fee_adjust":"N","total_fee":"0.01","gmt_payment":"2014-03-21 10:04:47","seller_email":"kiitochina@kii.com","gmt_close":"2014-03-21 10:04:47","price":"0.01","buyer_id":"2088311138446883","use_coupon":"N","dist_id":"7a621d674bb04547","user_id":"2670e865-8fc0-4a69-a99a-f8e97230ec3a","product_id":"3"}';

$arg = escapeshellarg($order_info);
exec("python /opt/payment_jobs/calltask.py " . $arg);
echo "Done!";
