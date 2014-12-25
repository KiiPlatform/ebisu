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
    $path = '/data/log_notify/';
    createDir($path);
    $fp = fopen($path  . date('Ymd') . '.log', 'ab+');
    fwrite($fp, date('Y-m-d H:i:s') . '|' . json_encode($_POST) . '|' . json_encode($_GET) . '|' . $sql_error . "\r\n");
    fclose($fp);
}

log_data('for_log');


echo "success";
