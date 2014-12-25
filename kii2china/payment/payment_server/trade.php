<?php
header("Content-Type:text/html;charset=utf8");

require_once ("config.php");
require_once ("alipay_function.php");
    
    $post_data = file_get_contents("php://input");
    
    $content = getData(json_decode($post_data, true));
    $mySign = sign($content);
    $data = array('orderinfo' => $content, 
                'sign' => urlencode($mySign), 
                'alipay_rsa' => $alipay_rsa); 
 
    echo json_encode($data);

    function getData($post_data){
        global $partner, $seller, $notify_url_root_v1, $notify_url_root_v2;

        $product_id = '';
        if (array_key_exists('productId', $post_data)) {
            $product_id = $post_data['productId'];
        }

        if (array_key_exists('userId', $post_data)) {
            $notify_url = sprintf($notify_url_root_v2, $post_data['distId'], $post_data['userId'], $product_id);
        } else {
            $notify_url = sprintf($notify_url_root_v1, $post_data['distId']);
        }

        $subject = $post_data["subject"];
        $body = $post_data["body"];
        $totalFee = $post_data["total_fee"];
        $out_trade_no = $post_data['out_trade_no'];        

        $signData = "partner=" . "\"" . $partner ."\"";
        $signData .= "&";
        $signData .= "seller=" . "\"" .$seller . "\"";
        $signData .= "&";
        $signData .= "out_trade_no=" . "\"" . $out_trade_no ."\"";
        $signData .= "&";
        $signData .= "subject=" . "\"" . $subject ."\"";
        $signData .= "&";
        $signData .= "body=" . "\"" . $body ."\"";
        $signData .= "&";
        $signData .= "total_fee=" . "\"" . $totalFee ."\"";
        $signData .= "&";
        $signData .= "notify_url=" . "\"" . $notify_url ."\"";
        
        return $signData;
    }

?>
