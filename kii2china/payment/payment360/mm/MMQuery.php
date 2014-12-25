<?php
/**
 * M-Market计费平台
 * 外部接口Demo
 * 向MMarket平台服务器查询订单数据
 * 
 */

 /**
  *  加载 MMarket.class.php 
  */
require_once dirname(__FILE__).'/MMarket.class.php';

/**
 *  创建 class MMarket
 */
$MMarket = new MMarket();


/**
 *  MMarket平台地址
 */
 $MMarketUrl = "http://ospd.mmarket.com:8089/trust";
 

/**
 * $AppID能力应用ID
 */
 $AppID ='300001504949'; //to do

/**
 * $OrderID 订单ID
 */
 $OrderID ='11131121173920560366';//to do

/**
 * $TradeID 外部交易ID
 */
 $TradeID ='74AC085437B9ADC3A8FF226768B68999';//to do


/**
 * $OrderType 订单类型  0：测试订单;1：正式订单
 */
 $OrderType =1;//to do


 /**
  * 查询数组
  */
 $ArrData = array('MMarketUrl'=>$MMarketUrl,
                  'AppID'=>$AppID,
				  'OrderID'=>$OrderID,
				  'TradeID'=>$TradeID,
				  'OrderType'=>$OrderType
				  );
 
 /**
  * 向MMarket平台查询订单请求并获取查询结果
  * 并将查询结果转换成数组
  */
 $ArrayDataResp =  $MMarket->SyncAppOrderQuery($ArrData);
 if(is_array($ArrayDataResp)){

      //print_r($ArrayDataResp);
	 //开发者进行处理
	 //to do
 }
?>