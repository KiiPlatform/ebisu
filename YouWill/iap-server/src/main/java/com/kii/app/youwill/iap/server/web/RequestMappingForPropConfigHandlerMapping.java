package com.kii.app.youwill.iap.server.web;

import org.apache.commons.lang3.StringUtils;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.core.annotation.AnnotationUtils;
import org.springframework.core.io.ResourceLoader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;
import org.springframework.web.servlet.mvc.condition.RequestCondition;
import org.springframework.web.servlet.mvc.method.RequestMappingInfo;
import org.springframework.web.servlet.mvc.method.annotation.RequestMappingHandlerMapping;

import javax.annotation.PostConstruct;
import java.io.IOException;
import java.lang.annotation.Annotation;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

/**
 * Created by ethan on 14-8-11.
 */
public class RequestMappingForPropConfigHandlerMapping extends RequestMappingHandlerMapping {

	@Autowired
	private ResourceLoader loader;

//	@Value("${iap-server.rest.sand-box.prefix}")
//	private String  sandBoxPrefix;


	private static class UrlInfo {
		private String url;
		private String contentType;

		public RequestMapping getMapping() {
			return new RequestMapping() {

				@Override
				public String[] value() {
					return new String[]{url};
				}

				@Override
				public RequestMethod[] method() {
					return new RequestMethod[]{RequestMethod.POST};
				}

				@Override
				public String[] params() {
					return new String[0];
				}

				@Override
				public String[] headers() {
					return new String[0];
				}

				@Override
				public String[] consumes() {
					return new String[0];
				}

				@Override
				public String[] produces() {
					return new String[]{contentType};
				}

				@Override
				public Class<? extends Annotation> annotationType() {
					return RequestMapping.class;
				}
			};
		}
	}

	private final static Map<String, UrlInfo> urlMap = new HashMap<String, UrlInfo>();

	@PostConstruct
	public void initUrlInfos() throws IOException {

		Properties prop = new Properties();


		prop.load(loader.getResource("classpath:com/kii/app/youwill/iap/server/iapserver-endpoint.properties").getInputStream());

		for (String key : prop.stringPropertyNames()) {

			String[] arr = StringUtils.split(key, ".");
			if (arr.length != 4) {
				continue;
			}
			if ("iap-server".equals(arr[0]) && "rest".equals(arr[1])) {

				if (!urlMap.containsKey(arr[2])) {
					urlMap.put(arr[2], new UrlInfo());
				}

				UrlInfo urlInfo = urlMap.get(arr[2]);

				if ("end-point".equals(arr[3])) {
					urlInfo.url = prop.getProperty(key);
				} else if ("content-type".equals(arr[3])) {
					urlInfo.contentType = prop.getProperty(key);
				}
			}
		}
	}


	@Override
	protected RequestMappingInfo getMappingForMethod(Method method, Class<?> handlerType) {
		RequestMappingProp methodProp = AnnotationUtils.findAnnotation(method, RequestMappingProp.class);
		if (methodProp != null) {

			RequestCondition<?> methodCondition = getCustomMethodCondition(method);
			RequestMapping methodAnnotation = urlMap.get(methodProp.value()).getMapping();

			RequestMappingInfo info = createRequestMappingInfo(methodAnnotation, methodCondition);


			return info;

		} else {
			return super.getMappingForMethod(method, handlerType);

		}
	}




}
