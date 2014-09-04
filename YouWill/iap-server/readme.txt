main
=======================

API调用入口点：

com.kii.app.youwill.iap.server.web.IAPController

回调入口点(实现框架，待调试)：

com.kii.app.youwill.iap.server.web.AlipayCallbackController

配置文件：

iapserver-cfg.properties
全局配置，相关kiicloud连接信息和sandbox前缀

iapserver-product-payserver-cfg.properties
支付服务器相关配置，alipay公钥，paypal服务器地址等

iapserver-test-payserver-cfg.properties
测试用fake服务器相关配置，配合模拟alipay server使用

=========

test

/mock   测试用fake 支付服务器实现，mock测试用factory实现

/test/dao   dao测试，可脱离kiicloud执行，使用Mock模拟kiicloud api
/test/web   web接口测试，基于mvcMock测试web接口功能
            可脱离kiicloud执行，使用Mock模拟kiicloud api

/test/intergation  集成测试，有测试console  ClientEntrance
需要先启动iap-server服务和fake payserver服务
启动方式：mvn  integration-test

或者顺次执行jetty:run任务和com.kii.app.youwill.iap.server.mock.FakePayService

由于接口修订，/test/web中测试用例需修正才能执行成功，/test/dao无问题
===========





