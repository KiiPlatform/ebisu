<?php
header("Content-Type:text/html;charset=utf8");

function createDir($path) {
    if (!file_exists($path)) {
        createDir(dirname($path));
        mkdir($path, 0777);
    }
}

function log_data($logdata) {
    $path = '/data/log_payment360/';
    createDir($path);
    $fp = fopen($path  . date('Ymd') . '.log', 'ab+');
    fwrite($fp, date('Y-m-d H:i:s') . '|' . $logdata . "\r\n");
    fclose($fp);
}

log_data(json_encode($_REQUEST));
//echo json_encode($_REQUEST);
echo "ok";
