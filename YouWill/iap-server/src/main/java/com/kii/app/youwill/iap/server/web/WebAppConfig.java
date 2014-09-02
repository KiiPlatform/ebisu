package com.kii.app.youwill.iap.server.web;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurationSupport;
import org.springframework.web.servlet.mvc.method.annotation.RequestMappingHandlerMapping;

/**
 * Created by ethan on 14-8-11.
 */

@Configuration
public class WebAppConfig extends WebMvcConfigurationSupport {



	@Override
	@Bean
	public RequestMappingHandlerMapping requestMappingHandlerMapping() {
		return new RequestMappingForPropConfigHandlerMapping();
	}

}
