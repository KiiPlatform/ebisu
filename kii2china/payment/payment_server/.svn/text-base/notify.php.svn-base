<?php
header("Content-Type:text/html;charset=utf8");

include_once('config.php');
include_once('alipay_function.php');

function createDir($path) {
    if (!file_exists($path)) {
        createDir(dirname($path));
        mkdir($path, 0777);
    }
}

function log_data($sql_error) {
    $path = '/data/log_payment/';
    createDir($path);
    $fp = fopen($path  . date('Ymd') . '.log', 'ab+');
    fwrite($fp, date('Y-m-d H:i:s') . '|' . $_POST['notify_data'] . '|' . $_POST['sign'] . '|' . json_encode($_GET) . '|' . $sql_error . "\r\n");
    fclose($fp);
}

function call_update_task($order_info) {
    $arg = escapeshellarg($order_info);
    exec("python /opt/payment_jobs/calltask.py " . $arg); 
}

$notify_data = "notify_data=" . $_POST["notify_data"];
$sign = $_POST["sign"];

$isVerify = verify($notify_data, $sign);
if (!$isVerify) {
    log_data('fail');
    exit();
}

$xml = simplexml_load_string($_POST['notify_data']);
$trade_status = $xml -> trade_status;
if (($trade_status == 'TRADE_FINISHED') || ($trade_status == 'TRADE_SUCCESS')) {
    $mysqli = new mysqli($config['db']['host'], $config['db']['userName'], $config['db']['userPass'], $config['db']['dbName']);
    mysqli_set_charset($mysqli, 'utf8');
   
    if (array_key_exists('dist_id', $_POST)) { 
        $dist_id = $_POST['dist_id'];
    } else {
        $dist_id = '';
    }
    if (array_key_exists('user_id', $_POST)) {
        $user_id = $_POST['user_id'];
    } else {
        $user_id = '';
    }

    if (array_key_exists('product_id', $_POST)) {
        $product_id = $_POST['product_id'];
    } else {
        $product_id = '';
    }
    
    $sql = "insert into t_order_info (dist_id, user_id, product_id, trade_status, total_fee, subject, out_trade_no,"
        . "trade_no, gmt_create, gmt_payment, buyer_email, buyer_id, payment_type, quantity, price, discount," 
        . "is_total_fee_adjust, use_coupon) values (" 
        . "'$dist_id', '$user_id', '$product_id', '{$xml -> trade_status}', '{$xml -> total_fee}', '{$xml -> subject}'," 
        . "'{$xml -> out_trade_no}', '{$xml -> trade_no}', '{$xml -> gmt_create}', '{$xml -> gmt_payment}', "
        . "'{$xml -> buyer_email}', '{$xml -> buyer_id}', '{$xml -> payment_type}', '{$xml -> quantity}',"
        . "'{$xml -> price}', '{$xml -> discount}', '{$xml -> is_total_fee_adjust}', '{$xml -> use_coupon}'"
        .");";
    $result = $mysqli->query($sql);
    if (!$result) {
        log_data($mysqli->error);
    }
    $mysqli -> close();

    $xml -> dist_id = $dist_id;
    $xml -> user_id = $user_id;
    $xml -> product_id = $product_id;
    call_update_task(json_encode($xml));    
} else {
    log_data('for_log');
}


echo "success";
