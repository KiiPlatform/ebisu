package com.kii.app.youwill.iap.server.web;

import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;

import javax.servlet.http.HttpServletResponse;

/**
 * Created by ethan on 14-8-7.
 */
@Controller
public class OptionsController {


	@RequestMapping(method = RequestMethod.OPTIONS, value="/util/apkparser")
	public void accessEverything(HttpServletResponse response){


		response.addHeader("Access-Control-Allow-Headers","accept, Authorization,x-kii-path,x-kii-sdk, x-kii-appid, content-type, x-kii-appkey");
		response.addHeader("Access-Control-Allow-Origin","*");
		response.addHeader("Access-Control-Expose-Headers","Content-Type, Authorization, Content-Length, X-Requested-With, ETag");
		response.addHeader("Allow","GET, HEAD, POST, PUT, DELETE, TRACE, OPTIONS, PATCH");
		response.addHeader("Access-Control-Allow-Methods","POST, GET, PUT, OPTIONS, PATCH, HEAD, DELETE");
	}
}
