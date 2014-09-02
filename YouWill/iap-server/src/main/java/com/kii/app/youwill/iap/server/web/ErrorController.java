package com.kii.app.youwill.iap.server.web;

import com.kii.platform.ufp.HttpHeaderConst;
import org.apache.commons.io.IOUtils;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;

import javax.servlet.http.HttpServletResponse;

@Controller
@RequestMapping(value = "/errors", method = RequestMethod.POST)
public class ErrorController {

	@RequestMapping(value = "/error_400", consumes = MediaType.ALL_VALUE)
	public void handle400(HttpServletResponse res) throws Exception {
		send(res, "BAD_REQUEST",
				"The request cannot be fulfilled due to bad syntax");
	}

	@RequestMapping(value = "/error_401", consumes = MediaType.ALL_VALUE)
	public void handle401(HttpServletResponse res) throws Exception {
		send(res, "UNAUTHORIZED", "The request must be authenticated");
	}

	@RequestMapping(value = "/error_403", consumes = MediaType.ALL_VALUE)
	public void handle403(HttpServletResponse res) throws Exception {
		send(res, "FORBIDDEN",
				"Generic error - Access to the requested resource is forbidden");
	}

	@RequestMapping(value = "/error_404", consumes = MediaType.ALL_VALUE)
	public void handle404(HttpServletResponse res) throws Exception {
		send(res, "NOT_FOUND",
				"Generic error - The requested resource cannot be found");
	}

	@RequestMapping(value = "/error_405", consumes = MediaType.ALL_VALUE)
	public void handle405(HttpServletResponse res) throws Exception {
		send(res, "METHOD_NOT_ALLOWED",
				"The requested method is not allowed over this resource");
	}

	@RequestMapping(value = "/error_406", consumes = MediaType.ALL_VALUE)
	public void handle406(HttpServletResponse res) throws Exception {
		send(res,
				"NOT_ACCEPTABLE",
				"Generic error - The requested resource is only capable of generating content not acceptable according to the Accept headers sent in the request");
	}

	@RequestMapping(value = "/error_408", consumes = MediaType.ALL_VALUE)
	public void handle408(HttpServletResponse res) throws Exception {
		send(res, "REQUEST_TIMEOUT",
				"Generic error - The server timed out waiting for the request");
	}

	@RequestMapping(value = "/error_409", consumes = MediaType.ALL_VALUE)
	public void handle409(HttpServletResponse res) throws Exception {
		send(res, "CONFLICT",
				"Generic error - The request could not be processed because of conflict");
	}

	@RequestMapping(value = "/error_410", consumes = MediaType.ALL_VALUE)
	public void handle410(HttpServletResponse res) throws Exception {
		send(res,
				"GONE",
				"Generic error - The resource requested is no longer available and will not be available again");
	}

	@RequestMapping(value = "/error_411", consumes = MediaType.ALL_VALUE)
	public void handle411(HttpServletResponse res) throws Exception {
		send(res,
				"LENGTH_REQUIRED",
				"Generic error - The request did not specify the length of its content, which is required by the requested resource.");
	}

	@RequestMapping(value = "/error_412", consumes = MediaType.ALL_VALUE)
	public void handle412(HttpServletResponse res) throws Exception {
		send(res,
				"PRECONDITION_FAILED",
				"Generic error - The server does not meet one of the preconditions that the requester put on the request");
	}

	@RequestMapping(value = "/error_413", consumes = MediaType.ALL_VALUE)
	public void handle413(HttpServletResponse res) throws Exception {
		send(res,
				"REQUEST_ENTITY_TO_LARGE",
				"Generic error - The request is larger than the server is willing or able to process");
	}

	@RequestMapping(value = "/error_414", consumes = MediaType.ALL_VALUE)
	public void handle414(HttpServletResponse res) throws Exception {
		send(res, "REQUEST_URI_TOO_LONG",
				"Generic error - The URI provided was too long for the server to process");
	}

	@RequestMapping(value = "/error_415", consumes = MediaType.ALL_VALUE)
	public void handle415(HttpServletResponse res) throws Exception {
		send(res,
				"UNSUPPORTED_MEDIA_TYPE",
				"Generic error - The request entity has a media type which the server or resource does not support.");
	}

	@RequestMapping(value = "/error_416", consumes = MediaType.ALL_VALUE)
	public void handle416(HttpServletResponse res) throws Exception {
		send(res, "REQUESTED_RANGE_NOT_SATISFIABLE",
				"Generic error - The server cannot supply the requested resource subset");
	}

	@RequestMapping(value = "/error_417", consumes = MediaType.ALL_VALUE)
	public void handle417(HttpServletResponse res) throws Exception {
		send(res,
				"EXPECTATION_FAILED",
				"Generic error - The server cannot meet the requirements of the Expect request-header field.");
	}

	@RequestMapping(value = "/error_429", consumes = MediaType.ALL_VALUE)
	public void handle429(HttpServletResponse res) throws Exception {
		send(res, "TOO_MANY_REQUESTS",
				"Generic error - Too many requests have been received");
	}

//	@RequestMapping(value = "/error_500", consumes = MediaType.ALL_VALUE)
//	public void handle500(HttpServletResponse res) throws Exception {
//		send(res, "INTERNAL_SERVER_ERROR",
//				"Generic error - An undefined error occured");
//	}

	@RequestMapping(value = "/error_501", consumes = MediaType.ALL_VALUE)
	public void handle501(HttpServletResponse res) throws Exception {
		send(res,
				"NOT_IMPLEMENTED",
				"Generic error - The server either does not recognise the request method, or it lacks the ability to fulfill the request");
	}

	@RequestMapping(value = "/error_502", consumes = MediaType.ALL_VALUE)
	public void handle502(HttpServletResponse res) throws Exception {
		send(res, "BAD_GATEWAY",
				"Generic error - An invalid response was received from upstream server");
	}

	@RequestMapping(value = "/error_503", consumes = MediaType.ALL_VALUE)
	public void handle503(HttpServletResponse res) throws Exception {
		send(res, "SERVICE_UNAVAILABLE",
				"Generic error - The server is currently unavailable");
	}

	@RequestMapping(value = "/error_504", consumes = MediaType.ALL_VALUE)
	public void handle504(HttpServletResponse res) throws Exception {
		send(res, "INTERNAL_SERVER_ERROR",
				"Generic error - A timely response was not received from the upstream server");
	}

	private static final void send(HttpServletResponse res, String errorCode,
								   String message) throws Exception {

		JSONObject error = new JSONObject();
		error.put("errorCode", errorCode);
		error.put("message", message);
		// use raw response handling to avoid problems w/
		// @RequestMapping#produces
		res.setHeader(HttpHeaderConst.CONTENT_TYPE, "application/json");
		IOUtils.write(error.toString(2), res.getOutputStream());
	}
}
