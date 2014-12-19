<?php

class MMarket {

     /**
	  * 接口信息版本号
	  */
	 const Version = '1.0.0' ;  
    
     /**
	  * 检测通知签名是否正确
	  * @params $data  数组, 服务器发送过来的数据
	  * @params $Appkey  应用App Key
	  * @return ture(签名验证正确) false(签名验证失败) 
	  */
     public function ValidSign($data,$AppKey) {
        
		 $MD5Sign = $data["MD5Sign"];
		 $TmpMD5Sign  = strtoupper(md5($data['OrderID'].'#'.$data['ChannelID'].'#'.$data['PayCode'].'#'.$AppKey));
		 if($AppKey&&$MD5Sign&&$TmpMD5Sign&&$MD5Sign==$TmpMD5Sign){
			 return true;
		 }else{
			 return false;
		 }       
    }

	/**
     * 构造返回xml
	 * @params $hRet 返回码
     */
    public function SyncAppOrderResp($hRet){
    	$data['MsgType'] = 'SyncAppOrderResp';//消息类型
    	$data['Version'] = MMarket::Version;//接口消息的版本号
    	$data['hRet'] = $hRet;
    	return $this->XmlEncode($data,'utf-8','SyncAppOrderResp');
    }

    /**
     * 订单查询接口
     * @return array $ret
     */
    public function SyncAppOrderQuery($data) {
        $url = $data['MMarketUrl'];
        $params['MsgType'] = 'Trusted2ServQueryReq';
        $params['Version'] = MMarket::Version;
        $params['AppID'] = $data['AppID'];
        $params['OrderID'] = $data['OrderID'];
        $params['TradeID'] = $data['TradeID'];
        $params['OrderType'] = $data['OrderType'];
		$paramsXml =  $this->XmlEncode($params,'utf-8','Trusted2ServQueryReq');
        $ret = $this->curl($url, $paramsXml);
		//print_R($ret);die();
        if($ret){
        	return $this->XmlToData($ret);
        }
    }
    /**
     * 向服务器post xml
     * 
     */
	private static function curl($url, $fields = NULL) {
        $ch = curl_init($url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        if ($fields) {
        	$header = array();
			$header[] = "Content-type: text/xml";	//定义content-type为xml
            curl_setopt($ch, CURLOPT_POST, true);
            curl_setopt($ch, CURLOPT_HTTPHEADER, $header);
            curl_setopt($ch, CURLOPT_POSTFIELDS, $fields);
            curl_setopt($ch, CURLOPT_HEADER, 0);
        }
        $ret = curl_exec($ch);
		if (! curl_errno ( $ch )) {
			$curl_code = curl_getinfo ( $ch, CURLINFO_HTTP_CODE );
			if ($curl_code === 200) {
				return $ret;
			} else {
				return false;
			}
			curl_close ( $ch );
		} else {
			curl_close ( $ch );
			return false;
		}
    }
    /**
     * 解释xml
     * simplexml_load_string
     * 
     */
    public function XmlToData($xmlstr){
    	$data=simplexml_load_string($xmlstr);
    	return (array)$data;
    }
	/**
	 * XML编码
	 * @param mixed $data 数据
	 * @param string $encoding 数据编码
	 * @param string $root 根节点名
	 * @return string
	 */
	private function XmlEncode($data, $encoding='utf-8', $root='mmarket') {
	    $xml    = '<?xml version="1.0" encoding="' . $encoding . '"?>';
	    $xml   .= '<'.$root.'>';
	    $xml   .= $this->DataToXml($data);
	    $xml   .= '</'. $root .'>';
	    return $xml;
	}
	
	/**
	 * 数据XML编码
	 * @param mixed $data 数据
	 * @return string
	 */
	private function DataToXml($data) {
	    $xml = '';
	    foreach ($data as $key => $val) {
	        is_numeric($key) && $key = "item id=\"$key\"";
	        $xml    .=  "<$key>";
	        $xml    .=  ( is_array($val) || is_object($val)) ? $this->DataToXml($val) : $val;
	        list($key, ) = explode(' ', $key);
	        $xml    .=  "</$key>";
	    }
	    return $xml;
	}
}

?>