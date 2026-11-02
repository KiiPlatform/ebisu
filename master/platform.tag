<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.9.8">
  <compound kind="file">
    <name>ebisu_clock.h</name>
    <path>include/</path>
    <filename>ebisu__clock_8h.html</filename>
    <includes id="ebisu__task_8h" name="ebisu_task.h" local="yes" import="no" module="no" objc="no">ebisu_task.h</includes>
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
      <anchor>a043704837c735ab8251b8cfd0b1f6fb1</anchor>
      <arglist>(ebisu_clock_t *clock, EBISU_CB_MONOTONIC_MS cb_monotonic_ms, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_clock_set_cb_lock</name>
      <anchorfile>ebisu__clock_8h.html</anchorfile>
      <anchor>a2ed02822fa8e33727d72b2197f584488</anchor>
      <arglist>(ebisu_clock_t *clock, EBISU_CB_LOCK cb_lock, EBISU_CB_LOCK cb_unlock, void *userdata)</arglist>
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
    <name>ebisu_socket.h</name>
    <path>include/</path>
    <filename>ebisu__socket_8h.html</filename>
    <member kind="typedef">
      <type>enum ebisu_sock_code_t</type>
      <name>ebisu_sock_code_t</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a2845d194b5ba641d5321831147cef126</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ebisu_sock_code_t(*</type>
      <name>EBISU_CB_SOCK_CONNECT</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a780e229bbc3fcf953655a2bfc62c38da</anchor>
      <arglist>)(void *sock_ctx, const char *host, uint16_t port)</arglist>
    </member>
    <member kind="typedef">
      <type>ebisu_sock_code_t(*</type>
      <name>EBISU_CB_SOCK_SEND</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>a6893a04d2aaab7f3c1c7538d5b91ca36</anchor>
      <arglist>)(void *sock_ctx, const char *buffer, size_t length, size_t *out_sent_length)</arglist>
    </member>
    <member kind="typedef">
      <type>ebisu_sock_code_t(*</type>
      <name>EBISU_CB_SOCK_RECV</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>acf0f4d76d750ddc101f50e0c1d578c9e</anchor>
      <arglist>)(void *sock_ctx, char *buffer, size_t length_to_read, size_t *out_actual_length)</arglist>
    </member>
    <member kind="typedef">
      <type>ebisu_sock_code_t(*</type>
      <name>EBISU_CB_SOCK_CLOSE</name>
      <anchorfile>ebisu__socket_8h.html</anchorfile>
      <anchor>adef9314ef39820eb6dba3c7aca22bbc4</anchor>
      <arglist>)(void *sock_ctx)</arglist>
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
  </compound>
  <compound kind="file">
    <name>ebisu_task.h</name>
    <path>include/</path>
    <filename>ebisu__task_8h.html</filename>
    <member kind="typedef">
      <type>enum ebisu_task_code_t</type>
      <name>ebisu_task_code_t</name>
      <anchorfile>ebisu__task_8h.html</anchorfile>
      <anchor>aaa3b1940a5a1fe2b64e28b21470d52ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void *(*</type>
      <name>EBISU_TASK_ENTRY</name>
      <anchorfile>ebisu__task_8h.html</anchorfile>
      <anchor>aa88036fe3014d1b2c19cd4808b5424d7</anchor>
      <arglist>)(void *value)</arglist>
    </member>
    <member kind="typedef">
      <type>ebisu_task_code_t(*</type>
      <name>EBISU_CB_TASK_CREATE</name>
      <anchorfile>ebisu__task_8h.html</anchorfile>
      <anchor>aef4e8f233b69f92311c3c78b3ac2efc2</anchor>
      <arglist>)(const char *name, EBISU_TASK_ENTRY entry, void *entry_param, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>EBISU_CB_DELAY_MS</name>
      <anchorfile>ebisu__task_8h.html</anchorfile>
      <anchor>a1a6065581c7d054ea6e6298c4a7f0f00</anchor>
      <arglist>)(uint32_t msec, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>EBISU_CB_LOCK</name>
      <anchorfile>ebisu__task_8h.html</anchorfile>
      <anchor>a2d0a87d7cb40c29ce98738310f630728</anchor>
      <arglist>)(void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>uint64_t(*</type>
      <name>EBISU_CB_MONOTONIC_MS</name>
      <anchorfile>ebisu__task_8h.html</anchorfile>
      <anchor>a1da5e106d6c8a76fbfa58819a424d915</anchor>
      <arglist>)(void *userdata)</arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ebisu_task_code_t</name>
      <anchorfile>ebisu__task_8h.html</anchorfile>
      <anchor>a90188fadee3bc027156aa75ab532454a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_clock_t</name>
    <filename>structebisu__clock__t.html</filename>
  </compound>
  <compound kind="page">
    <name>index</name>
    <title>platform</title>
    <filename>index.html</filename>
    <docanchor file="index.html" title="platform">platform_readme</docanchor>
  </compound>
</tagfile>
