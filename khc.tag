<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.9.8">
  <compound kind="file">
    <name>khc.h</name>
    <path>include/</path>
    <filename>khc_8h.html</filename>
    <includes id="khc__socket__callback_8h" name="khc_socket_callback.h" local="yes" import="no" module="no" objc="no">khc_socket_callback.h</includes>
    <class kind="struct">khc_slist</class>
    <class kind="struct">khc</class>
    <member kind="typedef">
      <type>size_t(*</type>
      <name>KHC_CB_WRITE</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a57e8d1b5da9a4222190c74f8fb87a304</anchor>
      <arglist>)(char *buffer, size_t size, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>size_t(*</type>
      <name>KHC_CB_READ</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a3b82452e6a8981c670e9eef8450a7ce4</anchor>
      <arglist>)(char *buffer, size_t size, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>size_t(*</type>
      <name>KHC_CB_HEADER</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a28da65fe4a96bed6b6ac8150b9481a20</anchor>
      <arglist>)(char *buffer, size_t size, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>struct khc_slist</type>
      <name>khc_slist</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a4a194b9bfb06071b09552f46a2d36c72</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>khc_slist *(*</type>
      <name>KHC_CB_SLIST_ALLOC</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a14e10ebf817cb0b1dc8dca9ce3794b9c</anchor>
      <arglist>)(const char *str, size_t str_length, void *data)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>KHC_CB_SLIST_FREE</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a32bafb6cf1da963a57d8a167efe9f833</anchor>
      <arglist>)(khc_slist *node, void *data)</arglist>
    </member>
    <member kind="typedef">
      <type>enum khc_state</type>
      <name>khc_state</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a1f0056ed7b4fc90f5a3159d5613418c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum khc_code</type>
      <name>khc_code</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a4ed30f721b5f5af350c4187f683964bb</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct khc</type>
      <name>khc</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a610e0ad9827b3e1fb1303d42fe7c636c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>khc_state</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a857caeeab83df303d0fe3a7d97601d0f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>khc_code</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_OK</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789ad362084b96bd996b692883323f38a71d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_SOCK_CONNECT</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789a63860890be328ec70b59ee94e6b3fa79</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_SOCK_CLOSE</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789a8371726af4f71dd9286834d572befa8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_SOCK_SEND</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789a23f70a0ca5536d5538cd259c0988a5d1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_SOCK_RECV</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789aa19da1c09bffcd5bf90a4a52295e8054</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_HEADER_CALLBACK</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789a87e69245b830da2d3cfee093ee5db0ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_WRITE_CALLBACK</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789af5c7c4c210efc30b9d3cf642a0342f52</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_ALLOCATION</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789a312c4dc6d68bf167f83aedc8def54cb1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_TOO_LARGE_DATA</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a90be23875108e9a39942c3af90a6d789abdc61249fe00e89ee9f2d00f11561e13</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>khc_slist *</type>
      <name>khc_cb_slist_alloc</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a1cf6aced91a66a1e204724907a433975</anchor>
      <arglist>(const char *str, size_t str_len, void *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_cb_slist_free</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>af8ee1aa7bad924b214164a031a63bcf8</anchor>
      <arglist>(khc_slist *slist, void *data)</arglist>
    </member>
    <member kind="function">
      <type>khc_slist *</type>
      <name>khc_slist_append</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a0c00d03901759308af4a906ad3a4bf57</anchor>
      <arglist>(khc_slist *slist, const char *string, size_t length)</arglist>
    </member>
    <member kind="function">
      <type>khc_slist *</type>
      <name>khc_slist_append_using_cb_alloc</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ae829b7d6c6b4adf4c43c5ce2084a1cfa</anchor>
      <arglist>(khc_slist *slist, const char *string, size_t length, KHC_CB_SLIST_ALLOC cb_alloc, void *cb_alloc_data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_slist_free_all</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a0697b94cf117f1e6475d4b6a281fdab6</anchor>
      <arglist>(khc_slist *slist)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_slist_free_all_using_cb_free</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a601f7b3c5298824462683436df2807fa</anchor>
      <arglist>(khc_slist *slist, KHC_CB_SLIST_FREE cb_free, void *cb_free_data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_init</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a5cb75ed1f039e02286cacb8d097f233e</anchor>
      <arglist>(khc *khc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_reset_except_cb</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>aea7b6dde93cd97fc303a90c1414dba21</anchor>
      <arglist>(khc *khc)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_perform</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a75fd843af7f3de58bcba2d348878becc</anchor>
      <arglist>(khc *khc)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_host</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a67fb3bf7ff0b65085e639aa5a1465cc3</anchor>
      <arglist>(khc *khc, const char *host)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_path</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a66d6d377286feccbf320117f7ffa1692</anchor>
      <arglist>(khc *khc, const char *path)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_method</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a3f16b241faeeb8e623e605fac9ed3901</anchor>
      <arglist>(khc *khc, const char *method)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_req_headers</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ac28ce7ac10ab32d62f0bdc0c5f3915c2</anchor>
      <arglist>(khc *khc, khc_slist *headers)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_resp_header_buff</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>afd2065be9638acf043438ac9e020d3b8</anchor>
      <arglist>(khc *khc, char *buffer, size_t buff_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_stream_buff</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a59a8813c4c65fb2097b3461e5303d4fa</anchor>
      <arglist>(khc *khc, char *buffer, size_t buff_size)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_cb_sock_connect</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a54f8f9bdcb11eead39b1665d705c5465</anchor>
      <arglist>(khc *khc, KHC_CB_SOCK_CONNECT cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_cb_sock_send</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a02713f2a404eb6f1487ecef3ce5fe2d9</anchor>
      <arglist>(khc *khc, KHC_CB_SOCK_SEND cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_cb_sock_recv</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>af922aa613df95d2c8bb10a2844d1d399</anchor>
      <arglist>(khc *khc, KHC_CB_SOCK_RECV cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_cb_sock_close</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a0902ecf7e31cc403467fafbb7c287c6d</anchor>
      <arglist>(khc *khc, KHC_CB_SOCK_CLOSE cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_cb_read</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a1ea30cbdfd91b2ba3bb156bfa693bcab</anchor>
      <arglist>(khc *khc, KHC_CB_READ cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_cb_write</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ac6acc9ee82789a8986a4a2b7e586a6f0</anchor>
      <arglist>(khc *khc, KHC_CB_WRITE cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>khc_code</type>
      <name>khc_set_cb_header</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>afe54c0defe07585305e7a5c52a772b7f</anchor>
      <arglist>(khc *khc, KHC_CB_HEADER cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_enable_insecure</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a9bdd7cfba271620aa0bcaadae490936b</anchor>
      <arglist>(khc *khc, int enable_insecure)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>khc_get_status_code</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>afa3313d6c8555ee7aedb75870992c449</anchor>
      <arglist>(khc *khc)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>khc_method_can_have_body</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>abb9d159ba3fb35e950c0eaf48ad746d1</anchor>
      <arglist>(khc *khc)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>khc_socket_callback.h</name>
    <path>include/</path>
    <filename>khc__socket__callback_8h.html</filename>
    <member kind="typedef">
      <type>enum khc_sock_code_t</type>
      <name>khc_sock_code_t</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>af497c50fbe34dcc5990d93c1e0c61909</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>khc_sock_code_t(*</type>
      <name>KHC_CB_SOCK_CONNECT</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>ae9552240ec64ad637e77cd63c3b65c65</anchor>
      <arglist>)(void *sock_ctx, const char *host, unsigned int port)</arglist>
    </member>
    <member kind="typedef">
      <type>khc_sock_code_t(*</type>
      <name>KHC_CB_SOCK_SEND</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a96345c64c6bcec1711d2ddaac43224c2</anchor>
      <arglist>)(void *sock_ctx, const char *buffer, size_t length, size_t *out_sent_length)</arglist>
    </member>
    <member kind="typedef">
      <type>khc_sock_code_t(*</type>
      <name>KHC_CB_SOCK_RECV</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a148cf4bd26b43909c9eab71f37b4cda6</anchor>
      <arglist>)(void *sock_ctx, char *buffer, size_t length_to_read, size_t *out_actual_length)</arglist>
    </member>
    <member kind="typedef">
      <type>khc_sock_code_t(*</type>
      <name>KHC_CB_SOCK_CLOSE</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>af54fb240d589fb1044188bd4a81ab78d</anchor>
      <arglist>)(void *sock_ctx)</arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>khc_sock_code_t</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a13e4505815712d86adb95ab0ea867cf2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_SOCK_OK</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a13e4505815712d86adb95ab0ea867cf2aad80f9912fd8afc2b80d157cee2c679e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_SOCK_FAIL</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a13e4505815712d86adb95ab0ea867cf2a1cc5f5fa5f38faab5fa245a2e06e9d4a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_SOCK_AGAIN</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a13e4505815712d86adb95ab0ea867cf2a90a3f6acf289ae5d18feee3a67e69a1c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>khc</name>
    <filename>structkhc.html</filename>
  </compound>
  <compound kind="struct">
    <name>khc_slist</name>
    <filename>structkhc__slist.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>data</name>
      <anchorfile>structkhc__slist.html</anchorfile>
      <anchor>afdc9bd8e86890913ee8949a9275bf0b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>struct khc_slist *</type>
      <name>next</name>
      <anchorfile>structkhc__slist.html</anchorfile>
      <anchor>ad2427c9cd37a7ef8cc829d8cc652a1fd</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="page">
    <name>index</name>
    <title>khc</title>
    <filename>index.html</filename>
    <docanchor file="index.html" title="khc">md__r_e_a_d_m_e</docanchor>
  </compound>
</tagfile>
