package com.kii.app.youwill.iap.server.test.web;

import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import org.junit.Before;
import org.junit.runner.RunWith;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.test.context.ContextConfiguration;
import org.springframework.test.context.junit4.SpringJUnit4ClassRunner;
import org.springframework.test.context.web.WebAppConfiguration;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.setup.MockMvcBuilders;
import org.springframework.web.context.WebApplicationContext;

/**
 * Created by ethan on 14-8-28.
 */
@RunWith(SpringJUnit4ClassRunner.class)
@WebAppConfiguration
@ContextConfiguration(locations={
		"classpath:com/kii/app/youwill/iap/server/test/webTestContext.xml"})

public abstract  class BaseWebTest {


	@Autowired
	 BucketClient bucketClient;

	@Autowired
	 AppContext appCtx;

	@Autowired
	 WebApplicationContext wac;

	 MockMvc mockMvc;

	@Before
	public void setup()  {
		this.mockMvc = MockMvcBuilders.webAppContextSetup(this.wac).build();
	}



}
