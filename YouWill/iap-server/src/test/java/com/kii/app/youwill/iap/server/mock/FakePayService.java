package com.kii.app.youwill.iap.server.mock;

import org.eclipse.jetty.server.*;
import org.eclipse.jetty.server.handler.DefaultHandler;
import org.eclipse.jetty.server.handler.HandlerList;
import org.eclipse.jetty.servlet.FilterHolder;
import org.eclipse.jetty.servlet.ServletHandler;
import org.eclipse.jetty.servlet.ServletHolder;

/**
 * Created by ethan on 14-8-25.
 */

public class FakePayService {




	public static void main(String[] args) throws Exception
	{



		Server server = new Server(28080);

		HttpConfiguration http_config = new HttpConfiguration();
		http_config.setOutputBufferSize(32768);

		ServerConnector http = new ServerConnector(server,new HttpConnectionFactory(http_config));
		http.setPort(28080);
		http.setIdleTimeout(30000);

		HttpConfiguration https_config = new HttpConfiguration(http_config);
		https_config.addCustomizer(new SecureRequestCustomizer());

		server.setConnectors(new Connector[] { http });

		ServletHandler  servletHandler=new ServletHandler();

		AlipayServlet alipay=new AlipayServlet();
		alipay.initServlet();
		ServletHolder holder=new ServletHolder();
		holder.setServlet(alipay);
		servletHandler.addServletWithMapping(holder, "/alipay");

		PaypalFilter paypal=new PaypalFilter();
		FilterHolder filterHolder=new FilterHolder();
		filterHolder.setFilter(paypal);
		servletHandler.addFilterWithMapping(filterHolder,"/paypal/",0);

		HandlerList handlers = new HandlerList();
		handlers.setHandlers(new Handler[] {servletHandler, new DefaultHandler() });
		server.setHandler(handlers);

		server.start();
		server.join();
	}

}
