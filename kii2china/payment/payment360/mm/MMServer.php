<?php
/**
 * M-Market计费平台
 * 外部接口Demo
 * 接收MMarket平台服务器通知接口
 * 
 */

 /**
  *  加载 MMarket.class.php 
  */
require_once dirname(__FILE__).'/MMarket.class.php';
include_once('config.php');

/**
 *  创建 class MMarket
 */
$MMarket = new MMarket();

function createDir($path) {
    if (!file_exists($path)) {
        createDir(dirname($path));
        mkdir($path, 0777);
    }
}

function log_data($logdata) {
    $path = '/data/log_payment_mm/';
    createDir($path);
    $fp = fopen($path  . date('Ymd') . '.log', 'ab+');
    fwrite($fp, date('Y-m-d H:i:s') . '|' . $logdata . "\r\n");
    fclose($fp);
}

/**
 *  接收POST数据
 */

$PostData = file_get_contents("php://input");

/**
 *  开发者服务器向MMarket 平台返回的状态说明
 *  $hRet:
	  		0		成功
			1		其他错误,未知错误
			2		网络异常（该错误指内部子系统之间调用出错）
			9010	繁忙
			9015	拒绝消息，服务器无法完成请求的服务
			9018	外部网元的网络错误
			4000	无效的MsgType 
			4003	无效的APPID
			4004	无效的PayCode
			4005	无效的MD5Sign
  */

if($PostData){
	$ArrayData = $MMarket->XmlToData($PostData);//xml数据转换为数组
	if(is_array($ArrayData)){
		$app_id = $ArrayData['AppID'];
        if ($app_id) {
            $AppKey =$APPS[$app_id];
        } else {
            log_data('Invalid APPID ' . json_encode($ArrayData));
            $hRet = 4003;
            echo $MMarket->SyncAppOrderResp($hRet);//向M-Market平台返回结果
            exit;        
        }

		$flag=$MMarket->ValidSign($ArrayData,$AppKey);        
        if($flag==true){
            log_data('Successful ' . json_encode($ArrayData));
			$hRet = 0;
			echo $MMarket->SyncAppOrderResp($hRet);//向M-Market平台返回结果
			exit;

        }else{
            log_data('Incorrect Sign ' . json_encode($ArrayData));
			$hRet = 4005;
			echo $MMarket->SyncAppOrderResp($hRet);//向M-Market平台返回结果
			exit;
		}		
	} else {
        log_data('Invalid POST data ' . json_encode($_REQUEST));
        $hRet = 1;
        echo $MMarket->SyncAppOrderResp($hRet);//向M-Market平台返回结果
        exit;
    }
} else {
    log_data('No post data ' . json_encode($_REQUEST));
    $hRet = 1;
    echo $MMarket->SyncAppOrderResp($hRet);//向M-Market平台返回结果
    exit;
}
?>
