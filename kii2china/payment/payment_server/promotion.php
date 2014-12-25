<?php
header("Content-Type:text/html;charset=utf8");
include_once('config.php');

$STATUS_CODE_VERIFIFIED = 10086;

if (array_key_exists('user_id', $_GET)) {
    $user_id = $_GET['user_id'];
}
if (array_key_exists('code', $_GET)) {
    $code = $_GET['code'];
}

if (empty($user_id) || empty($code)) {
    echo json_encode(array('status' => 1, 'msg' => 'user_id or code is empty'));
    exit;
}

$mysqli = new mysqli($config['db']['host'], $config['db']['userName'], $config['db']['userPass'], $config['db']['dbName']);
mysqli_set_charset($mysqli, 'utf8');

$sql = "select * from t_promotion_codes where user_id='$user_id' and code='$code' and status=0";

$result = $mysqli->query($sql);
if (!$result) {
    echo json_encode(array('status' => 0, 'msg' => '未知错误，请稍后重试！'), JSON_UNESCAPED_UNICODE);
    $mysqli -> close();
    exit;
}

$row = $result->fetch_array(MYSQLI_ASSOC);

if (!$row) {
    echo json_encode(array('status' => 1, 'msg' => '无效的优惠码，请联系客服'), JSON_UNESCAPED_UNICODE);
    $result->close();
    $mysqli -> close();
    exit;
}

$read_user_id = $row['user_id'];
$real_code = $row['user_id'];
$data = $row['data'];

if ($user_id === $real_user_id || $code === $real_code || empty($data)) {
    echo json_encode(array('status' => 1, 'msg' => '无效的优惠码，请联系客服'), JSON_UNESCAPED_UNICODE);
    $result->close();
    $mysqli -> close();
    exit;
}

echo json_encode(array('status' => $STATUS_CODE_VERIFIFIED, 'msg' => '成功', 'data' => "$data"), JSON_UNESCAPED_UNICODE);
$result->close();

$id = $row['id'];

$sql = "update t_promotion_codes set status=1, used_time=CURRENT_TIMESTAMP where id=$id";
$mysqli->query($sql);
$mysqli -> close();

