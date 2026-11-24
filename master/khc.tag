<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.15.0">
  <compound kind="file">
    <name>khc.h</name>
    <path>include/</path>
    <filename>khc_8h.html</filename>
    <class kind="struct">khc_t</class>
    <member kind="define">
      <type>#define</type>
      <name>KHC_HOST_MAX_SIZE</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a45dcbc2b1d425f1eaf7e3b5f9c1a84c9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KHC_PATH_MAX_SIZE</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a2c6c003904f5a7a3d4ecca7eaa0093e4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KHC_METHOD_MAX_SIZE</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ae34f7ab4d0288491b284718bb5973415</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KHC_AGAIN_WAIT_MS</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ada01f6a443b3a0a8667b608fc2db8f25</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KHC_REQ_BODY_LENGTH_UNKNOWN</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a0fefd590b69df986dda16f6b8356a0a9</anchor>
      <arglist></arglist>
    </member>
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
      <type>bool(*</type>
      <name>KHC_CB_CONTINUE</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a43421b59d8e43f1a9688a3a34fae6c96</anchor>
      <arglist>)(void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>enum khc_state_t</type>
      <name>khc_state_t</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>afa8116f529173b1fd2b1372737499d05</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum khc_code_t</type>
      <name>khc_code_t</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>abcf7a290f75b7ed3b52e55a2b285fc63</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct khc_t</type>
      <name>khc_t</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a1ff50fdd5482a253d7c1431721a4b823</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>khc_state_t</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>aed8253a06a7a9cf7688ce2c95504e704</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>khc_code_t</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_OK</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6cad362084b96bd996b692883323f38a71d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_SOCK_CONNECT</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6ca63860890be328ec70b59ee94e6b3fa79</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_SOCK_CLOSE</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6ca8371726af4f71dd9286834d572befa8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_SOCK_SEND</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6ca23f70a0ca5536d5538cd259c0988a5d1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_SOCK_RECV</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6caa19da1c09bffcd5bf90a4a52295e8054</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_HEADER_CALLBACK</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6ca87e69245b830da2d3cfee093ee5db0ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_WRITE_CALLBACK</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6caf5c7c4c210efc30b9d3cf642a0342f52</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_ALLOCATION</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6ca312c4dc6d68bf167f83aedc8def54cb1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_TOO_LARGE_DATA</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6cabdc61249fe00e89ee9f2d00f11561e13</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_READ_CALLBACK</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6ca3612ca2692ccace4e8e11157cf04c6bb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_TIMEOUT</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6ca257ea24324a4076f4020ca8348488592</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_ERR_CANCELLED</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a238bb716e422ba9853f65b7860f9dd6ca22c1ba4ceba113d4b6c4e025ce798f22</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_init</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ad413a75131c9b3c9206949044dd1e7a9</anchor>
      <arglist>(khc_t *khc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_reset_except_cb</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ace4678e5617fad499c0f401bb9829ea0</anchor>
      <arglist>(khc_t *khc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_reset_except_request</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ad48d9501d10ae35d377c9e29eb30cb8c</anchor>
      <arglist>(khc_t *khc)</arglist>
    </member>
    <member kind="function">
      <type>khc_code_t</type>
      <name>khc_perform</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ac2eccf72ab3ae08ee7aa026d47d0a34e</anchor>
      <arglist>(khc_t *khc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_system</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ae310e3fc9866494b832a99fb6ab050e9</anchor>
      <arglist>(khc_t *khc, ebisu_system_t *system)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_logger</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a8c5f39e7a4cd90deff0ab097e61abd4b</anchor>
      <arglist>(khc_t *khc, ebisu_logger_t *logger)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_timeout_ms</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a9800528067f95cd677dca75ab5508364</anchor>
      <arglist>(khc_t *khc, uint32_t timeout_ms)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_cb_continue</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a1e1874687a53076b6697ab05cd84840d</anchor>
      <arglist>(khc_t *khc, KHC_CB_CONTINUE cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>khc_code_t</type>
      <name>khc_set_host</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a1e20410fe644e36da420e505e400adf1</anchor>
      <arglist>(khc_t *khc, const char *host)</arglist>
    </member>
    <member kind="function">
      <type>khc_code_t</type>
      <name>khc_set_path</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ae327e6a3b0b409031cd97c4f0b7b94cd</anchor>
      <arglist>(khc_t *khc, const char *path)</arglist>
    </member>
    <member kind="function">
      <type>khc_code_t</type>
      <name>khc_set_method</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a0e392233d624405b991eab885cc706c3</anchor>
      <arglist>(khc_t *khc, const char *method)</arglist>
    </member>
    <member kind="function">
      <type>khc_code_t</type>
      <name>khc_set_req_headers</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a8ac193aa04c1942996c6f4a2b01b8dfd</anchor>
      <arglist>(khc_t *khc, const char *headers, size_t length)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_resp_header_buff</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a5461bcbb5b7da247bed5c0a3ee1fd47d</anchor>
      <arglist>(khc_t *khc, char *buffer, size_t buff_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_stream_buff</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>acd65a7b58f771603a15f972315e78614</anchor>
      <arglist>(khc_t *khc, char *buffer, size_t buff_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_socket</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>aae1eb51fb3d9df3f416955b0bbd3a73f</anchor>
      <arglist>(khc_t *khc, ebisu_socket_t *socket)</arglist>
    </member>
    <member kind="function">
      <type>khc_code_t</type>
      <name>khc_set_cb_read</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ab3092de311938f34922143408e4a36d3</anchor>
      <arglist>(khc_t *khc, KHC_CB_READ cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_set_req_body_length</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a736428a1e77b3369fe0e7e7b0a549710</anchor>
      <arglist>(khc_t *khc, size_t length)</arglist>
    </member>
    <member kind="function">
      <type>khc_code_t</type>
      <name>khc_set_cb_write</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ae49341971ce08a8226df6d003e11a3ca</anchor>
      <arglist>(khc_t *khc, KHC_CB_WRITE cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>khc_code_t</type>
      <name>khc_set_cb_header</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a1ce534b8e234038975581e93d75204d8</anchor>
      <arglist>(khc_t *khc, KHC_CB_HEADER cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_enable_insecure</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a52e531a218a49a7532af53e5087445de</anchor>
      <arglist>(khc_t *khc, bool enable_insecure)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>khc_enable_absolute_target</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ae52a7f9a211e4496cc49bf482fb19f82</anchor>
      <arglist>(khc_t *khc, bool enable_absolute_target)</arglist>
    </member>
    <member kind="function">
      <type>uint16_t</type>
      <name>khc_get_status_code</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a2a0636e5c349015914cf18a1e2226541</anchor>
      <arglist>(khc_t *khc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>khc_get_close_failed</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>aaf6fce0c1d3a35e8ff4762238e3e5b5f</anchor>
      <arglist>(khc_t *khc)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_sock_error_t</type>
      <name>khc_get_sock_error</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a828574d36f71896b7d8751bd716b1153</anchor>
      <arglist>(khc_t *khc, long *out_detail)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>khc_get_resp_header_skipped</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a1d2510e5cc637515a7f3cc4da99e9ff5</anchor>
      <arglist>(khc_t *khc)</arglist>
    </member>
    <member kind="function">
      <type>int64_t</type>
      <name>khc_get_resp_date</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>ae4d393d99631763e36e32294fea1b4e9</anchor>
      <arglist>(khc_t *khc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>khc_method_can_have_body</name>
      <anchorfile>khc_8h.html</anchorfile>
      <anchor>a3230aadba14f21f809647a56149ed01a</anchor>
      <arglist>(khc_t *khc)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>khc_t</name>
    <filename>structkhc__t.html</filename>
  </compound>
  <compound kind="page">
    <name>index</name>
    <title>khc</title>
    <filename>index.html</filename>
    <docanchor file="index.html" title="khc">khc_readme</docanchor>
  </compound>
</tagfile>
