<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.15.0">
  <compound kind="file">
    <name>ebisu_dns_probe.h</name>
    <path>dns_probe/include/</path>
    <filename>ebisu__dns__probe_8h.html</filename>
    <class kind="struct">ebisu_dns_server_t</class>
    <class kind="struct">ebisu_dns_probe_socket_t</class>
    <member kind="define">
      <type>#define</type>
      <name>EBISU_DNS_PROBE_DETAIL_SIZE</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a5674603bfeb9796aa2e38d94c7af80b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum ebisu_dns_probe_result_t</type>
      <name>ebisu_dns_probe_result_t</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a46187ce59f6e730e69c75fca926a8464</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_dns_server_t</type>
      <name>ebisu_dns_server_t</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>ab8200e51096daf747464484e49ab3a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_dns_probe_socket_t</type>
      <name>ebisu_dns_probe_socket_t</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a287377415d3178c2eceab38b9f6c7da6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ebisu_dns_probe_result_t</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a4acf05f6db48f7f1298d0d1a62cd13b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_DNS_PROBE_ANSWERED</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a4acf05f6db48f7f1298d0d1a62cd13b8ad82ff265013af3d97d2b7f0756c4f5ae</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_DNS_PROBE_NOT_NEEDED</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a4acf05f6db48f7f1298d0d1a62cd13b8a14b07e791558ba0f7c5d4ce737bbe3e4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>EBISU_DNS_PROBE_SILENT</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a4acf05f6db48f7f1298d0d1a62cd13b8a4a29066f4e9277a5d2d7a053dc8dc696</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>ebisu_dns_probe_result_t</type>
      <name>ebisu_dns_probe</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a1cec13c6ea37fdb1b5542fe8d4de79d1</anchor>
      <arglist>(const char *host, uint32_t timeout_ms, char *detail, size_t detail_size)</arglist>
    </member>
    <member kind="function">
      <type>ebisu_dns_probe_result_t</type>
      <name>ebisu_dns_probe_servers</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>ad3eb9b045566c64e1bcad35e00a245db</anchor>
      <arglist>(const char *host, const ebisu_dns_server_t *servers, size_t count, uint32_t timeout_ms, char *detail, size_t detail_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_dns_probe_socket_init</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a454033f6e9af7231756955b51229306e</anchor>
      <arglist>(ebisu_dns_probe_socket_t *sock, ebisu_socket_t *inner, uint32_t timeout_ms)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_dns_probe_socket_set_servers</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a0bdb2f4e19c931056900ef54071f5ab3</anchor>
      <arglist>(ebisu_dns_probe_socket_t *sock, const ebisu_dns_server_t *servers, size_t count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_dns_probe_socket_set_logger</name>
      <anchorfile>ebisu__dns__probe_8h.html</anchorfile>
      <anchor>a291c2266fc60fe785a8899bc070d9989</anchor>
      <arglist>(ebisu_dns_probe_socket_t *sock, ebisu_logger_t *logger)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_openssl.h</name>
    <path>openssl/include/</path>
    <filename>ebisu__openssl_8h.html</filename>
    <class kind="struct">ebisu_openssl_socket_t</class>
    <class kind="struct">ebisu_openssl_tls_t</class>
    <member kind="typedef">
      <type>struct ebisu_openssl_socket_t</type>
      <name>ebisu_openssl_socket_t</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>a4dc56053c784610a590ee3fceb160913</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct ebisu_openssl_tls_t</type>
      <name>ebisu_openssl_tls_t</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>a0b259235952ff790e57847975a85bf3f</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ebisu_openssl_tls_init</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>a0cd7e01753d8d4c89fd6e4fb791ef517</anchor>
      <arglist>(ebisu_openssl_tls_t *tls, const char *ca_file, const char *ca_path)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_openssl_tls_free</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>af15941ca02f6af688cce18ee10115528</anchor>
      <arglist>(ebisu_openssl_tls_t *tls)</arglist>
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
      <name>ebisu_openssl_socket_set_connect_timeout</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>a360ec4c1f29d9ab30a4b41aaeedb560d</anchor>
      <arglist>(ebisu_openssl_socket_t *sock, unsigned int seconds)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_openssl_socket_set_tls</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>afaa27f5cdf52db44f1a2d3532e901cb1</anchor>
      <arglist>(ebisu_openssl_socket_t *sock, ebisu_openssl_tls_t *tls)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_openssl_socket_set_clock</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>ad504ec9246a218a4cd22974b77da48f7</anchor>
      <arglist>(ebisu_openssl_socket_t *sock, ebisu_clock_t *clock)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_openssl_socket_set_logger</name>
      <anchorfile>ebisu__openssl_8h.html</anchorfile>
      <anchor>a85279e489e808b094e800a9f6d544878</anchor>
      <arglist>(ebisu_openssl_socket_t *sock, ebisu_logger_t *logger)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_posix.h</name>
    <path>posix/include/</path>
    <filename>ebisu__posix_8h.html</filename>
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
  <compound kind="file">
    <name>ebisu_schannel.h</name>
    <path>schannel/include/</path>
    <filename>ebisu__schannel_8h.html</filename>
    <class kind="struct">ebisu_schannel_socket_t</class>
    <member kind="typedef">
      <type>struct ebisu_schannel_socket_t</type>
      <name>ebisu_schannel_socket_t</name>
      <anchorfile>ebisu__schannel_8h.html</anchorfile>
      <anchor>a843245dd145c7210d297a099d38cdb59</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_schannel_socket_init</name>
      <anchorfile>ebisu__schannel_8h.html</anchorfile>
      <anchor>af2bab7e538e399c8ac8c049326302e4c</anchor>
      <arglist>(ebisu_schannel_socket_t *sock, unsigned int to_recv, unsigned int to_send)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_schannel_socket_set_clock</name>
      <anchorfile>ebisu__schannel_8h.html</anchorfile>
      <anchor>abc316edc7cfd99ae6f132654e184b24e</anchor>
      <arglist>(ebisu_schannel_socket_t *sock, ebisu_clock_t *clock)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_schannel_socket_set_logger</name>
      <anchorfile>ebisu__schannel_8h.html</anchorfile>
      <anchor>aa3b8c7de20857678edb3db44fdee1c3c</anchor>
      <arglist>(ebisu_schannel_socket_t *sock, ebisu_logger_t *logger)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ebisu_win32.h</name>
    <path>win32/include/</path>
    <filename>ebisu__win32_8h.html</filename>
    <class kind="struct">ebisu_win32_lock_t</class>
    <member kind="typedef">
      <type>struct ebisu_win32_lock_t</type>
      <name>ebisu_win32_lock_t</name>
      <anchorfile>ebisu__win32_8h.html</anchorfile>
      <anchor>a7c637645697f264f1b8c632579c1267b</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>ebisu_system_t *</type>
      <name>ebisu_win32_system</name>
      <anchorfile>ebisu__win32_8h.html</anchorfile>
      <anchor>a8d84f62ef6dfdfc190453a04cb3b2045</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ebisu_win32_lock_init</name>
      <anchorfile>ebisu__win32_8h.html</anchorfile>
      <anchor>aa77cb4447ff9ac43260fc0622ba4b6fd</anchor>
      <arglist>(ebisu_win32_lock_t *lock)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_dns_probe_socket_t</name>
    <filename>structebisu__dns__probe__socket__t.html</filename>
    <member kind="variable">
      <type>ebisu_socket_t</type>
      <name>base</name>
      <anchorfile>structebisu__dns__probe__socket__t.html</anchorfile>
      <anchor>a4b59711f8e98c0aaa5f53a61c485aeac</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_dns_server_t</name>
    <filename>structebisu__dns__server__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>address</name>
      <anchorfile>structebisu__dns__server__t.html</anchorfile>
      <anchor>a5c057c86f7efef4f0ced2a837f6394ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint16_t</type>
      <name>port</name>
      <anchorfile>structebisu__dns__server__t.html</anchorfile>
      <anchor>a1933ddc8051925af2248dc370517f88a</anchor>
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
    <name>ebisu_openssl_tls_t</name>
    <filename>structebisu__openssl__tls__t.html</filename>
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
    <name>ebisu_schannel_socket_t</name>
    <filename>structebisu__schannel__socket__t.html</filename>
    <member kind="variable">
      <type>ebisu_socket_t</type>
      <name>base</name>
      <anchorfile>structebisu__schannel__socket__t.html</anchorfile>
      <anchor>a06ab23d1096e9ab16290eb3c5bafbe9e</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ebisu_win32_lock_t</name>
    <filename>structebisu__win32__lock__t.html</filename>
    <member kind="variable">
      <type>ebisu_lock_t</type>
      <name>base</name>
      <anchorfile>structebisu__win32__lock__t.html</anchorfile>
      <anchor>aa14f0b279bf832d647bc9a080cb2d897</anchor>
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
