<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.15.0">
  <compound kind="file">
    <name>ebisu_clock.h</name>
    <path>include/</path>
    <filename>ebisu__clock_8h.html</filename>
    <includes id="ebisu__system_8h" name="ebisu_system.h" local="yes" import="no" module="no" objc="no">ebisu_system.h</includes>
    <class kind="struct">ebisu_clock_t</class>
    <member kind="typedef">
      <type>struct ebisu_clock_t</type>
      <name>ebisu_clock_t</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a01e37dc83e0abae84e96c4912fa1f687</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum ebisu_validity_t</type>
      <name>ebisu_validity_t</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a1cbb339f4419d4fe901d9ca09fa70573</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ebisu_validity_t</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a47f1e6284ca7728f3288cd6e117ad21f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_VALIDITY_OK</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a47f1e6284ca7728f3288cd6e117ad21fa98ee80daf2d7954ad5c8fadb2f3076b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_VALIDITY_EXPIRED</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a47f1e6284ca7728f3288cd6e117ad21faf5b66643f3b20a24140cfba2247cfe90</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_VALIDITY_NOT_YET_VALID</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a47f1e6284ca7728f3288cd6e117ad21fac5f12e616a91de19f45ef0cb95a27344</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_clock_init</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a4c85dfcb6d4b520957c2bf1ee26f5414</anchor>
      <arglist>(ebisu_clock_t *clock, ebisu_system_t *system)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_clock_set_lock</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a3c26369c70ce325e9d32dfa68122a9cb</anchor>
      <arglist>(ebisu_clock_t *clock, ebisu_lock_t *lock)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_clock_set_floor</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a0cf4c12578e20cf46a64aa39871145c1</anchor>
      <arglist>(ebisu_clock_t *clock, int64_t floor)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_clock_set_exact_for</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a7b8c62d02d3b1f1d10b33f6c9a31d5f2</anchor>
      <arglist>(ebisu_clock_t *clock, uint32_t seconds)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_clock_set_time</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>ad62b4e4b23622cb75c18ee859ccd565f</anchor>
      <arglist>(ebisu_clock_t *clock, int64_t now)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_clock_get_time</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a45709ee63c0559d9f29e5c199e1085bc</anchor>
      <arglist>(ebisu_clock_t *clock, int64_t *out_now)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_clock_forget_time</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>aa00ab1f7611766edb574494ca947f07d</anchor>
      <arglist>(ebisu_clock_t *clock)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_validity_t</type>
      <name>ebisu_clock_check_validity</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a5a05722564b0b889afc6c506ecc73435</anchor>
      <arglist>(ebisu_clock_t *clock, int64_t not_before, int64_t not_after)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_log.h</name>
    <path>include/</path>
    <filename>ebisu__log_8h.html</filename>
    <class kind="struct">ebisu_logger_ops_t</class>
    <class kind="struct">ebisu_logger_t</class>
    <class kind="struct">ebisu_log_fanout_t</class>
    <member kind="typedef">
      <type>enum ebisu_log_level_t</type>
      <name>ebisu_log_level_t</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a1e989cf944c554850be22c00495c28b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_logger_t</type>
      <name>ebisu_logger_t</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a94ac6ad513e2d836f9bbe0366ea47b44</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_logger_ops_t</type>
      <name>ebisu_logger_ops_t</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a6a4379602c0457f39fa3a4eef936cc32</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_log_fanout_t</type>
      <name>ebisu_log_fanout_t</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a9dd37a633c3f347ef4cab58eeb55f93b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ebisu_log_level_t</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a42e3fb546ca916214c4afd79eddce535</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_LOG_ERROR</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a42e3fb546ca916214c4afd79eddce535a9f54b49b57b006ccef48132d4180ba07</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_LOG_WARNING</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a42e3fb546ca916214c4afd79eddce535a2ed83f71fe32b50977e89849e8edd219</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_LOG_INFO</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a42e3fb546ca916214c4afd79eddce535ababfb518b73f6be08d795ad16c9b36aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_LOG_DEBUG</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a42e3fb546ca916214c4afd79eddce535a72cb7f90a0acc64fa2d563cefea16a7a</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_log</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a221be842a08c6b23d5083d8dad9ac203</anchor>
      <arglist>(ebisu_logger_t *logger, ebisu_log_level_t level, const char *fmt,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_log_fanout_init</name>
      <anchorfile>ebisu__log_8h.html</anchorfile>
      <anchor>a4ee7810e4c2a3c1949e7fa1c969ffa4f</anchor>
      <arglist>(ebisu_log_fanout_t *fanout, ebisu_logger_t *const *loggers, size_t count)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_mem.h</name>
    <path>include/</path>
    <filename>ebisu__mem_8h.html</filename>
    <member kind="function">
      <type>void</type>
      <name>ebisu_secure_zero</name>
      <anchorfile>ebisu__mem_8h.html</anchorfile>
      <anchor>a32244a5750a1713af5af8a0a3465b5f9</anchor>
      <arglist>(void *p, size_t n)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_object.h</name>
    <path>include/</path>
    <filename>ebisu__object_8h.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>EBISU_CONTAINER_OF</name>
      <anchorfile>ebisu__object_8h.html</anchorfile>
      <anchor>a5e7fdb772cc16ee485b366e1618a7136</anchor>
      <arglist>(ptr, type, member)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_socket.h</name>
    <path>include/</path>
    <filename>ebisu__socket_8h.html</filename>
    <includes id="ebisu__object_8h" name="ebisu_object.h" local="yes" import="no" module="no" objc="no">ebisu_object.h</includes>
    <class kind="struct">ebisu_socket_ops_t</class>
    <class kind="struct">ebisu_socket_t</class>
    <member kind="typedef">
      <type>enum ebisu_sock_code_t</type>
      <name>ebisu_sock_code_t</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a2845d194b5ba641d5321831147cef126</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum ebisu_sock_error_t</type>
      <name>ebisu_sock_error_t</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>af8cc3d6d42f30eea15da340261f2d657</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_socket_t</type>
      <name>ebisu_socket_t</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>af0e306cd72dfb837b228cdc12fae19a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_socket_ops_t</type>
      <name>ebisu_socket_ops_t</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>af6e42460441a6c5a4a33d9086aba41e0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ebisu_sock_code_t</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a7b634b2cbb6bc4349dab223fbee058ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_OK</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a7b634b2cbb6bc4349dab223fbee058baaba8e4dfed7fc8240ee78425110020017</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_FAIL</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a7b634b2cbb6bc4349dab223fbee058baa914e89bcb40f7ddc05c46892a3150874</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_AGAIN</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a7b634b2cbb6bc4349dab223fbee058baa3d0e5d85e07a8c0918b3b24ceaa53a44</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ebisu_sock_error_t</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a19f16391089f04ceb79936a6ecc26963</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_ERROR_NONE</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a19f16391089f04ceb79936a6ecc26963ab1081167e61254a9e5c105643e1ea671</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_ERROR_DNS</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a19f16391089f04ceb79936a6ecc26963a9372c342614a90ecd5b2bc32d982c9c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_ERROR_CONNECT</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a19f16391089f04ceb79936a6ecc26963a060e8ad7b6b55ad00dc10b1309a205a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_ERROR_TLS</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a19f16391089f04ceb79936a6ecc26963a0317b7ceb445d76314d2f153ddd59765</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_ERROR_VERIFY</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a19f16391089f04ceb79936a6ecc26963a22c267c81816001b90572b3f08bc8278</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_ERROR_TIMEOUT</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a19f16391089f04ceb79936a6ecc26963a4b34064a92d2a7a69201e8805b547887</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_ERROR_CLOSED</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a19f16391089f04ceb79936a6ecc26963ab7a641310a57f3a2e49eb2bd85746963</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_SOCK_ERROR_OTHER</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a19f16391089f04ceb79936a6ecc26963a25c85a793f6a5231da0c877f65699d19</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_str.h</name>
    <path>include/</path>
    <filename>ebisu__str_8h.html</filename>
    <class kind="struct">ebisu_strbuf_t</class>
    <class kind="struct">ebisu_span_t</class>
    <member kind="define">
      <type>#define</type>
      <name>EBISU_FIXED_MAX_DECIMALS</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a3a0b826d8230581af18a0e245964c599</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_strbuf_t</type>
      <name>ebisu_strbuf_t</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>afb7637ea30bc00a2c41e9a39883a464b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum ebisu_parse_err_t</type>
      <name>ebisu_parse_err_t</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a0665b9597a1372d128d16e50a96d710a</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_span_t</type>
      <name>ebisu_span_t</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a9a7c15b507dc86c7401f566823b004b4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ebisu_parse_err_t</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a0f1cfa4702e7646801104b7c0668e41f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_PARSE_OK</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a0f1cfa4702e7646801104b7c0668e41fac15c39f6f9582e6150a32eb123c1feeb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_PARSE_EMPTY</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a0f1cfa4702e7646801104b7c0668e41fa7fe348d416449fc51634402b55032711</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_PARSE_INVALID</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a0f1cfa4702e7646801104b7c0668e41faab3de77ee928d12b864b955c56ed2fb8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_PARSE_RANGE</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a0f1cfa4702e7646801104b7c0668e41fa25a04a6edc9ef3102c66698b778a9887</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ebisu_strlcpy</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a9745d04f60b5f273f02d225029c110df</anchor>
      <arglist>(char *dst, const char *src, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_init</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a987edf454101a781081e9108d6b4fddc</anchor>
      <arglist>(ebisu_strbuf_t *sb, char *buff, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_append</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a63a3e1fc0d9f3115f766bbdb0f2ac7ca</anchor>
      <arglist>(ebisu_strbuf_t *sb, const char *s)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_append_n</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>afc58c3dfca8065f169584c6602b42ffe</anchor>
      <arglist>(ebisu_strbuf_t *sb, const char *s, size_t n)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_append_char</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a0bedd1eefba40e41227bc9a82545fa55</anchor>
      <arglist>(ebisu_strbuf_t *sb, char c)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_append_int</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a13a7a04399b6871f1503d0bc3011ec28</anchor>
      <arglist>(ebisu_strbuf_t *sb, int64_t v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_append_uint</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a8d70c5227e7f945ff82f85f030549958</anchor>
      <arglist>(ebisu_strbuf_t *sb, uint64_t v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_append_hex</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>ad278936c67df60098937976f7128e035</anchor>
      <arglist>(ebisu_strbuf_t *sb, uint64_t v, unsigned int min_digits)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_append_hex_bytes</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a50eb363ffdf48b70bc9c10b28269496b</anchor>
      <arglist>(ebisu_strbuf_t *sb, const uint8_t *bytes, size_t n)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_append_fixed</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a2ea1b86908eeb1b2665b396219ddbbc1</anchor>
      <arglist>(ebisu_strbuf_t *sb, int64_t v, unsigned int decimals)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>ebisu_strbuf_room</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>aa73134d9c0c4cf2386aad8fdd4211ef4</anchor>
      <arglist>(ebisu_strbuf_t *sb, size_t *out_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_strbuf_commit</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>aa782a5b6f9dc243137899592ba3c7c80</anchor>
      <arglist>(ebisu_strbuf_t *sb, size_t n)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_strbuf_ok</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>accb5345552bbd23ed89c21bfe269aa8e</anchor>
      <arglist>(const ebisu_strbuf_t *sb)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_parse_err_t</type>
      <name>ebisu_parse_uint</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a286c615bd69d98934e6f6930d9594164</anchor>
      <arglist>(const char *s, size_t len, uint64_t max, uint64_t *out)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_parse_err_t</type>
      <name>ebisu_parse_int</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a2565a3df671c2f7970aeb78e0836b377</anchor>
      <arglist>(const char *s, size_t len, int64_t min, int64_t max, int64_t *out)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_parse_err_t</type>
      <name>ebisu_parse_hex</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>add4a3ea64bded35ea45ca7ed037b3061</anchor>
      <arglist>(const char *s, size_t len, uint64_t max, uint64_t *out)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_parse_err_t</type>
      <name>ebisu_parse_hex_bytes</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>ac190c169a26b65b96b7c161fb30554e7</anchor>
      <arglist>(const char *s, size_t len, uint8_t *out, size_t out_size, size_t *out_len)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_parse_err_t</type>
      <name>ebisu_parse_fixed</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a5b2fc52ed6578fd55c0b4c42577bddad</anchor>
      <arglist>(const char *s, size_t len, unsigned int decimals, int64_t min, int64_t max, int64_t *out)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_span_t</type>
      <name>ebisu_span_from_str</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a3a857b01de4d73a6981918e9b75d6672</anchor>
      <arglist>(const char *s)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_span_next</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a565d367f09afd12c8391ae9da153ded0</anchor>
      <arglist>(ebisu_span_t *rest, char delim, ebisu_span_t *field)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_span_t</type>
      <name>ebisu_span_trim</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>af46bdb83050e80e21758cb6f09ada6d8</anchor>
      <arglist>(ebisu_span_t s)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_span_eq</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a2a681adf4710e6b1342c21a034f56965</anchor>
      <arglist>(ebisu_span_t s, const char *literal)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_span_eq_nocase</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>acd68676a04fcd77f13d9ba3ab598b719</anchor>
      <arglist>(ebisu_span_t s, const char *literal)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_span_find</name>
      <anchorfile>ebisu__str_8h.html</anchorfile>
      <anchor>a867bc65afd856f0e94e9c23331a1dc22</anchor>
      <arglist>(ebisu_span_t s, const char *needle, size_t *out_offset)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_system.h</name>
    <path>include/</path>
    <filename>ebisu__system_8h.html</filename>
    <includes id="ebisu__object_8h" name="ebisu_object.h" local="yes" import="no" module="no" objc="no">ebisu_object.h</includes>
    <class kind="struct">ebisu_system_ops_t</class>
    <class kind="struct">ebisu_system_t</class>
    <class kind="struct">ebisu_lock_ops_t</class>
    <class kind="struct">ebisu_lock_t</class>
    <member kind="typedef">
      <type>enum ebisu_task_code_t</type>
      <name>ebisu_task_code_t</name>
      <anchorfile>ebisu__system_8h.html</anchorfile>
      <anchor>aaa3b1940a5a1fe2b64e28b21470d52ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void *(*</type>
      <name>EBISU_TASK_ENTRY</name>
      <anchorfile>ebisu__system_8h.html</anchorfile>
      <anchor>aa88036fe3014d1b2c19cd4808b5424d7</anchor>
      <arglist>)(void *value)</arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_system_t</type>
      <name>ebisu_system_t</name>
      <anchorfile>ebisu__system_8h.html</anchorfile>
      <anchor>a46cc0d1b6d1d6eea76e3dad3ec8115a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_system_ops_t</type>
      <name>ebisu_system_ops_t</name>
      <anchorfile>ebisu__system_8h.html</anchorfile>
      <anchor>af1cb97f5f05599dd432cd58a05b321c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_lock_t</type>
      <name>ebisu_lock_t</name>
      <anchorfile>ebisu__system_8h.html</anchorfile>
      <anchor>a9c40062bf30682a38e5e15c6a30853e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_lock_ops_t</type>
      <name>ebisu_lock_ops_t</name>
      <anchorfile>ebisu__system_8h.html</anchorfile>
      <anchor>abfd4a8c349bec0a5c1b5a97e25da50af</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ebisu_task_code_t</name>
      <anchorfile>ebisu__system_8h.html</anchorfile>
      <anchor>a90188fadee3bc027156aa75ab532454a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_TASKC_OK</name>
      <anchorfile>ebisu__system_8h.html</anchorfile>
      <anchor>a90188fadee3bc027156aa75ab532454aa21b207febbcc0f779b0754a6da63d0d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_TASKC_FAIL</name>
      <anchorfile>ebisu__system_8h.html</anchorfile>
      <anchor>a90188fadee3bc027156aa75ab532454aadb245d24463ad5c39c95dae216af46ea</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_time.h</name>
    <path>include/</path>
    <filename>ebisu__time_8h.html</filename>
    <class kind="struct">ebisu_civil_time_t</class>
    <member kind="typedef">
      <type>struct ebisu_civil_time_t</type>
      <name>ebisu_civil_time_t</name>
      <anchorfile>ebisu__time_8h.html</anchorfile>
      <anchor>ad327c82321f65eb00b002979c91e19ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_time_from_civil</name>
      <anchorfile>ebisu__time_8h.html</anchorfile>
      <anchor>a5a0f3e53e949f6cad1134f58d1df632c</anchor>
      <arglist>(int year, int month, int day, int hour, int minute, int second, int64_t *out_seconds)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_time_to_civil</name>
      <anchorfile>ebisu__time_8h.html</anchorfile>
      <anchor>ac5e28e8f61669eb81df1ed85703267a6</anchor>
      <arglist>(int64_t seconds, ebisu_civil_time_t *out)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_civil_time_t</name>
    <filename>structebisu__civil__time__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>year</name>
      <anchorfile>structebisu__civil__time__t.html</anchorfile>
      <anchor>abb4eca81fdae088b09e6ca3cdd2bef5e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>month</name>
      <anchorfile>structebisu__civil__time__t.html</anchorfile>
      <anchor>a776a99e99d9af8e39db4c460dc7ac78d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>day</name>
      <anchorfile>structebisu__civil__time__t.html</anchorfile>
      <anchor>aeadd124b187fff8daa2c04b0831af49f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>hour</name>
      <anchorfile>structebisu__civil__time__t.html</anchorfile>
      <anchor>acc1eb569fcb52c1d6b94d882db794383</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>minute</name>
      <anchorfile>structebisu__civil__time__t.html</anchorfile>
      <anchor>a5d122f787fdebe2bf6d6735d3c21530a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>second</name>
      <anchorfile>structebisu__civil__time__t.html</anchorfile>
      <anchor>a93d3b2af6194e4a47b6ff8800d0e1f89</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>weekday</name>
      <anchorfile>structebisu__civil__time__t.html</anchorfile>
      <anchor>af9b043a87c344be4a1cb12aabb3a8f79</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_clock_t</name>
    <filename>structebisu__clock__t.html</filename>
  </compound>
  <compound kind="struct">
    <name>ebisu_lock_ops_t</name>
    <filename>structebisu__lock__ops__t.html</filename>
    <member kind="variable">
      <type>void(*</type>
      <name>lock</name>
      <anchorfile>structebisu__lock__ops__t.html</anchorfile>
      <anchor>a745899b14950b7ba82f33d2cc6a85174</anchor>
      <arglist>)(ebisu_lock_t *self)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>unlock</name>
      <anchorfile>structebisu__lock__ops__t.html</anchorfile>
      <anchor>a25112960416a6dabd5b93862db66528a</anchor>
      <arglist>)(ebisu_lock_t *self)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_lock_t</name>
    <filename>structebisu__lock__t.html</filename>
    <member kind="variable">
      <type>const ebisu_lock_ops_t *</type>
      <name>ops</name>
      <anchorfile>structebisu__lock__t.html</anchorfile>
      <anchor>a71d30ddec61e4e6d4790bab56b4c0fa5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_log_fanout_t</name>
    <filename>structebisu__log__fanout__t.html</filename>
    <member kind="variable">
      <type>ebisu_logger_t</type>
      <name>base</name>
      <anchorfile>structebisu__log__fanout__t.html</anchorfile>
      <anchor>ac8ac9936c9fe790257ec129b4cb5d635</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_logger_ops_t</name>
    <filename>structebisu__logger__ops__t.html</filename>
    <member kind="variable">
      <type>void(*</type>
      <name>log</name>
      <anchorfile>structebisu__logger__ops__t.html</anchorfile>
      <anchor>af27fdf43827399fa9407f8cb6000b0e8</anchor>
      <arglist>)(ebisu_logger_t *self, ebisu_log_level_t level, const char *fmt, va_list args)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_logger_t</name>
    <filename>structebisu__logger__t.html</filename>
    <member kind="variable">
      <type>const ebisu_logger_ops_t *</type>
      <name>ops</name>
      <anchorfile>structebisu__logger__t.html</anchorfile>
      <anchor>a664af796bdac52391732995055c0d340</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_socket_ops_t</name>
    <filename>structebisu__socket__ops__t.html</filename>
    <member kind="variable">
      <type>ebisu_sock_code_t(*</type>
      <name>connect</name>
      <anchorfile>structebisu__socket__ops__t.html</anchorfile>
      <anchor>ad7c954a124d7f7029c24052e1fbdc2c2</anchor>
      <arglist>)(ebisu_socket_t *self, const char *host, uint16_t port)</arglist>
    </member>
    <member kind="variable">
      <type>ebisu_sock_code_t(*</type>
      <name>send</name>
      <anchorfile>structebisu__socket__ops__t.html</anchorfile>
      <anchor>adb543d912cc7f9758ed4713cfeed4666</anchor>
      <arglist>)(ebisu_socket_t *self, const char *buffer, size_t length, size_t *out_sent_length)</arglist>
    </member>
    <member kind="variable">
      <type>ebisu_sock_code_t(*</type>
      <name>recv</name>
      <anchorfile>structebisu__socket__ops__t.html</anchorfile>
      <anchor>a28417969117e077e747944faac38efa8</anchor>
      <arglist>)(ebisu_socket_t *self, char *buffer, size_t length_to_read, size_t *out_actual_length)</arglist>
    </member>
    <member kind="variable">
      <type>ebisu_sock_code_t(*</type>
      <name>close</name>
      <anchorfile>structebisu__socket__ops__t.html</anchorfile>
      <anchor>afab911ce1025829b38f7df1096e67e7b</anchor>
      <arglist>)(ebisu_socket_t *self)</arglist>
    </member>
    <member kind="variable">
      <type>bool(*</type>
      <name>recv_timed_out</name>
      <anchorfile>structebisu__socket__ops__t.html</anchorfile>
      <anchor>af8c5f0ab8132bb300aa61db25a4a8073</anchor>
      <arglist>)(ebisu_socket_t *self)</arglist>
    </member>
    <member kind="variable">
      <type>ebisu_sock_error_t(*</type>
      <name>last_error</name>
      <anchorfile>structebisu__socket__ops__t.html</anchorfile>
      <anchor>a7d723df34a7a3ec7b0813d4949375fda</anchor>
      <arglist>)(ebisu_socket_t *self, long *out_detail)</arglist>
    </member>
    <member kind="variable">
      <type>bool(*</type>
      <name>recv_wait</name>
      <anchorfile>structebisu__socket__ops__t.html</anchorfile>
      <anchor>a4b245d596fc7e2c6937a70794b6cd031</anchor>
      <arglist>)(ebisu_socket_t *self, uint32_t timeout_ms)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_socket_t</name>
    <filename>structebisu__socket__t.html</filename>
    <member kind="variable">
      <type>const ebisu_socket_ops_t *</type>
      <name>ops</name>
      <anchorfile>structebisu__socket__t.html</anchorfile>
      <anchor>aff2c41c492bffd29686c2970f4a4a375</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_span_t</name>
    <filename>structebisu__span__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>ptr</name>
      <anchorfile>structebisu__span__t.html</anchorfile>
      <anchor>a6207e249a9dd0f045c9e77fd06a3876f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>len</name>
      <anchorfile>structebisu__span__t.html</anchorfile>
      <anchor>aa597a17c8246dc8eede20ae9ecbbc0e7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_strbuf_t</name>
    <filename>structebisu__strbuf__t.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>buff</name>
      <anchorfile>structebisu__strbuf__t.html</anchorfile>
      <anchor>a0387a9ac7fc90b16ebd2ce20af50eccd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>size</name>
      <anchorfile>structebisu__strbuf__t.html</anchorfile>
      <anchor>a0f33a1d98be1357c1aa03020b0d2fdad</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>len</name>
      <anchorfile>structebisu__strbuf__t.html</anchorfile>
      <anchor>af8dac0456b018f8ca649265733b8bcdd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>overflow</name>
      <anchorfile>structebisu__strbuf__t.html</anchorfile>
      <anchor>a47b6dc470f254a39b91a70175c9b3c91</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_system_ops_t</name>
    <filename>structebisu__system__ops__t.html</filename>
    <member kind="variable">
      <type>ebisu_task_code_t(*</type>
      <name>task_create</name>
      <anchorfile>structebisu__system__ops__t.html</anchorfile>
      <anchor>acb93b9e260c324f9f727d1eacefc5c94</anchor>
      <arglist>)(ebisu_system_t *self, const char *name, EBISU_TASK_ENTRY entry, void *entry_param)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>delay_ms</name>
      <anchorfile>structebisu__system__ops__t.html</anchorfile>
      <anchor>a28038f19ac8ae6170f028a366571e505</anchor>
      <arglist>)(ebisu_system_t *self, uint32_t msec)</arglist>
    </member>
    <member kind="variable">
      <type>uint64_t(*</type>
      <name>monotonic_ms</name>
      <anchorfile>structebisu__system__ops__t.html</anchorfile>
      <anchor>a2a245518f6edc8e03312e7458e4fd730</anchor>
      <arglist>)(ebisu_system_t *self)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_system_t</name>
    <filename>structebisu__system__t.html</filename>
    <member kind="variable">
      <type>const ebisu_system_ops_t *</type>
      <name>ops</name>
      <anchorfile>structebisu__system__t.html</anchorfile>
      <anchor>a9710fe0c41ebd32f126d0d00b73fbb81</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="page">
    <name>index</name>
    <title>core</title>
    <filename>index.html</filename>
    <docanchor file="index.html" title="core">core_readme</docanchor>
  </compound>
</tagfile>
