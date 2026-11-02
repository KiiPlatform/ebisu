<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.9.8">
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
    <name>ebisu_openssl.h</name>
    <path>openssl/include/</path>
    <filename>ebisu__openssl_8h.html</filename>
    <includes id="ebisu__clock_8h" name="ebisu_clock.h" local="no" import="no" module="no" objc="no">ebisu_clock.h</includes>
    <includes id="ebisu__socket_8h" name="ebisu_socket.h" local="no" import="no" module="no" objc="no">ebisu_socket.h</includes>
    <class kind="struct">ebisu_openssl_socket_t</class>
    <member kind="typedef">
      <type>struct ebisu_openssl_socket_t</type>
      <name>ebisu_openssl_socket_t</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>a4dc56053c784610a590ee3fceb160913</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_openssl_socket_init</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>adbc237cc5b52c24a208886300ee0a8a2</anchor>
      <arglist>(ebisu_openssl_socket_t *sock, unsigned int to_recv, unsigned int to_send)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_openssl_socket_set_clock</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>ad504ec9246a218a4cd22974b77da48f7</anchor>
      <arglist>(ebisu_openssl_socket_t *sock, ebisu_clock_t *clock)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_posix.h</name>
    <path>posix/include/</path>
    <filename>ebisu__posix_8h.html</filename>
    <includes id="ebisu__system_8h" name="ebisu_system.h" local="no" import="no" module="no" objc="no">ebisu_system.h</includes>
    <class kind="struct">ebisu_posix_lock_t</class>
    <member kind="typedef">
      <type>struct ebisu_posix_lock_t</type>
      <name>ebisu_posix_lock_t</name>
      <anchorfile>ebisu__posix_8h.html</anchorfile>
      <anchor>a22d27d681115334fe2e1410cbb2b9226</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>ebisu_system_t *</type>
      <name>ebisu_posix_system</name>
      <anchorfile>ebisu__posix_8h.html</anchorfile>
      <anchor>a1d2bbb9e6c2952242e1c6738ad24f8d4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>ebisu_posix_lock_init</name>
      <anchorfile>ebisu__posix_8h.html</anchorfile>
      <anchor>a1c771ebce9cfb180f497092f6b8b5059</anchor>
      <arglist>(ebisu_posix_lock_t *lock)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_posix_lock_destroy</name>
      <anchorfile>ebisu__posix_8h.html</anchorfile>
      <anchor>a48ff6977e6c447af50911fefb115690d</anchor>
      <arglist>(ebisu_posix_lock_t *lock)</arglist>
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
    <name>ebisu_openssl_socket_t</name>
    <filename>structebisu__openssl__socket__t.html</filename>
    <member kind="variable">
      <type>ebisu_socket_t</type>
      <name>base</name>
      <anchorfile>structebisu__openssl__socket__t.html</anchorfile>
      <anchor>a0525a6b30d917271238a685b221b9739</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_posix_lock_t</name>
    <filename>structebisu__posix__lock__t.html</filename>
    <member kind="variable">
      <type>ebisu_lock_t</type>
      <name>base</name>
      <anchorfile>structebisu__posix__lock__t.html</anchorfile>
      <anchor>a77630c857724e99e3bd71960e6101b13</anchor>
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
    <title>platform</title>
    <filename>index.html</filename>
    <docanchor file="index.html" title="platform">platform_readme</docanchor>
  </compound>
</tagfile>
