package com.kii.app.youwill.iap.server.alipay;

/* *
 *类名：YouWillAlipayConfig
 *功能：基础配置类
 *详细：设置帐户有关信息及返回路径
 *版本：3.3
 *日期：2012-08-10
 *说明：
 *以下代码只是为了方便商户测试而提供的样例代码，商户可以根据自己网站的需要，按照技术文档编写,并非一定要使用该代码。
 *该代码仅供学习和研究支付宝接口使用，只是提供一个参考。
	
 *提示：如何获取安全校验码和合作身份者ID
 *1.用您的签约支付宝账号登录支付宝网站(www.alipay.com)
 *2.点击“商家服务”(https://b.alipay.com/order/myOrder.htm)
 *3.点击“查询合作者身份(PID)”、“查询安全校验码(Key)”

 *安全校验码查看时，输入支付密码后，页面呈灰色的现象，怎么办？
 *解决方法：
 *1、检查浏览器配置，不让浏览器做弹框屏蔽设置
 *2、更换浏览器或电脑，重新登录查询。
 */

public class YouWillAlipayConfig {
	
	//↓↓↓↓↓↓↓↓↓↓请在这里配置您的基本信息↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓
	// 合作身份者ID，以2088开头由16位纯数字组成的字符串
	public static String partner = "2088701768276994";
	// 商户的私钥
	public static String private_key = "MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBAMWDVNo9/9kZbrLAmFP0SUy6zk0F42/SgRUTmuLQnvXQ+zcUv6OJ/gNULAfFVDdXFXuSXMVMMqzmcOgU2pY9WV5klYmq+2GIm4sEF57aTHvmuFq10iCqwlVsH99XHasKWxDi73Z+OxMCUk23RwmZa7NCapHNBbz0LMjpjX9j7IqrAgMBAAECgYAscaf3T/oxROrG6FWelpqj3GktpcSO1gRb1MSGg0ExuSu+MjoJN7Tj1N3vhbS0mzDnVIDxmHrSsyv23TJumjFwieWE5lFcSm1R/H7RljGg47zSYgJPQHYlqN6rPgytmoO9H/bgWR6qoQ3lQAo+Yv0LuPLzRNzbwyAM+1Wve7f4QQJBAOSS7tXnaSqna3xxeaq9JP13XN36hTekwdLL3W1AXbRaX5HTRGkZkGUbrPHoGVIA1Kya2KJt9wKEY+UlRBD7swsCQQDdNk4Jcm4Ie3wuN2hmwl/3aCcF0L7m06hpotFG9vKy55RrIcerSj1oIutz1506Lpo5Rhf8rMRvGbuQlzE2uUrhAkEA1tfE5gm9L/ptbWq7kWB3hEbNY1g2Hzcdd6VnW2lEqmDHs6mAg8kjj+WTiU0/f3SIcjSZqxLxOcx2mMuOTuhGQQJADh2nASdSKixnEjPq75O7t/fndZGKfQRInOUM4bBkcFIiwGtZaG7GWFzSFYROviW3Jax5X2BO8J/9vg5Hz6wnQQJBAIDGCNKngCfz5GhTUNveV7nDpgAE+aKo5v6X2gunwBLI6cBCXYxajbEcHdGWc4VWMgXotjLHlxiQ6+vmOGrLkRk=";
	
	// 支付宝的公钥，无需修改该值
	public static String ali_public_key  = "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCnxj/9qwVfgoUh/y2W89L6BkRAFljhNhgPdyPuBV64bfQNN1PjbCzkIM6qRdKBoLPXmKKMiFYnkd6rAoprih3/PrQEB/VsW8OoM8fxn67UDYuyBTqA23MML9q1+ilIZwBC2AQ2UBVOrFXfFl75p6/B5KsiNG9zpgmLCUYuLkxpLQIDAQAB";


	// 调试用，创建TXT日志文件夹路径
	public static String log_path = "/tmp";

	// 字符编码格式 目前支持 gbk 或 utf-8
	public static String input_charset = "utf-8";

	// 签名方式 不需修改
	public static String sign_type = "RSA";

}
